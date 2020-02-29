/*
	bme280_usi.c
	
	Copyright 2020 Piotr Gertz <piotrek@piopawlu.net>

 	This file is part of simavr.

	simavr is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	simavr is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with simavr.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <libgen.h>
#include <pthread.h>
#include <string.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "avr_usi.h"

#define USI_SCK_BIT 7
#define USI_DO_BIT 6
#define USI_DI_BIT 5

#define NRF24L01_CS 3
#define NRF24L01_CE 2

#define BME280_CS 5
#define BME280_POWER 6

#define TEXT_CS 1

struct thread_param {
    avr_t* avr;
    int working;
};

enum spi_mode {
    SPI_WRITE=0,
    SPI_READ,
    SPI_IGNORE
};

struct spi_device {
    uint8_t r_addr;
    uint8_t w_addr;
    uint8_t addr_set;
    uint8_t status_read;
    enum spi_mode spi_mode;
    uint8_t data[256];
};

struct spi_device* current_spi_device = NULL;
struct spi_device spi_uart;
struct spi_device spi_bme280;
struct spi_device spi_nrf24l01;


int text_out_enabled = 0;

void pin_changed_hook_nrf24l01_cs(struct avr_irq_t * irq, uint32_t value, void * param)
{
	printf("PINB%d=%d\n", irq->irq, value);

    if( (value << irq->irq) & (1 << NRF24L01_CS) ) {
         current_spi_device = NULL;
    } else {
         current_spi_device = &spi_nrf24l01;
         current_spi_device->addr_set = 0;
         current_spi_device->status_read = 0;
    }
}

void pin_changed_hook_bme280_cs(struct avr_irq_t * irq, uint32_t value, void * param)
{
    printf("PIND%d=%d\n", irq->irq, value);

    if( (value << irq->irq) & (1 << BME280_CS) ) {
        current_spi_device = NULL;
    } else {
        current_spi_device = &spi_bme280;
        current_spi_device->addr_set = 0;
        current_spi_device->status_read = 0;
    }
}

void pin_changed_hook_text_cs(struct avr_irq_t * irq, uint32_t value, void * param)
{
    if( (value << irq->irq) & (1 << TEXT_CS) ) {
        text_out_enabled = 0;
        current_spi_device = NULL;
    } else {
        text_out_enabled = 1;
        current_spi_device = &spi_uart;
        current_spi_device->addr_set = 1;
        current_spi_device->status_read = 1;
    }
}

void usi_dr_change_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	printf("USIDR=%02x\n", value & 0x0FF);
}

void usi_data_written(struct avr_irq_t * irq, uint32_t value, void * param)
{


}

void usi_data_write(struct avr_t * avr, avr_io_addr_t addr, uint8_t v, void * param)
{
    if( current_spi_device == &spi_uart ) {
        if( text_out_enabled ) {
            putchar(avr->data[0x2f]);
        }
    } else if( current_spi_device == &spi_nrf24l01 ) {

        if( current_spi_device->addr_set == 0 ) {

            if( v < 0b00100000 ) {
                current_spi_device->spi_mode = SPI_READ;
                current_spi_device->r_addr = v;
            } else if( v < 0b01000000 ) {
                current_spi_device->spi_mode = SPI_WRITE;
                current_spi_device->w_addr = v & 0b00011111;
            } else {
                current_spi_device->spi_mode = SPI_IGNORE;
            }

            current_spi_device->addr_set = 1;
        } else if( current_spi_device->spi_mode == SPI_WRITE ) {
           current_spi_device->data[current_spi_device->w_addr++] = v;
        }


    } else if( current_spi_device == &spi_bme280 ) {

       if( current_spi_device->addr_set == 0 ) {

           if( v & 0b10000000 ) {
               current_spi_device->spi_mode = SPI_READ;
               current_spi_device->r_addr = v;
               printf("BME280:R:0x%02x\n", current_spi_device->r_addr);
           } else {
                current_spi_device->spi_mode = SPI_WRITE;
                current_spi_device->w_addr = v | 0b10000000;
                printf("BME280:W:0x%02x\n", current_spi_device->w_addr);
           }

           current_spi_device->addr_set = 1;
       } else if( current_spi_device->spi_mode == SPI_WRITE ) {
            current_spi_device->data[current_spi_device->w_addr++] = v;
       }

    }
}

uint8_t usi_data_read(struct avr_t * avr, avr_io_addr_t addr, void * param)
{
    if( current_spi_device && current_spi_device->addr_set == 1 && current_spi_device->spi_mode == SPI_READ ) {

        if( current_spi_device->status_read == 0 ) {
            current_spi_device->status_read = 1;
            avr->data[addr] = 0x00;
        } else {
            avr->data[addr] = current_spi_device->data[current_spi_device->r_addr++];
        }

    }

    return avr->data[addr];
}

static void * avr_run_thread(void * param)
{
	struct thread_param* wp = (struct thread_param*)(param);

	while (wp->working) {
		avr_run(wp->avr);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	elf_firmware_t f;
    avr_t * avr = NULL;

	if (argc != 4) {
		fprintf(stderr, "%s firmware.elf attiny4313 500000\n", argv[0]);
		return -1;
	}

	printf("Firmware pathname is %s\n", argv[1]);
	elf_read_firmware(argv[1], &f);

	printf("firmware %s f=%d mmcu=%s\n", argv[1], (int)atoi(argv[3]), argv[2]);

	avr = avr_make_mcu_by_name(argv[2]);
	if (!avr) {
		fprintf(stderr, "%s: AVR '%s' not known\n", argv[0], argv[2]);
		exit(1);
	}
	avr_init(avr);
	avr_load_firmware(avr, &f);
    avr->trace = 0;

	//avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_SCK_BIT), pin_changed_hook, NULL);
	//avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_DO_BIT), pin_changed_hook, NULL);
	//avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_DI_BIT), pin_changed_hook, NULL);
	
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), NRF24L01_CS), pin_changed_hook_nrf24l01_cs, NULL);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), BME280_CS), pin_changed_hook_bme280_cs, NULL);
    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), TEXT_CS), pin_changed_hook_text_cs, NULL);

    avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_USI_GETIRQ(), 0), usi_data_written, avr);


    avr_register_io_read(avr, 0x2F, usi_data_read, avr);
    avr_register_io_write(avr, 0x2F, usi_data_write, avr);

    memcpy(&spi_bme280.data[0x88], "\x80\x6F\x77\x68\x32\x00\xDA\x93\xFE\xD6\xD0\x0B\x8B\x25\x1C\xFF\xF9\xFF\xAC\x26\x0A\xD8\xBD\x10\x00\x4B", 26);
    memcpy(&spi_bme280.data[0xE1], "\x78\x01\x00\x11\x2F\x03\x1E",7);
    memcpy(&spi_bme280.data[0xF7], "\x48\x0A\x00\x80\x2D\x80\x58\x31", 8);


    spi_bme280.data[0xD0] = 0x60;
    spi_bme280.data[0xF3] = 0x00;
    

	// avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_USI_GETIRQ(), )

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
    avr->frequency = (int)atoi(argv[3]);
	if (0) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	pthread_t run;
	struct thread_param tp = { avr, 1};
	pthread_create(&run, NULL, avr_run_thread, &tp);

	while( fgetc(stdin) != 'q' ) {
		puts("Press 'q' to quit");
	}

	tp.working = 0;

	pthread_join(run, NULL);
	return 0;
}
