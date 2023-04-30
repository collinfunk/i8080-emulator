
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
};

void i8080_init(struct i8080 *);

#endif /* I8080_H */

