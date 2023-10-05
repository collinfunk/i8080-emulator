/*-
 * Copyright (c) 2023, Collin Funk
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
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
#define SI_VRAM_OFFSET 0x2400

/*
 * Space Invaders was originally made for the Taito 8080 in Japan and
 * then was liscened to Midway for US/EU markets. It's kind of hard to
 * find information on the Taito 8080 and Midway 8080 and I am NOT smart
 * enough to read the ciruit diagrams and stuff. From what I can find the
 * i8080 used had a clock speed of 2MHz or slightly under 2 MHz.
 *
 * The screen was 256x224 but rotated 90 degrees counter-clockwise in the
 * Space Invaders cabinet. It had a refresh rate of 60 Hz. Each pixel is
 * on/off so 1 byte encodes for 8 pixels ((256 * 244) / 8 = 7168 bytes).
 * Original machines used physical covers on portions on the screen for color
 * but later supported colored images.
 *
 * The game uses RST 1 (call $0x08) and RST 2 (call $0x10) for interrupts.
 * Interrupts happen around twice per second since their timings are based
 * on the vertical blanking interval of the CRT monitor. Interrupt 1 (RST 1)
 * is used when the beam is around the middle of the screen. The second
 * interrupt (RST 2) is used when the beam is at the last line of the screen.
 */

/* 2 MHz */
#define SI_CLOCK_SPEED 2000000
/* 60 Hz screen */
#define SI_REFRESH_RATE 60
/* Clock speed / Refresh rate */
#define SI_CYCLES_PER_FRAME 33333
/* Interrupts twice per frame, see above */
#define SI_CYCLES_PER_INT 16666
/* 8 pixels per bit, see above */
#define SI_SCREEN_BITS 7168

/*
 * SI_SCREEN_WIDTH and SI_SCREEN_HEIGHT are name based on the rotated
 * screen for clarity with SDL functions.
 */
#define SI_SCREEN_WIDTH 224
#define SI_SCREEN_HEIGHT 256

/*
 * ABGR colors
 * These colors are just a guess from images online. I have
 * no clue what color codes they actually use.
 */
#define SI_ABGR_GREEN 0xff33ff00
#define SI_ABGR_RED 0xff0000ff
#define SI_ABGR_WHITE 0xffffffff
#define SI_ABGR_BLACK 0xff000000

struct spaceinvaders
{
  struct i8080 cpu;
  uint8_t *memory;
  size_t memory_size;
  SDL_Window *window;
  SDL_Renderer *renderer;
  SDL_Texture *texture;
  SDL_Event event;
  uint8_t sdl_started; /* SDL_Quit() */
  uint8_t exit_flag;   /* Signals the end of the loop. */
  uint8_t pause_flag;  /* 1 if emulation is paused. */
  uint8_t color_flag;  /* 1 for color, 0 for black and white */
  uint32_t *video_buffer;
  uint8_t inp0;         /* Input port 0, unused? */
  uint8_t inp1;         /* Input port 1 */
  uint8_t inp2;         /* Input port 2 */
  uint8_t shift0;       /* Shift register lsb */
  uint8_t shift1;       /* Shift register msb */
  uint8_t shift_offset; /* Shift offset */
  uint8_t next_int;     /* RST 1 (0xcf) or RST 2 (0xd7) */
  void *tpixels;        /* SDL_LockTexture() */
  int tpitch;           /* SDL_LockTexture() */
  uint32_t curr_time;
  uint32_t prev_time;
  uint32_t delta_time;
};

static void usage (void);
static struct spaceinvaders *spaceinvaders_create (void);
static void spaceinvaders_destroy (struct spaceinvaders *);
static int spaceinvaders_load_file (struct spaceinvaders *, const char *);
static int sdl_init (struct spaceinvaders *);
static uint8_t spaceinvaders_read_byte (void *, uint16_t);
static void spaceinvaders_write_byte (void *, uint16_t, uint8_t);
static uint8_t spaceinvaders_io_inb (void *, uint8_t);
static void spaceinvaders_io_outb (void *, uint8_t, uint8_t);
static void spaceinvaders_update_texture (struct spaceinvaders *);
static void spaceinvaders_update_screen (struct spaceinvaders *);
static void spaceinvaders_handle_keydown (struct spaceinvaders *,
                                          SDL_Scancode);
