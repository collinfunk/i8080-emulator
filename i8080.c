
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "i8080.h"

#define FLAG_C  0x01
#define FLAG_P  0x04
#define FLAG_AC 0x10
#define FLAG_Z  0x40
#define FLAG_S  0x80

static inline void
set_flag(struct i8080 *ctx, uint8_t mask)
{
	ctx->f |= mask;
}

static inline void
clr_flag(struct i8080 *ctx, uint8_t mask)
{
	ctx->f &= ~mask;
}

static inline void
set_flag_to(struct i8080 *ctx, uint8_t mask, int val)
{
	if (val == 0)
		clr_flag(ctx, mask);
	else
		set_flag(ctx, mask);
}

static inline uint8_t
get_flag(struct i8080 *ctx, uint8_t mask)
{
	return ((ctx->f & mask) != 0) ? 1 : 0;
}

static inline uint16_t
get_psw(struct i8080 *ctx)
{
	return ((uint16_t)ctx->a << 8) | ((uint16_t)ctx->f);
}

static inline uint16_t
get_bc(struct i8080 *ctx)
{
	return ((uint16_t)ctx->b << 8) | ((uint16_t)ctx->c);
}

static inline uint16_t
get_de(struct i8080 *ctx)
{
	return ((uint16_t)ctx->d << 8) | ((uint16_t)ctx->e);
}

static inline uint16_t
get_hl(struct i8080 *ctx)
{
	return ((uint16_t)ctx->h << 8) | ((uint16_t)ctx->l);
}

static inline void
set_psw(struct i8080 *ctx, uint16_t val)
{
	ctx->a = (val >> 8) & 0xff;
	ctx->f = val & 0xff;
}

static inline void
set_bc(struct i8080 *ctx, uint16_t val)
{
	ctx->b = (val >> 8) & 0xff;
	ctx->c = val & 0xff;
}

static inline void
set_de(struct i8080 *ctx, uint16_t val)
{
	ctx->d = (val >> 8) & 0xff;
	ctx->e = val & 0xff;
}

static inline void
set_hl(struct i8080 *ctx, uint16_t val)
{
	ctx->h = (val >> 8) & 0xff;
	ctx->l = val & 0xff;
}

static inline uint8_t
read_byte(struct i8080 *ctx, uint16_t address)
{
	return ctx->read_byte(ctx->opaque, address);
}

static inline void
write_byte(struct i8080 *ctx, uint16_t address, uint8_t val)
{
	ctx->write_byte(ctx->opaque, address, val);
}

static inline uint16_t
read_word(struct i8080 *ctx, uint16_t address)
{
	return ((uint16_t)read_byte(ctx, address)) |
		((uint16_t)read_byte(ctx, address + 1) << 8);
}

static inline void
write_word(struct i8080 *ctx, uint16_t address, uint16_t val)
{
	write_byte(ctx, address, val & 0xff);
	write_byte(ctx, address + 1, (val >> 8) & 0xff);
}

static inline uint8_t
fetch_byte(struct i8080 *ctx)
{
	return read_byte(ctx, ctx->pc++);
}

static inline uint16_t
fetch_word(struct i8080 *ctx)
{
	uint16_t w;

	w = read_word(ctx, ctx->pc);
	ctx->pc += 2;
	return w;
}

static inline uint16_t
pop_word(struct i8080 *ctx)
{
	uint16_t w;

	w = read_word(ctx, ctx->sp);
	ctx->sp += 2;
	return w;
}

static inline void
push_word(struct i8080 *ctx, uint16_t val)
{
	ctx->sp -= 2;
	write_word(ctx, ctx->sp, val);
}

static inline void
op_jmp(struct i8080 *ctx)
{
	ctx->pc = fetch_word(ctx);
}

static inline void
op_call(struct i8080 *ctx)
{
	push_word(ctx, ctx->pc + 2);
	ctx->pc = fetch_word(ctx);
}

static inline void
op_ret(struct i8080 *ctx)
{
	ctx->pc = pop_word(ctx);
}

static inline void
op_xchg(struct i8080 *ctx)
{
	uint16_t tmp16;

	tmp16 = get_hl(ctx);
	set_hl(ctx, get_de(ctx));
	set_de(ctx, tmp16);
}

