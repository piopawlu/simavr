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

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_gdb.h"
#include "avr_usi.h"

avr_t * avr = NULL;
uint8_t	pin_state = 0;	// current port B

#define USI_SCK_BIT 7
#define USI_DO_BIT 6
#define USI_DI_BIT 5
#define NRF24L01_CS 4

/*
 * called when the AVR change any of the pins on port B
 * so lets update our buffer
 */
void pin_changed_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	pin_state = (pin_state & ~(1 << irq->irq)) | (value << irq->irq);

	printf("PINB=%02x\n", value & 0x0FF);
}

void usi_dr_change_hook(struct avr_irq_t * irq, uint32_t value, void * param)
{
	printf("USIDR=%02x\n", value & 0x0FF);
}

static void * avr_run_thread(void * param)
{
	uint8_t* working = (uint8_t*)(param);

	while (*working) {
		avr_run(avr);
	}
	return NULL;
}

int main(int argc, char *argv[])
{
	elf_firmware_t f;

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

	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_SCK_BIT), pin_changed_hook, NULL);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_DO_BIT), pin_changed_hook, NULL);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), USI_DI_BIT), pin_changed_hook, NULL);
	avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), NRF24L01_CS), pin_changed_hook, NULL);

	// avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_USI_GETIRQ(), )

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (1) {
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	pthread_t run;
	uint8_t working = 1;
	pthread_create(&run, NULL, avr_run_thread, &working);

	while( fgetc(stdin) != 'q' ) {
		puts("Press 'q' to quit");
	}

	working = 0;

	pthread_join(run, NULL);
	return 0;
}
