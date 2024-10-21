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
#include "avr_uart.h"

// Define a function pointer type for the callback
typedef void (*PinChangeCallback)(uint8_t port, uint8_t pin, uint8_t value);

// Declare a static variable to hold the callback function pointer
static PinChangeCallback pin_change_callback = NULL;

// Extern functions for external use (e.g., from C#)
extern void avr_init_simulation();
extern void avr_run_simulation();
extern void set_pin_change_callback(PinChangeCallback callback);
extern void register_pin_change(char port_char, uint8_t pin);
extern void avr_stop_simulation();

avr_t *avr = NULL;
uart_pty_t uart_pty;
volatile int stop_simulation = 0;

struct avr_flash
{
    char avr_flash_path[1024];
    int avr_flash_fd;
};

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

// AVR initialization
extern void avr_init_simulation()
{
    struct avr_flash flash_data;
    char boot_path[1024] = "/home/lonewolf/Documents/Godot/Game/Lib/ATmegaBOOT_168_atmega328.ihex";
    uint32_t boot_base, boot_size;
    char *mmcu = "atmega328p";
    uint32_t freq = 16000000;
    int verbose = 0;

    uint8_t *boot = read_ihex_file(boot_path, &boot_size, &boot_base);
    if (!boot)
    {
        fprintf(stderr, "Unable to load %s\n", boot_path);
        exit(1);
    }

    printf("%s bootloader 0x%05x: %d bytes\n", mmcu, boot_base, boot_size);

    avr = avr_make_mcu_by_name(mmcu);
    if (!avr)
    {
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

    uart_pty_init(avr, &uart_pty);
    uart_pty_connect(&uart_pty, '0');
}

// Run simulation
extern void avr_run_simulation()
{
    while (!stop_simulation)
    {
        int state = avr_run(avr);
        if (state == cpu_Done || state == cpu_Crashed)
            break;
    }
    avr_terminate(avr);
}

// Function to stop the simulation
extern void avr_stop_simulation()
{
    stop_simulation = 1;
}

int8_t port_e = 0;

// Pin change hook function
void pin_changed_hook_e(struct avr_irq_t *irq, uint32_t value, void *param)
{
    if (pin_change_callback != NULL)
    {
        uint8_t *port_pin = (uint8_t *)param;
        uint8_t port_char = port_pin[0];
        uint8_t pin = port_pin[1];
        // Call the callback function provided by C#
        pin_change_callback(port_char, pin, (uint8_t)value);
    }
}

// Function to set the callback from C#
extern void set_pin_change_callback(PinChangeCallback callback)
{
    pin_change_callback = callback;
}

// Get the value of a specific pin (e.g., Arduino pin 13 -> PB5)
extern void register_pin_change(char port, uint8_t pin)
{
    switch (port)
    {
    case 'B':
        uint8_t *param = malloc(2);
        param[0] = (uint8_t)port;
        param[1] = pin;
        avr_irq_register_notify(avr_io_getirq(avr, AVR_IOCTL_IOPORT_GETIRQ('B'), pin), pin_changed_hook_e, param);
        // Create a parameter struct to pass port and pin to the callback
        break;
    default:
        fprintf(stderr, "Invalid port %c\n", port);
    }
}
