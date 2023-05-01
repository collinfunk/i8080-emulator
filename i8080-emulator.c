
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "i8080.h"

struct emulator {
	struct i8080 cpu;
	uint8_t *memory;
	size_t memory_size;
};

static void usage(void);
static struct emulator *emulator_create(void);
static void emulator_destroy(struct emulator *);
static int emulator_load_file(struct emulator *, const char *, uint16_t);
static uint8_t emulator_read_byte(void *, uint16_t);
static void emulator_write_byte(void *, uint16_t, uint8_t);
static uint8_t emulator_io_outb(void *, uint8_t);
static void emulator_io_inb(void *, uint8_t, uint8_t);

int
main(int argc, char **argv)
{
	struct emulator *emu;
	struct i8080 *cpu;

	if (argc != 2)
		usage();
	emu = emulator_create();
	if (emu == NULL) {
		fprintf(stderr, "Failed to allocate memory.\n");
		goto fail;
	}

	cpu = &emu->cpu;

	if (emulator_load_file(emu, argv[1], 0x100) < 0)
		goto fail;


	emulator_destroy(emu);
	return 0;
fail:
	if (emu != NULL)
		emulator_destroy(emu);
	exit(1);
}

static void
usage(void)
{
	fprintf(stderr, "i8080-emulator file\n");
	exit(1);
}

static struct emulator *
emulator_create(void)
{
	struct emulator *emu;
	struct i8080 *cpu;

	emu = calloc(1, sizeof(*emu));
	if (emu == NULL)
		return NULL;
	cpu = &emu->cpu;
	i8080_init(cpu);
	cpu->opaque = emu;
	cpu->read_byte = emulator_read_byte;
	cpu->write_byte = emulator_write_byte;
	cpu->io_inb = emulator_io_outb;
	cpu->io_outb = emulator_io_inb;
	emu->memory = NULL;
	emu->memory_size = 0;
	return emu;
}

static void
emulator_destroy(struct emulator *emu)
{
	if (emu == NULL)
		return;
	free(emu->memory);
	emu->memory = NULL;
	free(emu);
	emu = NULL;
}

static int
emulator_load_file(struct emulator *emu, const char *name, uint16_t offset)
{
	struct stat st;
	FILE *fp;
	uint8_t *mem;

	if (stat(name, &st) < 0) {
		fprintf(stderr, "%s: %s.\n", name, strerror(errno));
		goto err0;
	}

	if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "%s: Not a regular file.\n", name);
		goto err0;
	}

	if (st.st_size <= 0 || st.st_size > UINT16_MAX) {
		fprintf(stderr, "%s: Invalid file size (%jd bytes).\n",
				name, (intmax_t)st.st_size);
		goto err0;
	}

	if (st.st_size + offset > UINT16_MAX) {
		fprintf(stderr, "%s: Offset too large to address file.\n");
		goto err0;
	}

	fp = fopen(name, "rb");
	if (fp == NULL) {
		fprintf(stderr, "%s: %s.\n", name, strerror(errno));
		goto err0;
	}

	mem = calloc(1, 0x10000);
	if (mem == NULL) {
		fprintf(stderr, "Memory allocation failed.\n");
		goto err1;
	}

	if (fread(&mem[offset], st.st_size, 1, fp) != 1) {
		fprintf(stderr, "%s: Failed to load file.\n", name);
		goto err2;
	}

	emu->memory = mem;
	emu->memory_size = 0x10000;
	emu->cpu.pc = offset;
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
emulator_read_byte(void *emuptr, uint16_t address)
{
	struct emulator *emu = emuptr;

	if (emu->memory_size <= address)
		return 0;
	return emu->memory[address];
}

static void
emulator_write_byte(void *emuptr, uint16_t address, uint8_t val)
{
	struct emulator *emu = emuptr;

	if (emu->memory_size <= address)
		return;
	emu->memory[address] = val;
}

static uint8_t
emulator_io_outb(void *emuptr, uint8_t port)
{
	return 0;
}

static void
emulator_io_inb(void *emuptr, uint8_t port, uint8_t val)
{

}

