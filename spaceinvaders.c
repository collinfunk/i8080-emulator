
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "i8080.h"

/*
 * 
 * 0000 - 1fff rom
 * 2000 - 23ff ram
 * 2400 - 3fff video RAM
 * 4000 - ram mirror
 */
#define SI_MEMORY_SIZE 0x4000
#define SI_SCREEN_WIDTH 256
#define SI_SCREEN_HEIGHT 224

struct spaceinvaders {
	struct i8080 cpu;
	uint8_t *memory;
	size_t memory_size;
	uint8_t inp0; /* Unused? */
	uint8_t inp1;
	uint8_t inp2;
	uint8_t shift0;
	uint8_t shift1;
	uint8_t shift_offset;
};

static struct spaceinvaders *spaceinvaders_create(void);
static void spaceinvaders_destroy(struct spaceinvaders *);
static uint8_t spaceinvaders_read_byte(void *, uint16_t);
static void spaceinvaders_write_byte(void *, uint16_t, uint8_t);
static uint8_t spaceinvaders_io_inb(void *, uint8_t);
static void spaceinvaders_io_outb(void *, uint8_t, uint8_t);

int
main(int argc, char **argv)
{
	return 0;
}

static struct spaceinvaders *
spaceinvaders_create(void)
{
	struct spaceinvaders *emu;
	struct i8080 *cpu;

	emu = calloc(1, sizeof(*emu));
	if (emu == NULL)
		return NULL;
	cpu = &emu->cpu;
	i8080_init(cpu);
	i8080_init(cpu);
	cpu->opaque = emu;
	cpu->read_byte = spaceinvaders_read_byte;
	cpu->write_byte = spaceinvaders_write_byte;
	cpu->io_inb = spaceinvaders_io_inb;
	cpu->io_outb = spaceinvaders_io_outb;
	emu->memory = NULL;
	emu->memory_size = 0;
	return emu;
}

static void
spaceinvaders_destroy(struct spaceinvaders *);

static uint8_t
spaceinvaders_read_byte(void *emuptr, uint16_t address)
{
	return 0;
}

static void
spaceinvaders_write_byte(void *emuptr, uint16_t address, uint8_t val)
{

}

static uint8_t
spaceinvaders_io_inb(void *emuptr, uint8_t port)
{
	return 0;
}

static void
spaceinvaders_io_outb(void *emuptr, uint8_t port, uint8_t val)
{

}

