
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <SDL2/SDL.h>

#include "i8080.h"

/*
 *
 * 0000 - 1fff rom
 * 2000 - 23ff ram
 * 2400 - 3fff video RAM
 * 4000 - ram mirror
 */
#define SI_MEMORY_SIZE 0x4000
#define SI_SCREEN_WIDTH 224
#define SI_SCREEN_HEIGHT 256

struct spaceinvaders {
	struct i8080 cpu;
	uint8_t *memory;
	size_t memory_size;
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_Event event;
	uint8_t sdl_started; /* SDL_Quit() */
	uint8_t exit_flag;
	uint8_t *video_buffer;
	uint8_t inp0; /* Unused? */
	uint8_t inp1;
	uint8_t inp2;
	uint8_t shift0;
	uint8_t shift1;
	uint8_t shift_offset;
	void *tpixels; /* SDL_LockTexture() */
	int tpitch; /* SDL_LockTexture() */
	uint64_t curr_time;
	uint64_t prev_time;
	uint64_t delta_time;
};

static void usage(void);
static struct spaceinvaders *spaceinvaders_create(void);
static void spaceinvaders_destroy(struct spaceinvaders *);
static int spaceinvaders_load_file(struct spaceinvaders *, const char *);
static int sdl_init(struct spaceinvaders *);
static uint8_t spaceinvaders_read_byte(void *, uint16_t);
static void spaceinvaders_write_byte(void *, uint16_t, uint8_t);
static uint8_t spaceinvaders_io_inb(void *, uint8_t);
static void spaceinvaders_io_outb(void *, uint8_t, uint8_t);
static void spaceinvaders_update_texture(struct spaceinvaders *);
static void spaceinvaders_update_screen(struct spaceinvaders *);
static void spaceinvaders_handle_keydown(struct spaceinvaders *, SDL_Scancode);
static void spaceinvaders_handle_keyup(struct spaceinvaders *, SDL_Scancode);
static void spaceinvaders_loop(struct spaceinvaders *);

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

	if (sdl_init(emu) < 0)
		goto fail;

	for (emu->exit_flag = 0; emu->exit_flag == 0;)
		spaceinvaders_loop(emu);

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
	cpu->opaque = emu;
	cpu->read_byte = spaceinvaders_read_byte;
	cpu->write_byte = spaceinvaders_write_byte;
	cpu->io_inb = spaceinvaders_io_inb;
	cpu->io_outb = spaceinvaders_io_outb;
	emu->memory = NULL;
	emu->memory_size = 0;
	emu->video_buffer = NULL;
	emu->window = NULL;
	emu->renderer = NULL;
	emu->texture = NULL;
	emu->sdl_started = 0;
	emu->exit_flag = 0;
	emu->inp0 = 0;
	emu->inp1 = 0;
	emu->inp2 = 0;
	emu->shift0 = 0;
	emu->shift1 = 0;
	emu->shift_offset = 0;
	emu->tpixels = NULL;
	emu->tpitch = 0;
	emu->curr_time = 0;
	emu->prev_time = 0;
	emu->delta_time = 0;
	return emu;
}

static void
spaceinvaders_destroy(struct spaceinvaders *emu)
{
	if (emu == NULL)
		return;
	free(emu->memory);
	if (emu->video_buffer != NULL)
		free(emu->video_buffer);
	if (emu->texture != NULL)
		SDL_DestroyTexture(emu->texture);
	if (emu->renderer != NULL)
		SDL_DestroyRenderer(emu->renderer);
	if (emu->window != NULL)
		SDL_DestroyWindow(emu->window);
	if (emu->sdl_started != 0)
		SDL_Quit();
	free(emu);
}

