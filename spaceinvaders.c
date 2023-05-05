
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

static void usage(void);
static struct spaceinvaders *spaceinvaders_create(void);
static void spaceinvaders_destroy(struct spaceinvaders *);
static int spaceinvaders_load_file(struct spaceinvaders *, const char *);
static uint8_t spaceinvaders_read_byte(void *, uint16_t);
static void spaceinvaders_write_byte(void *, uint16_t, uint8_t);
static uint8_t spaceinvaders_io_inb(void *, uint8_t);
static void spaceinvaders_io_outb(void *, uint8_t, uint8_t);

int
main(int argc, char **argv)
{
	struct spaceinvaders *emu;
	struct i8080 *cpu;
	if (argc != 2)
		usage();
	emu = spaceinvaders_create();
	if (emu == NULL)
		goto fail;

	cpu = &emu->cpu;
	if (spaceinvaders_load_file(emu, argv[1]) < 0)
		goto fail;

	spaceinvaders_destroy(emu);
	return 0;
fail:
	if (emu != NULL)
		spaceinvaders_destroy(emu);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "spaceinvaders file\n");
	exit(1);
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
	emu->inp0 = 0;
	emu->inp1 = 0;
	emu->inp2 = 0;
	emu->shift0 = 0;
	emu->shift1 = 0;
	emu->shift_offset = 0;
	return emu;
}

static void
spaceinvaders_destroy(struct spaceinvaders *emu)
{
	if (emu == NULL)
		return;
	free(emu->memory);
	emu->memory = NULL;
	free(emu);
}

static int
spaceinvaders_load_file(struct spaceinvaders *emu, const char *file)
{
	struct stat st;
	FILE *fp;
	uint8_t *mem;

	if (stat(file, &st) < 0) {
		fprintf(stderr, "%s: %s.\n", file, strerror(errno));
		goto err0;
	}

	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "%s: Not a regular file.\n", file);
		goto err0;
	}

	if (st.st_size < 0 || st.st_size > 0x4000) {
		fprintf(stderr, "%s: Invalid file size. Input the invaders "
				"image combined.\n", file);
		goto err0;
	}

	fp = fopen(file, "rb");
	if (fp == NULL) {
		fprintf(stderr, "%s: %s.\n", file, strerror(errno));
		goto err0;
	}

	mem = calloc(1, 0x4000);
	if (mem == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		goto err1;
	}

	if (fread(mem, st.st_size, 1, fp) != 1) {
		fprintf(stderr, "%s: Failed to load file.\n", file);
		goto err2;
	}

	emu->memory = mem;
	emu->memory_size = 0x10000;
	fclose(fp);
	return 0;
err2:
	free(mem);
err1:
	fclose(fp);
err0:
	return -1;
}

static uint8_t
spaceinvaders_read_byte(void *emuptr, uint16_t address)
{
	struct spaceinvaders *emu = emuptr;

	if (emu->memory_size <= address || 0x6000 <= address)
		return 0;
	/* Mirror */
	if (address >= 0x4000)
		return emu->memory[address - 0x2000];
	return emu->memory[address];
}

static void
spaceinvaders_write_byte(void *emuptr, uint16_t address, uint8_t val)
{
	struct spaceinvaders *emu = emuptr;

	if (emu->memory_size <= address)
		return;
	/* Read only */
	if (address < 0x2000)
		return;
	if (address > 0x4000)
		return;
	emu->memory[address] = val;
}

static uint8_t
spaceinvaders_io_inb(void *emuptr, uint8_t port)
{
	struct spaceinvaders *emu = emuptr;
	uint8_t val;
	uint16_t s;

	switch (val) {
		case 0x00: /* Unused ? */
			val = emu->inp0;
			break;
		case 0x01: /* Input 1 */
			val = emu->inp1;
			break;
		case 0x02: /* Input 2 */
			val = emu->inp2;
			break;
		case 0x03: /* Shift register */
			s = ((uint16_t)emu->shift1 << 8) |
				((uint16_t)emu->shift0);
			val = (s >> (8 - emu->shift_offset)) & 0xff;
			break;
		default: /* Invalid port */
			val = 0;
			break;
	}

	return val;
}

static void
spaceinvaders_io_outb(void *emuptr, uint8_t port, uint8_t val)
{
	struct spaceinvaders *emu = emuptr;
	printf("out\n");

	switch (port) {
		case 0x02: /* Shift amount (3 bits) */
			emu->shift_offset & 0x07;
			break;
		case 0x03: /* Sound bits */
			break;
		case 0x04: /* Shift data */
			emu->shift0 = emu->shift1;
			emu->shift1 = val;
			break;
		case 0x05: /* Sound bits */
			break;
		case 0x06: /* Watch dog */
			break;
	}
}