void
i8080_init(struct i8080 *ctx)
{
	ctx->a = 0;
	ctx->f = 0x02;
	ctx->b = 0;
	ctx->c = 0;
	ctx->d = 0;
	ctx->e = 0;
	ctx->h = 0;
	ctx->l = 0;
	ctx->sp = 0;
	ctx->pc = 0;
	ctx->halted = 0;
	ctx->cycles = 0;
	ctx->opaque = NULL;
	ctx->read_byte = NULL;
	ctx->write_byte = NULL;
	ctx->io_inb = NULL;
	ctx->io_outb = NULL;
}

void
i8080_print_state(struct i8080 *ctx)
{

	printf("FLAGS: S Z 0 A 0 P 1 C\n");
	printf("       %u %u %u %u %u %u %u %u\n",
			get_flag(ctx, FLAG_S), get_flag(ctx, FLAG_Z),
			get_flag(ctx, 0x20), get_flag(ctx, FLAG_AC),
			get_flag(ctx, 0x08), get_flag(ctx, FLAG_P),
			get_flag(ctx, 0x02), get_flag(ctx, FLAG_C));
	printf("PC:  0x%04x SP: 0x%04x\n", ctx->pc, ctx->sp);
	printf("PSW: 0x%04x (A: 0x%02x F: 0x%02x)\n", get_psw(ctx),
			ctx->a, ctx->f);
	printf("BC:  0x%04x (B: 0x%02x C: 0x%02x)\n", get_bc(ctx),
			ctx->b, ctx->c);
	printf("DE:  0x%04x (D: 0x%02x E: 0x%02x)\n", get_de(ctx),
			ctx->d, ctx->e);
	printf("HL:  0x%04x (H: 0x%02x L: 0x%02x)\n", get_hl(ctx),
			ctx->h, ctx->l);
	printf("TOTAL CYCLES: %ju\n", (uintmax_t)ctx->cycles);
}

void
i8080_exec_opcode(struct i8080 *ctx, uint8_t opcode)
{
	switch (opcode) {
		case 0x00: /* NOP */
			ctx->cycles += 4;
			break;
		case 0x01: /* LXI B */
			set_bc(ctx, fetch_word(ctx));
			ctx->cycles += 10;
			break;
		case 0x06: /* MVI B */
			ctx->b = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x0e: /* MVI C */
			ctx->c = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x11: /* LXI D */
			set_de(ctx, fetch_word(ctx));
			ctx->cycles += 10;
			break;
		case 0x16: /* MVI D */
			ctx->d = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x1e: /* MVI E */
			ctx->e = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x21: /* LXI H */
			set_hl(ctx, fetch_word(ctx));
			ctx->cycles += 10;
			break;
		case 0x26: /* MVI H */
			ctx->h = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x2e: /* MVI L */
			ctx->l = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x31: /* LXI SP */
			ctx->sp = fetch_word(ctx);
			ctx->cycles += 10;
			break;
		case 0x36: /* MVI M */
			write_byte(ctx, get_hl(ctx), fetch_byte(ctx));
			ctx->cycles += 10;
			break;
		case 0x3e: /* MVI A */
			ctx->a = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0xc3: /* JMP */
			op_jmp(ctx);
			ctx->cycles += 10;
			break;
		case 0xc9: /* RET */
			op_ret(ctx);
			ctx->cycles += 10;
			break;
		case 0xcd: /* CALL */
			op_call(ctx);
			ctx->cycles += 17;
			break;
		case 0xd1: /* POP D */
			set_de(ctx, pop_word(ctx));
			ctx->cycles += 10;
			break;
		case 0xd3: /* OUT */
			ctx->io_outb(ctx->opaque, fetch_byte(ctx), ctx->a);
			ctx->cycles += 10;
			break;
		case 0xd5: /* PUSH D */
			push_word(ctx, get_de(ctx));
			ctx->cycles += 11;
			break;
		case 0xeb: /* XCHG */
			op_xchg(ctx);
			ctx->cycles += 5;
			break;
		default:
			fprintf(stderr, "0x%02x: Unimplemented opcode.\n",
					opcode);
			i8080_print_state(ctx);
			exit(1);
	}
}