static int
sdl_init(struct spaceinvaders *emu)
{
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
		return -1;
	emu->sdl_started = 1;

	/* User defined ratios for screen? */
	emu->window = SDL_CreateWindow("Space Invaders Emulator",
			SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
			SI_SCREEN_WIDTH * 4, SI_SCREEN_HEIGHT * 4,
			SDL_WINDOW_RESIZABLE);

	if (emu->window == NULL)
		return -1;

	emu->renderer = SDL_CreateRenderer(emu->window, -1,
			SDL_RENDERER_ACCELERATED);
	if (emu->renderer == NULL)
		return -1;

	emu->texture = SDL_CreateTexture(emu->renderer, SDL_PIXELFORMAT_RGBA32,
			SDL_TEXTUREACCESS_STREAMING, SI_SCREEN_WIDTH,
			SI_SCREEN_HEIGHT);

	if (emu->texture == NULL)
		return -1;

	emu->video_buffer = calloc(256, 244 * 4);
	if (emu->video_buffer == NULL)
		return -1;

	SDL_UpdateTexture(emu->texture, NULL, emu->video_buffer,
			4 * SI_SCREEN_WIDTH);
	return 0;
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

static void
spaceinvaders_update_texture(struct spaceinvaders *emu)
{
	emu->tpixels = NULL;
	emu->tpitch = 0;
	if (SDL_LockTexture(emu->texture, NULL, &emu->tpixels,
				&emu->tpitch) < 0) {
		fprintf(stderr, "SDL_LockTexture(): %s.\n", SDL_GetError());
		return;
	}

	memcpy(emu->tpixels, emu->video_buffer,
			emu->tpitch * SI_SCREEN_HEIGHT);
	SDL_UnlockTexture(emu->texture);
}

static void
spaceinvaders_update_screen(struct spaceinvaders *emu)
{
	SDL_RenderClear(emu->renderer);
	SDL_RenderCopy(emu->renderer, emu->texture, NULL, NULL);
	SDL_RenderPresent(emu->renderer);
}

/*
 * Inputs:
 *	Port 1:
 *		Bit 0 (0x01): Credit
 *		Bit 1 (0x02): 2 player start
 *		Bit 2 (0x04): 1 player start
 *		Bit 3 (0x08): Always 1
 *		Bit 4 (0x10): Player 1 fired missle
 *		Bit 5 (0x20): Player 1 moved left
 *		Bit 6 (0x40): Player 1 moved right
 *		Bit 7 (0x80): Not connected
 */
static void
spaceinvaders_handle_keydown(struct spaceinvaders *emu, SDL_Scancode key)
{
	switch (key) {
		case SDL_SCANCODE_3: /* Insert coin */
			emu->inp1 |= 0x01;
			break;
		case SDL_SCANCODE_2: /* Two players */
			emu->inp1 |= 0x02;
			break;
		case SDL_SCANCODE_1: /* One player */
			emu->inp1 |= 0x04;
			break;
		case SDL_SCANCODE_SPACE: /* Fire missle */
			emu->inp1 |= 0x10;
			break;
		case SDL_SCANCODE_A: /* Move left */
			emu->inp1 |= 0x20;
			break;
		case SDL_SCANCODE_D: /* Move right */
			emu->inp1 |= 0x40;
			break;
		case SDL_SCANCODE_ESCAPE:
			emu->exit_flag = 1;
			break;
	}
}

static void
spaceinvaders_handle_keyup(struct spaceinvaders *emu, SDL_Scancode key)
{
	switch (key) {
		case SDL_SCANCODE_3: /* Insert coin */
			emu->inp1 &= ~0x01;
			break;
		case SDL_SCANCODE_2: /* Two players */
			emu->inp1 &= ~0x02;
			break;
		case SDL_SCANCODE_1: /* One player */
			emu->inp1 &= ~0x04;
			break;
		case SDL_SCANCODE_SPACE: /* Fire missle */
			emu->inp1 &= ~0x10;
			break;
		case SDL_SCANCODE_A: /* Move left */
			emu->inp1 &= ~0x20;
			break;
		case SDL_SCANCODE_D: /* Move right */
			emu->inp1 &= ~0x40;
			break;
	}
}

static void
spaceinvaders_loop(struct spaceinvaders *emu)
{
	/* Milliseconds since SDL_Init() */
	emu->curr_time = SDL_GetTicks64();
	emu->delta_time = emu->curr_time - emu->prev_time;

	while (SDL_PollEvent(&emu->event) != 0) {
		if (emu->event.type == SDL_QUIT)
			emu->exit_flag = 1;
		else if (emu->event.type == SDL_KEYDOWN)
			spaceinvaders_handle_keydown(emu,
					emu->event.key.keysym.scancode);
		else if (emu->event.type == SDL_KEYUP)
			spaceinvaders_handle_keyup(emu,
					emu->event.key.keysym.scancode);
	}

	spaceinvaders_update_screen(emu);
	emu->prev_time = emu->curr_time;
}


