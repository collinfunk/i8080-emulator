
#ifndef I8080_H
#define I8080_H

#include <stdint.h>

struct i8080 {
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
	uint8_t halted;
	uint8_t int_enable; /* INTE - Interrupt enable */
	uint8_t int_requested; /* INT - Interrupt requested */
	uint8_t int_opcode; /* In case someone interrupts with a 0x00 nop? */
	uint64_t cycles;
	void *opaque;
	uint8_t (*read_byte)(void *, uint16_t);
	void (*write_byte)(void *, uint16_t, uint8_t);
	uint8_t (*io_inb)(void *, uint8_t);
	void (*io_outb)(void *, uint8_t, uint8_t);
};

void i8080_init(struct i8080 *);
void i8080_step(struct i8080 *);
/* Send an interrupt to execute an instruction */
void i8080_interrupt(struct i8080 *, uint8_t);
void i8080_exec_opcode(struct i8080 *, uint8_t);

#endif /* I8080_H */

