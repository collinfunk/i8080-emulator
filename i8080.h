
#ifndef I8080_H
#define I8080_H

#include <stdint.h>

struct i8080 {
	uint8_t a;
	uint8_t f;
	uint8_t b;
	uint8_t c;
	uint8_t d;
	uint8_t e;
	uint8_t h;
	uint8_t l;
	uint16_t sp;
	uint16_t pc;
	uint8_t halted;
	uint64_t cycles;
	void *opaque;
	uint8_t (*read_byte)(void *, uint16_t);
	void (*write_byte)(void *, uint16_t, uint8_t);
	uint8_t (*io_inb)(void *, uint8_t);
	void (*io_outb)(void *, uint8_t, uint8_t);
};

void i8080_init(struct i8080 *);
void i8080_print_state(struct i8080 *);
void i8080_exec_opcode(struct i8080 *, uint8_t);

#endif /* I8080_H */

