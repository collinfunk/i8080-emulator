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

#include "i8080.h"

struct emulator
{
  struct i8080 cpu;
  uint8_t *memory;
  size_t memory_size;
};

static void usage (void);
static struct emulator *emulator_create (void);
static void emulator_destroy (struct emulator *);
static int emulator_load_file (struct emulator *, const char *, uint16_t);
static uint8_t emulator_read_byte (void *, uint16_t);
static void emulator_write_byte (void *, uint16_t, uint8_t);
static uint8_t emulator_io_inb (void *, uint8_t);
static void emulator_io_outb (void *, uint8_t, uint8_t);

int
main (int argc, char **argv)
{
  struct emulator *emu;
  uintmax_t opcount;

  if (argc != 2)
    usage ();

  emu = emulator_create ();
  if (emu == NULL)
    {
      fprintf (stderr, "Failed to allocate memory.\n");
      exit (1);
    }

  if (emulator_load_file (emu, argv[1], 0x100) < 0)
    {
      emulator_destroy (emu);
      exit (1);
    }

  /* Set the start of memory to HLT's for when the test finishes. */
  memset (emu->memory, 0x76, 0x100);

  /* Substitute CP/M BDOS calls with an OUT 1 followed by a RET. */
  emu->memory[0x0005] = 0xd3;
  emu->memory[0x0006] = 0x01;
  /* RET */
  emu->memory[0x0007] = 0xc9;

  for (opcount = 0; !emu->cpu.halted; ++opcount)
    i8080_step (&emu->cpu);

  printf ("\n");
  printf ("Instruction count: %ju\n", opcount);
  printf ("Cycle count:       %ju\n", emu->cpu.cycles);
  emulator_destroy (emu);
  return 0;
}

static void
usage (void)
{
  fprintf (stderr, "i8080-emulator file\n");
  exit (1);
}

static struct emulator *
emulator_create (void)
{
  struct emulator *emu;

  emu = (struct emulator *) calloc (1, sizeof (struct emulator));
  if (emu == NULL)
    return NULL;
  i8080_init (&emu->cpu);
  emu->cpu.opaque = emu;
  emu->cpu.read_byte = emulator_read_byte;
  emu->cpu.write_byte = emulator_write_byte;
  emu->cpu.io_inb = emulator_io_inb;
  emu->cpu.io_outb = emulator_io_outb;
  return emu;
}

static void
emulator_destroy (struct emulator *emu)
{
  free (emu->memory);
  free (emu);
}

static int
emulator_load_file (struct emulator *emu, const char *name, uint16_t offset)
{
  struct stat st;
  FILE *fp;
  uint8_t *memory;

  if (stat (name, &st) < 0)
    {
      fprintf (stderr, "%s: %s.\n", name, strerror (errno));
      return -1;
    }

  if (!S_ISREG (st.st_mode))
    {
      fprintf (stderr, "%s: Not a regular file.\n", name);
      return -1;
    }

  if (st.st_size <= 0 || st.st_size > UINT16_MAX)
    {
      fprintf (stderr, "%s: Invalid file size (%jd bytes).\n", name,
               (intmax_t) st.st_size);
      return -1;
    }

  if (st.st_size + offset > UINT16_MAX)
    {
      fprintf (stderr, "%s: Offset too large to address file.\n", name);
      return -1;
    }

  fp = fopen (name, "rb");
  if (fp == NULL)
    {
      fprintf (stderr, "%s: %s.\n", name, strerror (errno));
      return -1;
    }

  memory = (uint8_t *) calloc (1, 0x10000);
  if (memory == NULL)
    {
      fprintf (stderr, "Memory allocation failed.\n");
      fclose (fp);
      return -1;
    }

  if (fread (&memory[offset], st.st_size, 1, fp) != 1)
    {
      fprintf (stderr, "%s: Failed to load file.\n", name);
      fclose (fp);
      free (memory);
      return -1;
    }

  emu->memory = memory;
  emu->memory_size = 0x10000;
  emu->cpu.pc = offset;
  fclose (fp);
  return 0;
}

static uint8_t
emulator_read_byte (void *emuptr, uint16_t address)
{
  struct emulator *emu = (struct emulator *) emuptr;

  return (emu->memory_size > address) ? emu->memory[address] : UINT8_C (0);
}

static void
emulator_write_byte (void *emuptr, uint16_t address, uint8_t value)
{
  struct emulator *emu = (struct emulator *) emuptr;

  if (emu->memory_size > address)
    emu->memory[address] = value;
}

static uint8_t
emulator_io_inb (void *emuptr, uint8_t port)
{
  return 0;
}

/*
 * Handles the output port from the CPU. This is used to replace
 * CP/M BDOS system calls used by the test binaries. The tests use
 * functions 5 and 9. Function 5 is used when the 8-bit register C contains
 * 2. This function prints the ASCII character stored in E to the screen.
 * Function 9 sends a memory address to a string in the 16-bit register
 * DE. Characters a read and printed until a terminating $ character.
 */
static void
emulator_io_outb (void *emuptr, uint8_t port, uint8_t value)
{
  struct emulator *emu = (struct emulator *) emuptr;
  struct i8080 *cpu = &emu->cpu;
  uint16_t de;
  uint8_t ch;

  if (port == 1)
    {
      if (cpu->c == 2)
        putchar (cpu->e);
      else if (cpu->c == 9)
        {
          de = ((uint16_t) cpu->d << 8) | ((uint16_t) cpu->e);
          ch = emulator_read_byte (cpu->opaque, de++);
          while (ch != '$')
            {
              putchar (ch);
              ch = emulator_read_byte (cpu->opaque, de++);
            }
        }
    }
}
