/*
	simduino.c

	Copyright 2008, 2009 Michel Pollet <buserror@gmail.com>

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

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libgen.h>
#include <signal.h>

#include <pthread.h>

#include "sim_avr.h"
#include "avr_ioport.h"
#include "sim_elf.h"
#include "sim_hex.h"
#include "sim_gdb.h"
#include "uart_pty.h"
#include "sim_vcd_file.h"

uart_pty_t uart_pty;
avr_t *avr = NULL;
avr_vcd_t vcd_file;

struct avr_flash
{
	char avr_flash_path[1024];
	int avr_flash_fd;
};

volatile int stop_simulation = 0;

// avr special flash initalization
// here: open and map a file to enable a persistent storage for the flash memory
void avr_special_init(avr_t *avr, void *data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	// open the file
	flash_data->avr_flash_fd = open(flash_data->avr_flash_path,
									O_RDWR | O_CREAT, 0644);
	if (flash_data->avr_flash_fd < 0)
	{
		perror(flash_data->avr_flash_path);
		exit(1);
	}
	// resize and map the file the file
	(void)ftruncate(flash_data->avr_flash_fd, avr->flashend + 1);
	ssize_t r = read(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1)
	{
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
		exit(1);
	}
}

// avr special flash deinitalization
// here: cleanup the persistent storage
void avr_special_deinit(avr_t *avr, void *data)
{
	struct avr_flash *flash_data = (struct avr_flash *)data;

	printf("%s\n", __func__);
	lseek(flash_data->avr_flash_fd, SEEK_SET, 0);
	ssize_t r = write(flash_data->avr_flash_fd, avr->flash, avr->flashend + 1);
	if (r != avr->flashend + 1)
	{
		fprintf(stderr, "unable to load flash memory\n");
		perror(flash_data->avr_flash_path);
	}
	close(flash_data->avr_flash_fd);
	uart_pty_stop(&uart_pty);
}

// Signal handler for SIGINT (Ctrl+C)
void sig_int(int sign)
{
	if (!stop_simulation)
	{
		printf("Signal caught, simavr terminating\n");
		stop_simulation = 1;
	}
	else
	{
		printf("Signal caught again, ignoring...\n");
	}
}

int main(int argc, char *argv[])
{
	signal(SIGINT, sig_int);

	struct avr_flash flash_data;
	char boot_path[1024] = "ATmegaBOOT_168_atmega328.ihex";
	uint32_t boot_base, boot_size;
	char *mmcu = "atmega328p";
	uint32_t freq = 16000000;
	int debug = 0;
	int verbose = 0;

	for (int i = 1; i < argc; i++)
	{
		if (!strcmp(argv[i] + strlen(argv[i]) - 4, ".hex"))
			strncpy(boot_path, argv[i], sizeof(boot_path));
		else if (!strcmp(argv[i], "-d"))
			debug++;
		else if (!strcmp(argv[i], "-v"))
			verbose++;
		else
		{
			fprintf(stderr, "%s: invalid argument %s\n", argv[0], argv[i]);
			exit(1);
		}
	}

	uint8_t *boot = read_ihex_file(boot_path, &boot_size, &boot_base);
	if (!boot)
	{
		fprintf(stderr, "%s: Unable to load %s\n", argv[0], boot_path);
		exit(1);
	}
	if (boot_base > 32 * 1024 * 1024)
	{
		mmcu = "atmega2560";
		freq = 20000000;
	}
	printf("%s bootloader 0x%05x: %d bytes\n", mmcu, boot_base, boot_size);

	avr = avr_make_mcu_by_name(mmcu);
	if (!avr)
	{
		fprintf(stderr, "%s: Error creating the AVR core\n", argv[0]);
		exit(1);
	}

	snprintf(flash_data.avr_flash_path, sizeof(flash_data.avr_flash_path),
			 "simduino_%s_flash.bin", mmcu);
	flash_data.avr_flash_fd = 0;
	// register our own functions
	avr->custom.init = avr_special_init;
	avr->custom.deinit = avr_special_deinit;
	avr->custom.data = &flash_data;
	avr_init(avr);
	avr->frequency = freq;

	memcpy(avr->flash + boot_base, boot, boot_size);
	free(boot);
	avr->pc = boot_base;
	/* end of flash, remember we are writing /code/ */
	avr->codeend = avr->flashend;
	avr->log = 1 + verbose;

	// Initialize VCD file for tracking
	// 	avr_vcd_t vcd_file;
	// 	avr_vcd_init(avr, "output.vcd", &vcd_file, 10); // 10ms period

	// 	// Register signals for tracking
	// 	avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), 5), 1, "PB5");		// Pin B5 (LED)
	// 	avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('D'), 1), 1, "TX_PD1");	// Pin D1 (TX)
	// avr_vcd_add_signal(&vcd_file, avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('U'), 0), 8, "UART_UDR");

	// 	avr_vcd_start(&vcd_file);

	// even if not setup at startup, activate gdb if crashing
	avr->gdb_port = 1234;
	if (debug)
	{
		avr->state = cpu_Stopped;
		avr_gdb_init(avr);
	}

	uart_pty_init(avr, &uart_pty);
	uart_pty_connect(&uart_pty, '0');

	while (!stop_simulation)
	{
		int state = avr_run(avr);
		if (state == cpu_Done || state == cpu_Crashed)
			break;
	}
	avr_terminate(avr);

	return 0;

	// avr_vcd_stop(&vcd_file);
	// avr_vcd_close(&vcd_file);
}