static void spaceinvaders_handle_keyup (struct spaceinvaders *, SDL_Scancode);
static void spaceinvaders_handle_cpu (struct spaceinvaders *);
static inline void spaceinvaders_set_pixel (struct spaceinvaders *, uint32_t,
                                            uint32_t, uint32_t);
static inline void spaceinvaders_handle_vram_bit (struct spaceinvaders *,
                                                  uint8_t, uint32_t, uint32_t);
static void spaceinvaders_handle_vram (struct spaceinvaders *);
static inline uint32_t spaceinvaders_get_deltatime32 (struct spaceinvaders *);
static void spaceinvaders_loop (struct spaceinvaders *);

int
main (int argc, char **argv)
{
  struct spaceinvaders *emu;
  if (argc != 2)
    usage ();
  emu = spaceinvaders_create ();
  if (emu == NULL)
    goto fail;
  if (spaceinvaders_load_file (emu, argv[1]) < 0)
    goto fail;
  if (sdl_init (emu) < 0)
    goto fail;
  for (emu->exit_flag = 0; emu->exit_flag == 0;)
    spaceinvaders_loop (emu);
  spaceinvaders_destroy (emu);
  return 0;
fail:
  if (emu != NULL)
    spaceinvaders_destroy (emu);
  exit (1);
}

static void
usage (void)
{
  fprintf (stderr, "spaceinvaders file\n");
  exit (1);
}

static struct spaceinvaders *
spaceinvaders_create (void)
{
  struct spaceinvaders *emu;

  emu = (struct spaceinvaders *) calloc (1, sizeof (struct spaceinvaders));
  if (emu == NULL)
    return NULL;
  i8080_init (&emu->cpu);
  emu->cpu.opaque = emu;
  emu->cpu.read_byte = spaceinvaders_read_byte;
  emu->cpu.write_byte = spaceinvaders_write_byte;
  emu->cpu.io_inb = spaceinvaders_io_inb;
  emu->cpu.io_outb = spaceinvaders_io_outb;
  emu->color_flag = 1;
  emu->next_int = 0xcf;
  return emu;
}

static void
spaceinvaders_destroy (struct spaceinvaders *emu)
{
  if (emu == NULL)
    return;
  if (emu->video_buffer != NULL)
    free (emu->video_buffer);
  if (emu->texture != NULL)
    SDL_DestroyTexture (emu->texture);
  if (emu->renderer != NULL)
    SDL_DestroyRenderer (emu->renderer);
  if (emu->window != NULL)
    SDL_DestroyWindow (emu->window);
  if (emu->sdl_started != 0)
    SDL_Quit ();
  free (emu);
}

