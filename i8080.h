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

#ifndef I8080_H
#define I8080_H

#include <stdbool.h>
#include <stdint.h>

struct i8080
{
  uint8_t a; /* Accumulator */
  uint8_t f; /* Flags */
  uint8_t b;
  uint8_t c;
  uint8_t d;
  uint8_t e;
  uint8_t h;
  uint8_t l;
  uint16_t sp; /* Stack pointer */
  uint16_t pc; /* Program counter */
  bool halted;
  bool int_enable;    /* INTE - Interrupt enable */
  bool int_requested; /* INT - Interrupt requested */
  uint8_t int_opcode; /* In case someone interrupts with a 0x00 nop? */
  uintmax_t cycles;
  void *user_data;
  uint8_t (*read_byte) (void *, uint16_t);
  void (*write_byte) (void *, uint16_t, uint8_t);
  uint8_t (*io_inb) (void *, uint8_t);
  void (*io_outb) (void *, uint8_t, uint8_t);
};

void i8080_init (struct i8080 *);
void i8080_step (struct i8080 *);
/* Send an interrupt to execute an instruction */
void i8080_interrupt (struct i8080 *, uint8_t);
void i8080_exec_opcode (struct i8080 *, uint8_t);

#endif /* I8080_H */