static int
sdl_init (struct spaceinvaders *emu)
{
  if (SDL_Init (SDL_INIT_VIDEO) < 0)
    {
      fprintf (stderr, "SDL_Init(): %s.\n", SDL_GetError ());
      return -1;
    }

  /* Flag for cleaning up with SDL_Quit(). */
  emu->sdl_started = 1;
  /* User defined ratios for screen? */
  emu->window
      = SDL_CreateWindow ("Space Invaders Emulator", SDL_WINDOWPOS_CENTERED,
                          SDL_WINDOWPOS_CENTERED, SI_SCREEN_WIDTH * 4,
                          SI_SCREEN_HEIGHT * 4, SDL_WINDOW_RESIZABLE);

  if (emu->window == NULL)
    {
      fprintf (stderr, "SDL_CreateWindow(): %s.\n", SDL_GetError ());
      return -1;
    }

  emu->renderer = SDL_CreateRenderer (
      emu->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (emu->renderer == NULL)
    {
      fprintf (stderr, "SDL_CreateRenderer(): %s.\n", SDL_GetError ());
      return -1;
    }

  /* If window is maximized it won't stretch. */
  if (SDL_RenderSetLogicalSize (emu->renderer, SI_SCREEN_WIDTH,
                                SI_SCREEN_HEIGHT)
      < 0)
    {
      fprintf (stderr, "SDL_RendererSetLogicalSize(): %s.\n", SDL_GetError ());
      return -1;
    }

  emu->texture = SDL_CreateTexture (emu->renderer, SDL_PIXELFORMAT_ABGR8888,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    SI_SCREEN_WIDTH, SI_SCREEN_HEIGHT);

  if (emu->texture == NULL)
    {
      fprintf (stderr, "SDL_CreateTexture(): %s.\n", SDL_GetError ());
      return -1;
    }

  emu->video_buffer = calloc (1, 224 * 256 * 4);
  if (emu->video_buffer == NULL)
    {
      fprintf (stderr, "Failed to allocate video memory.\n");
      return -1;
    }

  SDL_UpdateTexture (emu->texture, NULL, emu->video_buffer,
                     4 * SI_SCREEN_WIDTH);
  return 0;
}

static int
spaceinvaders_load_file (struct spaceinvaders *emu, const char *file)
{
  struct stat st;
  FILE *fp;
  uint8_t *mem;

  if (stat (file, &st) < 0)
    {
      fprintf (stderr, "%s: %s.\n", file, strerror (errno));
      goto err0;
    }

  if (!S_ISREG (st.st_mode))
    {
      fprintf (stderr, "%s: Not a regular file.\n", file);
      goto err0;
    }

  if (st.st_size < 0 || st.st_size > 0x4000)
    {
      fprintf (stderr,
               "%s: Invalid file size. Input the invaders "
               "image combined.\n",
               file);
      goto err0;
    }

  fp = fopen (file, "rb");
  if (fp == NULL)
    {
      fprintf (stderr, "%s: %s.\n", file, strerror (errno));
      goto err0;
    }

  mem = calloc (1, 0x10000);
  if (mem == NULL)
    {
      fprintf (stderr, "Memory allocation failed.\n");
      goto err1;
    }

  if (fread (mem, st.st_size, 1, fp) != 1)
    {
      fprintf (stderr, "%s: Failed to load file.\n", file);
      goto err2;
    }

  emu->memory = mem;
  emu->memory_size = 0x10000;
  fclose (fp);
  return 0;
err2:
  free (mem);
err1:
  fclose (fp);
err0:
  return -1;
}

static uint8_t
spaceinvaders_read_byte (void *emuptr, uint16_t address)
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
spaceinvaders_write_byte (void *emuptr, uint16_t address, uint8_t val)
{
  struct spaceinvaders *emu = emuptr;

  if (emu->memory_size <= address)
    return;
  /* Read only */
  if (address < 0x2000 || address > 0x4000)
    return;
  emu->memory[address] = val;
}

static uint8_t
spaceinvaders_io_inb (void *emuptr, uint8_t port)
{
  struct spaceinvaders *emu = emuptr;
  uint8_t val;
  uint16_t s;

  switch (port)
    {
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
      s = ((uint16_t) emu->shift1 << 8);
      s |= ((uint16_t) emu->shift0);
      val = (s >> (8 - emu->shift_offset)) & 0xff;
      break;
    default: /* Invalid port */
      val = 0;
      break;
    }

  return val;
}

static void
spaceinvaders_io_outb (void *emuptr, uint8_t port, uint8_t val)
{
  struct spaceinvaders *emu = emuptr;

  switch (port)
    {
    case 0x02: /* Shift amount (3 bits) */
      emu->shift_offset = val & 0x07;
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
      /* Pretty sure this checks if the machine crashes? */
      break;
    }
}

static void
spaceinvaders_update_texture (struct spaceinvaders *emu)
{
  emu->tpixels = NULL;
  emu->tpitch = 0;
  if (SDL_LockTexture (emu->texture, NULL, &emu->tpixels, &emu->tpitch) < 0)
    {
      fprintf (stderr, "SDL_LockTexture(): %s.\n", SDL_GetError ());
      return;
    }
  memcpy (emu->tpixels, emu->video_buffer, emu->tpitch * SI_SCREEN_HEIGHT);
  SDL_UnlockTexture (emu->texture);
}

static void
spaceinvaders_update_screen (struct spaceinvaders *emu)
{
  SDL_RenderClear (emu->renderer);
  SDL_RenderCopy (emu->renderer, emu->texture, NULL, NULL);
  SDL_RenderPresent (emu->renderer);
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
 *	Port 2:
 *		Bit 0 (0x01): ???
 *		Bit 1 (0x02): ???
 *		Bit 2 (0x04): ???
 *		Bit 3 (0x08): ???
 *		Bit 4 (0x10): Player 2 fired missle
 *		Bit 5 (0x20): Player 2 moved left
 *		Bit 6 (0x40): Player 2 moved right
 *		Bit 7 (0x80): ???
 */
static void
spaceinvaders_handle_keydown (struct spaceinvaders *emu, SDL_Scancode key)
{
  switch (key)
    {
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
      emu->inp2 |= 0x10;
      break;
    case SDL_SCANCODE_A: /* Move left */
                         /* fallthrough */
    case SDL_SCANCODE_LEFT:
      emu->inp1 |= 0x20;
      emu->inp2 |= 0x20;
      break;
    case SDL_SCANCODE_D: /* Move right */
                         /* fallthrough */
    case SDL_SCANCODE_RIGHT:
      emu->inp1 |= 0x40;
      emu->inp2 |= 0x40;
      break;
    case SDL_SCANCODE_ESCAPE: /* Exit */
      emu->exit_flag = 1;
      break;
    case SDL_SCANCODE_E: /* Toggle color emulation */
      emu->color_flag = (emu->color_flag == 1) ? 0 : 1;
      break;
    case SDL_SCANCODE_Q: /* Pause toggle */
      emu->pause_flag = (emu->pause_flag == 0) ? 1 : 0;
    default:
      break;
    }
}

static void
spaceinvaders_handle_keyup (struct spaceinvaders *emu, SDL_Scancode key)
{
  switch (key)
    {
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
      emu->inp2 &= ~0x10;
      break;
    case SDL_SCANCODE_A: /* Move left */
                         /* fallthrough */
    case SDL_SCANCODE_LEFT:
      emu->inp1 &= ~0x20;
      emu->inp2 &= ~0x20;
      break;
    case SDL_SCANCODE_D: /* Move right */
                         /* fallthrough */
    case SDL_SCANCODE_RIGHT:
      emu->inp1 &= ~0x40;
      emu->inp2 &= ~0x40;
      break;
    default:
      break;
    }
}

/*
 * Each frame has 120 interrupts total, 60 of each RST 1 and RST 2.
 * The interrupts are based on the raster scanning of the CRT monitor.
 * RST 1 is used when the beam is towards the middle of the monitor and
 * RST 2 is used when the beam is about to do a verticle retrace to
 * draw the next frame. Therefore we alternate between each interrupt
 * performing 120 (refresh rate * 2) interrupts total. When RST 2 is called
 * we also update the actual screen based on the contents of vram.
 */
static void
spaceinvaders_handle_cpu (struct spaceinvaders *emu)
{
  struct i8080 *cpu = &emu->cpu;
  const uint64_t need = (emu->delta_time * SI_CLOCK_SPEED) / 1000;
  uint64_t i, prev, diff;

  for (i = diff = 0; i < need; i += diff)
    {
      prev = emu->cpu.cycles;
      i8080_step (cpu);
      diff = cpu->cycles - prev;
      if (cpu->cycles >= SI_CYCLES_PER_INT)
        {
          cpu->cycles -= SI_CYCLES_PER_INT;
          i8080_interrupt (cpu, emu->next_int);
          if (emu->next_int == 0xcf)
            emu->next_int = 0xd7;
          else
            {
              spaceinvaders_handle_vram (emu);
              emu->next_int = 0xcf;
            }
        }
    }
}

/*
 * Estimation of the colors based on the overlay images I could find online.
 * Probably a little off but it is what it is. This can def be cleaned up.
 * If you comment out:
 *	if (set == 0)
 *			emu->video_buffer[off] = SI_ABGR_BLACK;
 *		else
 * in spaceinvaders_handle_vram_bit() you can see the overlay which might help
 * if actual dimensions are out there somewhere.
 */
static inline void
spaceinvaders_set_pixel (struct spaceinvaders *emu, uint32_t off, uint32_t cx,
                         uint32_t cy)
{
  /* Lives / Credit area */
  if (cy >= 240)
    {
      /* Lives Number. */
      if (cx < 16)
        emu->video_buffer[off] = SI_ABGR_WHITE;
      /* Ships avaliable. */
      else if (cx < 102)
        emu->video_buffer[off] = SI_ABGR_GREEN;
      /* Credits. */
      else
        emu->video_buffer[off] = SI_ABGR_WHITE;
    }
  else if (cy >= 184)
    {
      /* Barrier and player, 10 point alien on start screen. */
      emu->video_buffer[off] = SI_ABGR_GREEN;
    }
  else if (cy >= 64)
    {
      /* The main portion of the screen with all the aliens. */
      emu->video_buffer[off] = SI_ABGR_WHITE;
    }
  else if (cy >= 32)
    {
      /* UFO and missle explosions. */
      emu->video_buffer[off] = SI_ABGR_RED;
    }
  else
    {
      /*
       * High score. I've seen this red in some images but I think
       * it is supposed to be white.
       */
      emu->video_buffer[off] = SI_ABGR_WHITE;
    }
}

/*
 * Space invaders machines have a screen thats rotated 90 degrees counter
 * clockwise. A good diagram is at the bottom of this page:
 * https://computerarcheology.com/Arcade/SpaceInvaders/Hardware.html
 */
static inline void
spaceinvaders_handle_vram_bit (struct spaceinvaders *emu, uint8_t cb,
                               uint32_t xoff, uint32_t y)
{
  uint32_t *vbuff;
  uint32_t i, cx, cy, tx, off;
  uint8_t set;

  vbuff = emu->video_buffer;
  for (i = 0; i < 8; ++i, cb = (cb >> 1))
    {
      cx = xoff + i;
      cy = y;
      set = (cb & 0x01) != 0 ? 1 : 0;
      /* Rotate */
      tx = cx;
      cx = cy;
      cy = SI_SCREEN_HEIGHT - tx - 1;
      off = (cy * SI_SCREEN_WIDTH) + cx;
      /* Unlit pixels */
      if (set == 0)
        vbuff[off] = SI_ABGR_BLACK;
      else if (emu->color_flag == 1)
        spaceinvaders_set_pixel (emu, off, cx, cy);
      else
        vbuff[off] = SI_ABGR_WHITE;
    }
}

static void
spaceinvaders_handle_vram (struct spaceinvaders *emu)
{
  uint32_t i, y, xoff;

  for (i = 0; i < SI_SCREEN_BITS; ++i)
    {
      y = (i * 8) / 256;
      xoff = (i * 8) & 255;
      spaceinvaders_handle_vram_bit (emu, emu->memory[SI_VRAM_OFFSET + i],
                                     xoff, y);
    }
  spaceinvaders_update_texture (emu);
}

/*
 * Probably a more percise way to handle timing but SDL_GetTicks() returns
 * milliseconds which seems to work fine. SDL version 2.0.18 has
 * SDL_GetTicks64() which should be used to avoid overflows but Debian stable
 * still uses version 2.0.14. This function just handles any overflows if
 * someone spends ~49 days on space invaders.
 */
static inline uint32_t
spaceinvaders_get_deltatime32 (struct spaceinvaders *emu)
{
  /* Only time this happens is if curr_time overflows. */
  if (emu->prev_time > emu->curr_time)
    return UINT32_MAX - emu->prev_time + emu->curr_time;
  else
    return emu->curr_time - emu->prev_time;
}

static void
spaceinvaders_loop (struct spaceinvaders *emu)
{
  SDL_Scancode kp;

  /* Milliseconds since SDL_Init() */
  emu->curr_time = SDL_GetTicks ();
  emu->delta_time = spaceinvaders_get_deltatime32 (emu);
  while (SDL_PollEvent (&emu->event) != 0)
    {
      if (emu->event.type == SDL_QUIT)
        emu->exit_flag = 1;
      else if (emu->event.type == SDL_KEYDOWN)
        {
          kp = emu->event.key.keysym.scancode;
          spaceinvaders_handle_keydown (emu, kp);
        }
      else if (emu->event.type == SDL_KEYUP)
        {
          kp = emu->event.key.keysym.scancode;
          spaceinvaders_handle_keyup (emu, kp);
        }
    }

  /* If delta time is 0 we can chill. */
  if (emu->delta_time > 0 && emu->pause_flag == 0)
    {
      spaceinvaders_handle_cpu (emu);
      spaceinvaders_update_screen (emu);
    }

  emu->prev_time = emu->curr_time;
}
