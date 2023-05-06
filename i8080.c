
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "i8080.h"

#define FLAG_C  0x01 /* Carry flag */
/* 0x02 is always set to 1. */
#define FLAG_P  0x04 /* Parity flag */
/* 0x08 is always set to 0. */
#define FLAG_AC 0x10 /* Aux. carry flag */
/* 0x20 is always set to 0. */
#define FLAG_Z  0x40 /* Zero flag */
#define FLAG_S  0x80 /* Sign flag */

/*
 * Parity lookup table. 1 for even parity, 0 for odd.
 * This is generated from a file in /misc.
 */
static const uint8_t parity_table[256] = {
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0,
	1, 0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1,
};

/*
 * Tables used for setting the auxiliary carry bit in the flags
 * register. The flag should be set when there is a carry out of bit 3.
 * It is affected by all addition, subtraction, increment, decrement,
 * and compare instructions.
 */
static const uint8_t ac_table[8] = {
	0, 0, 1, 0, 1, 0, 1, 1
};

static const uint8_t subtract_ac_table[8] = {
	1, 0, 0, 0, 1, 1, 1, 0
};

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
op_rst(struct i8080 *ctx, uint16_t address)
{
	push_word(ctx, ctx->pc);
	ctx->pc = address;
}

static inline void
op_xchg(struct i8080 *ctx)
{
	uint16_t tmp16;

	tmp16 = get_hl(ctx);
	set_hl(ctx, get_de(ctx));
	set_de(ctx, tmp16);
}

static inline void
op_xthl(struct i8080 *ctx)
{
	uint16_t tmp16;

	tmp16 = read_word(ctx, ctx->sp);
	write_word(ctx, ctx->sp, get_hl(ctx));
	set_hl(ctx, tmp16);
}

static inline void
op_add(struct i8080 *ctx, uint8_t val)
{
	uint16_t tmp16;
	uint8_t acindex;

	tmp16 = ctx->a + val;
	acindex = ((ctx->a & 0x88) >> 1) | ((val & 0x88) >> 2) |
		((tmp16 & 0x88) >> 3);
	ctx->a = tmp16 & 0xff;
	set_flag_to(ctx, FLAG_C, (tmp16 & 0x100) != 0);
	set_flag_to(ctx, FLAG_P, parity_table[ctx->a]);
	set_flag_to(ctx, FLAG_AC, ac_table[acindex & 7]);
	set_flag_to(ctx, FLAG_Z, ctx->a == 0);
	set_flag_to(ctx, FLAG_S, (ctx->a & 0x80) != 0);
}

static inline void
op_adc(struct i8080 *ctx, uint8_t val)
{
	uint16_t tmp16;
	uint8_t acindex;

	tmp16 = ctx->a + val + get_flag(ctx, FLAG_C);
	acindex = ((ctx->a & 0x88) >> 1) | ((val & 0x88) >> 2) |
		((tmp16 & 0x88) >> 3);
	ctx->a = tmp16 & 0xff;
	set_flag_to(ctx, FLAG_C, (tmp16 & 0x100) != 0);
	set_flag_to(ctx, FLAG_P, parity_table[ctx->a]);
	set_flag_to(ctx, FLAG_AC, ac_table[acindex & 7]);
	set_flag_to(ctx, FLAG_Z, ctx->a == 0);
	set_flag_to(ctx, FLAG_S, (ctx->a & 0x80) != 0);
}

static inline void
op_sub(struct i8080 *ctx, uint8_t val)
{
	uint16_t tmp16;
	uint8_t acindex;

	tmp16 = ctx->a - val;
	acindex = ((ctx->a & 0x88) >> 1) | ((val & 0x88) >> 2) |
		((tmp16 & 0x88) >> 3);
	ctx->a = tmp16 & 0xff;
	set_flag_to(ctx, FLAG_C, (tmp16 & 0x100) != 0);
	set_flag_to(ctx, FLAG_P, parity_table[ctx->a]);
	set_flag_to(ctx, FLAG_AC, subtract_ac_table[acindex & 7]);
	set_flag_to(ctx, FLAG_Z, ctx->a == 0);
	set_flag_to(ctx, FLAG_S, (ctx->a & 0x80) != 0);
}

static inline void
op_sbb(struct i8080 *ctx, uint8_t val)
{
	uint16_t tmp16;
	uint8_t acindex;

	tmp16 = ctx->a - val - get_flag(ctx, FLAG_C);
	acindex = ((ctx->a & 0x88) >> 1) | ((val & 0x88) >> 2) |
		((tmp16 & 0x88) >> 3);
	ctx->a = tmp16 & 0xff;
	set_flag_to(ctx, FLAG_C, (tmp16 & 0x100) != 0);
	set_flag_to(ctx, FLAG_P, parity_table[ctx->a]);
	set_flag_to(ctx, FLAG_AC, subtract_ac_table[acindex & 7]);
	set_flag_to(ctx, FLAG_Z, ctx->a == 0);
	set_flag_to(ctx, FLAG_S, (ctx->a & 0x80) != 0);
}

static inline void
op_ana(struct i8080 *ctx, uint8_t val)
{
	uint8_t tmp8;

	tmp8 = ctx->a & val;
	clr_flag(ctx, FLAG_C);
	set_flag_to(ctx, FLAG_P, parity_table[tmp8]);
	set_flag_to(ctx, FLAG_AC, ((ctx->a | val) & 0x08) != 0);
	set_flag_to(ctx, FLAG_Z, tmp8 == 0);
	set_flag_to(ctx, FLAG_S, (tmp8 & 0x80) != 0);
	ctx->a = tmp8;
}

static inline void
op_xra(struct i8080 *ctx, uint8_t val)
{
	uint8_t tmp8;

	tmp8 = ctx->a ^ val;
	clr_flag(ctx, FLAG_C);
	set_flag_to(ctx, FLAG_P, parity_table[tmp8]);
	clr_flag(ctx, FLAG_AC);
	set_flag_to(ctx, FLAG_Z, tmp8 == 0);
	set_flag_to(ctx, FLAG_S, (tmp8 & 0x80) != 0);
	ctx->a = tmp8;
}

static inline void
op_ora(struct i8080 *ctx, uint8_t val)
{
	uint8_t tmp8;

	tmp8 = ctx->a | val;
	clr_flag(ctx, FLAG_C);
	set_flag_to(ctx, FLAG_P, parity_table[tmp8]);
	clr_flag(ctx, FLAG_AC);
	set_flag_to(ctx, FLAG_Z, tmp8 == 0);
	set_flag_to(ctx, FLAG_S, (tmp8 & 0x80) != 0);
	ctx->a = tmp8;
}

static inline void
op_cmp(struct i8080 *ctx, uint8_t val)
{
	uint16_t tmp16;
	uint8_t acindex;

	tmp16 = ctx->a - val;
	acindex = ((ctx->a & 0x88) >> 1) | ((val & 0x88) >> 2) |
		((tmp16 & 0x88) >> 3);
	set_flag_to(ctx, FLAG_C, (tmp16 & 0x100) != 0);
	set_flag_to(ctx, FLAG_P, parity_table[tmp16 & 0xff]);
	set_flag_to(ctx, FLAG_AC, subtract_ac_table[acindex & 7]);
	set_flag_to(ctx, FLAG_Z, (tmp16 & 0xff) == 0);
	set_flag_to(ctx, FLAG_S, (tmp16 & 0x80) != 0);
}

static inline uint8_t
op_inr(struct i8080 *ctx, uint8_t val)
{
	uint8_t tmp8;

	tmp8 = val + 1;
	set_flag_to(ctx, FLAG_P, parity_table[tmp8]);
	set_flag_to(ctx, FLAG_AC, (tmp8 & 0x0f) == 0);
	set_flag_to(ctx, FLAG_Z, tmp8 == 0);
	set_flag_to(ctx, FLAG_S, (tmp8 & 0x80) != 0);
	return tmp8;
}

static inline uint8_t
op_dcr(struct i8080 *ctx, uint8_t val)
{
	uint8_t tmp8;

	tmp8 = val - 1;
	set_flag_to(ctx, FLAG_P, parity_table[tmp8]);
	set_flag_to(ctx, FLAG_AC, !((tmp8 & 0x0f) == 0x0f));
	set_flag_to(ctx, FLAG_Z, tmp8 == 0);
	set_flag_to(ctx, FLAG_S, (tmp8 & 0x80) != 0);
	return tmp8;
}

static inline void
op_dad(struct i8080 *ctx, uint16_t val)
{
	uint32_t tmp32;

	tmp32 = get_hl(ctx) + val;
	set_flag_to(ctx, FLAG_C, (tmp32 & 0x10000) != 0);
	set_hl(ctx, tmp32 & 0xffff);
}

static inline void
op_rlc(struct i8080 *ctx)
{
	set_flag_to(ctx, FLAG_C, (ctx->a & 0x80) != 0);
	ctx->a = (ctx->a << 1) | get_flag(ctx, FLAG_C);
}

static inline void
op_rrc(struct i8080 *ctx)
{
	set_flag_to(ctx, FLAG_C, ctx->a & 0x01);
	ctx->a = (ctx->a >> 1) | (get_flag(ctx, FLAG_C) << 7);
}

static inline void
op_ral(struct i8080 *ctx)
{
	uint8_t tmp8;

	tmp8 = get_flag(ctx, FLAG_C);
	set_flag_to(ctx, FLAG_C, (ctx->a & 0x80) != 0);
	ctx->a = (ctx->a << 1) | tmp8;
}

static inline void
op_rar(struct i8080 *ctx)
{
	uint8_t tmp8;

	tmp8 = get_flag(ctx, FLAG_C);
	set_flag_to(ctx, FLAG_C, ctx->a & 0x01);
	ctx->a = (ctx->a >> 1) | (tmp8 << 7);
}

static inline void
op_daa(struct i8080 *ctx)
{
	uint8_t ahi, alo, c, auxc, newc, inc;

	ahi = (ctx->a >> 4) & 0x0f;
	alo = ctx->a & 0x0f;
	c = get_flag(ctx, FLAG_C);
	auxc = get_flag(ctx, FLAG_AC);
	newc = inc = 0;

	if (alo > 9 || auxc != 0)
		inc += 0x06;
	if (ahi > 9 || c != 0 || (ahi >= 9 && alo > 9)) {
		newc = 1;
		inc += 0x60;
	}

	op_add(ctx, inc);
	set_flag_to(ctx, FLAG_C, newc == 1);
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
	ctx->int_enable = 0;
	ctx->int_requested = 0;
	ctx->int_opcode = 0;
	ctx->cycles = 0;
	ctx->opaque = NULL;
	ctx->read_byte = NULL;
	ctx->write_byte = NULL;
	ctx->io_inb = NULL;
	ctx->io_outb = NULL;
}

void
i8080_step(struct i8080 *ctx)
{
	/* If an interrupt is requested */
	if (ctx->int_requested != 0 && ctx->int_enable != 0) {
		/* Acknowledge request and reset INTE. */
		ctx->int_enable = 0;
		ctx->int_requested = 0;
		/* Interrupt occured so unhalt */
		ctx->halted = 0;

		/* Execute the requested opcode */
		i8080_exec_opcode(ctx, ctx->int_opcode);
		return;
	}

	/* Wait for an interrupt. */
	if (ctx->halted != 0)
		return;
	i8080_exec_opcode(ctx, fetch_byte(ctx));
}

void
i8080_interrupt(struct i8080 *ctx, uint8_t opcode)
{
	ctx->int_requested = 1;
	ctx->int_opcode = opcode;
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
		case 0x02: /* STAX B */
			write_byte(ctx, get_bc(ctx), ctx->a);
			ctx->cycles += 7;
			break;
		case 0x03: /* INX B */
			if (++ctx->c == 0)
				ctx->b++;
			ctx->cycles += 5;
			break;
		case 0x04: /* INR B */
			ctx->b = op_inr(ctx, ctx->b);
			ctx->cycles += 5;
			break;
		case 0x05: /* DCR B */
			ctx->b = op_dcr(ctx, ctx->b);
			ctx->cycles += 5;
			break;
		case 0x06: /* MVI B */
			ctx->b = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x07: /* RLC */
			op_rlc(ctx);
			ctx->cycles += 4;
			break;
		case 0x08: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x09: /* DAD B */
			op_dad(ctx, get_bc(ctx));
			ctx->cycles += 10;
			break;
		case 0x0a: /* LDAX B */
			ctx->a = read_byte(ctx, get_bc(ctx));
			ctx->cycles += 7;
			break;
		case 0x0b: /* DCX B */
			if (--ctx->c == 0xff)
				ctx->b--;
			ctx->cycles += 5;
			break;
		case 0x0c: /* INR C */
			ctx->c = op_inr(ctx, ctx->c);
			ctx->cycles += 5;
			break;
		case 0x0d: /* DCR C */
			ctx->c = op_dcr(ctx, ctx->c);
			ctx->cycles += 5;
			break;
		case 0x0e: /* MVI C */
			ctx->c = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x0f: /* RRC */
			op_rrc(ctx);
			ctx->cycles += 4;
			break;
		case 0x10: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x11: /* LXI D */
			set_de(ctx, fetch_word(ctx));
			ctx->cycles += 10;
			break;
		case 0x12: /* STAX D */
			write_byte(ctx, get_de(ctx), ctx->a);
			ctx->cycles += 7;
			break;
		case 0x13: /* INX D */
			if (++ctx->e == 0)
				ctx->d++;
			ctx->cycles += 5;
			break;
		case 0x14: /* INR D */
			ctx->d = op_inr(ctx, ctx->d);
			ctx->cycles += 5;
			break;
		case 0x15: /* DCR D */
			ctx->d = op_dcr(ctx, ctx->d);
			ctx->cycles += 5;
			break;
		case 0x16: /* MVI D */
			ctx->d = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x17: /* RAL */
			op_ral(ctx);
			ctx->cycles += 4;
			break;
		case 0x18: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x19: /* DAD D */
			op_dad(ctx, get_de(ctx));
			ctx->cycles += 10;
			break;
		case 0x1a: /* LDAX D */
			ctx->a = read_byte(ctx, get_de(ctx));
			ctx->cycles += 7;
			break;
		case 0x1b: /* DCX D */
			if (--ctx->e == 0xff)
				ctx->d--;
			ctx->cycles += 5;
			break;
		case 0x1c: /* INR E */
			ctx->e = op_inr(ctx, ctx->e);
			ctx->cycles += 5;
			break;
		case 0x1d: /* DCR E */
			ctx->e = op_dcr(ctx, ctx->e);
			ctx->cycles += 5;
			break;
		case 0x1e: /* MVI E */
			ctx->e = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x1f: /* RAR */
			op_rar(ctx);
			ctx->cycles += 4;
			break;
		case 0x20: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x21: /* LXI H */
			set_hl(ctx, fetch_word(ctx));
			ctx->cycles += 10;
			break;
		case 0x22: /* SHLD */
			write_word(ctx, fetch_word(ctx), get_hl(ctx));
			ctx->cycles += 16;
			break;
		case 0x23: /* INX H */
			if (++ctx->l == 0)
				ctx->h++;
			ctx->cycles += 5;
			break;
		case 0x24: /* INR H */
			ctx->h = op_inr(ctx, ctx->h);
			ctx->cycles += 5;
			break;
		case 0x25: /* DCR H */
			ctx->h = op_dcr(ctx, ctx->h);
			ctx->cycles += 5;
			break;
		case 0x26: /* MVI H */
			ctx->h = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x27: /* DAA */
			op_daa(ctx);
			ctx->cycles += 4;
			break;
		case 0x28: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x29: /* DAD H */
			op_dad(ctx, get_hl(ctx));
			ctx->cycles += 10;
			break;
		case 0x2a: /* LHLD */
			set_hl(ctx, read_word(ctx, fetch_word(ctx)));
			ctx->cycles += 16;
			break;
		case 0x2b: /* DCX H */
			if (--ctx->l == 0xff)
				ctx->h--;
			ctx->cycles += 5;
			break;
		case 0x2c: /* INR L */
			ctx->l = op_inr(ctx, ctx->l);
			ctx->cycles += 5;
			break;
		case 0x2d: /* DCR L */
			ctx->l = op_dcr(ctx, ctx->l);
			ctx->cycles += 5;
			break;
		case 0x2e: /* MVI L */
			ctx->l = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x2f: /* CMA */
			ctx->a ^= 0xff;
			ctx->cycles += 4;
			break;
		case 0x30: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x31: /* LXI SP */
			ctx->sp = fetch_word(ctx);
			ctx->cycles += 10;
			break;
		case 0x32: /* STA */
			write_byte(ctx, fetch_word(ctx), ctx->a);
			ctx->cycles += 13;
			break;
		case 0x33: /* INX SP */
			ctx->sp++;
			ctx->cycles += 5;
			break;
		case 0x34: /* INR M */
			write_byte(ctx, get_hl(ctx), op_inr(ctx,
						read_byte(ctx, get_hl(ctx))));
			ctx->cycles += 10;
			break;
		case 0x35: /* DCR M */
			write_byte(ctx, get_hl(ctx), op_dcr(ctx,
						read_byte(ctx, get_hl(ctx))));
			ctx->cycles += 10;
			break;
		case 0x36: /* MVI M */
			write_byte(ctx, get_hl(ctx), fetch_byte(ctx));
			ctx->cycles += 10;
			break;
		case 0x37: /* STC */
			set_flag(ctx, FLAG_C);
			ctx->cycles += 4;
			break;
		case 0x38: /* NOP (Undocumented) */
			ctx->cycles += 4;
			break;
		case 0x39: /* DAD SP */
			op_dad(ctx, ctx->sp);
			ctx->cycles += 10;
			break;
		case 0x3a: /* LDA */
			ctx->a = read_byte(ctx, fetch_word(ctx));
			ctx->cycles += 13;
			break;
		case 0x3b: /* DCX SP */
			ctx->sp--;
			ctx->cycles += 5;
			break;
		case 0x3c: /* INR A */
			ctx->a = op_inr(ctx, ctx->a);
			ctx->cycles += 5;
			break;
		case 0x3d: /* DCR A */
			ctx->a = op_dcr(ctx, ctx->a);
			ctx->cycles += 5;
			break;
		case 0x3e: /* MVI A */
			ctx->a = fetch_byte(ctx);
			ctx->cycles += 7;
			break;
		case 0x3f: /* CMC */
			if (get_flag(ctx, FLAG_C) == 0)
				set_flag(ctx, FLAG_C);
			else
				clr_flag(ctx, FLAG_C);
			ctx->cycles += 4;
			break;
		case 0x40: /* MOV B, B */
			ctx->b = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x41: /* MOV B, C */
			ctx->b = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x42: /* MOV B, D */
			ctx->b = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x43: /* MOV B, E */
			ctx->b = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x44: /* MOV B, H */
			ctx->b = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x45: /* MOV B, L */
			ctx->b = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x46: /* MOV B, M */
			ctx->b = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x47: /* MOV B, A */
			ctx->b = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x48: /* MOV C, B */
			ctx->c = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x49: /* MOV C, C */
			ctx->c = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x4a: /* MOV C, D */
			ctx->c = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x4b: /* MOV C, E */
			ctx->c = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x4c: /* MOV C, H */
			ctx->c = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x4d: /* MOV C, L */
			ctx->c = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x4e: /* MOV C, M */
			ctx->c = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x4f: /* MOV C, A */
			ctx->c = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x50: /* MOV D, B */
			ctx->d = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x51: /* MOV D, C */
			ctx->d = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x52: /* MOV D, D */
			ctx->d = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x53: /* MOV D, E */
			ctx->d = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x54: /* MOV D, H */
			ctx->d = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x55: /* MOV D, L */
			ctx->d = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x56: /* MOV D, M */
			ctx->d = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x57: /* MOV D, A */
			ctx->d = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x58: /* MOV E, B */
			ctx->e = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x59: /* MOV E, C */
			ctx->e = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x5a: /* MOV E, D */
			ctx->e = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x5b: /* MOV E, E */
			ctx->e = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x5c: /* MOV E, H */
			ctx->e = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x5d: /* MOV E, L */
			ctx->e = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x5e: /* MOV E, M */
			ctx->e = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x5f: /* MOV E, A */
			ctx->e = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x60: /* MOV H, B */
			ctx->h = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x61: /* MOV H, C */
			ctx->h = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x62: /* MOV H, D */
			ctx->h = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x63: /* MOV H, E */
			ctx->h = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x64: /* MOV H, H */
			ctx->h = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x65: /* MOV H, L */
			ctx->h = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x66: /* MOV H, M */
			ctx->h = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x67: /* MOV H, A */
			ctx->h = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x68: /* MOV L, B */
			ctx->l = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x69: /* MOV L, C */
			ctx->l = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x6a: /* MOV L, D */
			ctx->l = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x6b: /* MOV L, E */
			ctx->l = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x6c: /* MOV L, H */
			ctx->l = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x6d: /* MOV L, L */
			ctx->l = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x6e: /* MOV L, M */
			ctx->l = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x6f: /* MOV L, A */
			ctx->l = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x70: /* MOV M, B */
			write_byte(ctx, get_hl(ctx), ctx->b);
			ctx->cycles += 7;
			break;
		case 0x71: /* MOV M, C */
			write_byte(ctx, get_hl(ctx), ctx->c);
			ctx->cycles += 7;
			break;
		case 0x72: /* MOV M, D */
			write_byte(ctx, get_hl(ctx), ctx->d);
			ctx->cycles += 7;
			break;
		case 0x73: /* MOV M, E */
			write_byte(ctx, get_hl(ctx), ctx->e);
			ctx->cycles += 7;
			break;
		case 0x74: /* MOV M, H */
			write_byte(ctx, get_hl(ctx), ctx->h);
			ctx->cycles += 7;
			break;
		case 0x75: /* MOV M, L */
			write_byte(ctx, get_hl(ctx), ctx->l);
			ctx->cycles += 7;
			break;
		case 0x76: /* HLT */
			ctx->halted = 1;
			ctx->cycles += 7;
			break;
		case 0x77: /* MOV M, A */
			write_byte(ctx, get_hl(ctx), ctx->a);
			ctx->cycles += 7;
			break;
		case 0x78: /* MOV A, B */
			ctx->a = ctx->b;
			ctx->cycles += 5;
			break;
		case 0x79: /* MOV A, C */
			ctx->a = ctx->c;
			ctx->cycles += 5;
			break;
		case 0x7a: /* MOV A, D */
			ctx->a = ctx->d;
			ctx->cycles += 5;
			break;
		case 0x7b: /* MOV A, E */
			ctx->a = ctx->e;
			ctx->cycles += 5;
			break;
		case 0x7c: /* MOV A, H */
			ctx->a = ctx->h;
			ctx->cycles += 5;
			break;
		case 0x7d: /* MOV A, L */
			ctx->a = ctx->l;
			ctx->cycles += 5;
			break;
		case 0x7e: /* MOV A, M */
			ctx->a = read_byte(ctx, get_hl(ctx));
			ctx->cycles += 7;
			break;
		case 0x7f: /* MOV A, A */
			ctx->a = ctx->a;
			ctx->cycles += 5;
			break;
		case 0x80: /* ADD B */
			op_add(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0x81: /* ADD C */
			op_add(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0x82: /* ADD D */
			op_add(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0x83: /* ADD E */
			op_add(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0x84: /* ADD H */
			op_add(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0x85: /* ADD L */
			op_add(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0x86: /* ADD M */
			op_add(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0x87: /* ADD A */
			op_add(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0x88: /* ADC B */
			op_adc(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0x89: /* ADC C */
			op_adc(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0x8a: /* ADC D */
			op_adc(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0x8b: /* ADC E */
			op_adc(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0x8c: /* ADC H */
			op_adc(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0x8d: /* ADC L */
			op_adc(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0x8e: /* ADC M */
			op_adc(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0x8f: /* ADC A */
			op_adc(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0x90: /* SUB B */
			op_sub(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0x91: /* SUB C */
			op_sub(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0x92: /* SUB D */
			op_sub(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0x93: /* SUB E */
			op_sub(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0x94: /* SUB H */
			op_sub(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0x95: /* SUB L */
			op_sub(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0x96: /* SUB M */
			op_sub(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0x97: /* SUB A */
			op_sub(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0x98: /* SBB B */
			op_sbb(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0x99: /* SBB C */
			op_sbb(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0x9a: /* SBB D */
			op_sbb(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0x9b: /* SBB E */
			op_sbb(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0x9c: /* SBB H */
			op_sbb(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0x9d: /* SBB L */
			op_sbb(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0x9e: /* SBB M */
			op_sbb(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0x9f: /* SBB A */
			op_sbb(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0xa0: /* ANA B */
			op_ana(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0xa1: /* ANA C */
			op_ana(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0xa2: /* ANA D */
			op_ana(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0xa3: /* ANA E */
			op_ana(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0xa4: /* ANA H */
			op_ana(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0xa5: /* ANA L */
			op_ana(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0xa6: /* ANA M */
			op_ana(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0xa7: /* ANA A */
			op_ana(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0xa8: /* XRA B */
			op_xra(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0xa9: /* XRA C */
			op_xra(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0xaa: /* XRA D */
			op_xra(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0xab: /* XRA E */
			op_xra(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0xac: /* XRA H */
			op_xra(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0xad: /* XRA L */
			op_xra(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0xae: /* XRA M */
			op_xra(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0xaf: /* XRA A */
			op_xra(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0xb0: /* ORA B */
			op_ora(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0xb1: /* ORA C */
			op_ora(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0xb2: /* ORA D */
			op_ora(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0xb3: /* ORA E */
			op_ora(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0xb4: /* ORA H */
			op_ora(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0xb5: /* ORA L */
			op_ora(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0xb6: /* ORA M */
			op_ora(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0xb7: /* ORA A */
			op_ora(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0xb8: /* CMP B */
			op_cmp(ctx, ctx->b);
			ctx->cycles += 4;
			break;
		case 0xb9: /* CMP C */
			op_cmp(ctx, ctx->c);
			ctx->cycles += 4;
			break;
		case 0xba: /* CMP D */
			op_cmp(ctx, ctx->d);
			ctx->cycles += 4;
			break;
		case 0xbb: /* CMP E */
			op_cmp(ctx, ctx->e);
			ctx->cycles += 4;
			break;
		case 0xbc: /* CMP H */
			op_cmp(ctx, ctx->h);
			ctx->cycles += 4;
			break;
		case 0xbd: /* CMP L */
			op_cmp(ctx, ctx->l);
			ctx->cycles += 4;
			break;
		case 0xbe: /* CMP M */
			op_cmp(ctx, read_byte(ctx, get_hl(ctx)));
			ctx->cycles += 7;
			break;
		case 0xbf: /* CMP A */
			op_cmp(ctx, ctx->a);
			ctx->cycles += 4;
			break;
		case 0xc0: /* RNZ */
			if (get_flag(ctx, FLAG_Z) == 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xc1: /* POP B */
			set_bc(ctx, pop_word(ctx));
			ctx->cycles += 10;
			break;
		case 0xc2: /* JNZ */
			if (get_flag(ctx, FLAG_Z) == 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xc3: /* JMP */
			op_jmp(ctx);
			ctx->cycles += 10;
			break;
		case 0xc4: /* CNZ */
			if (get_flag(ctx, FLAG_Z) == 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xc5: /* PUSH B */
			push_word(ctx, get_bc(ctx));
			ctx->cycles += 11;
			break;
		case 0xc6: /* ADI */
			op_add(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xc7: /* RST 0 */
			op_rst(ctx, 0x0000);
			ctx->cycles += 11;
			break;
		case 0xc8: /* RZ */
			if (get_flag(ctx, FLAG_Z) != 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xc9: /* RET */
			op_ret(ctx);
			ctx->cycles += 10;
			break;
		case 0xca: /* JZ */
			if (get_flag(ctx, FLAG_Z) != 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xcb: /* JMP (Undocumented) */
			op_jmp(ctx);
			ctx->cycles += 10;
			break;
		case 0xcc: /* CZ */
			if (get_flag(ctx, FLAG_Z) != 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xcd: /* CALL */
			op_call(ctx);
			ctx->cycles += 17;
			break;
		case 0xce: /* ACI */
			op_adc(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xcf: /* RST 1 */
			op_rst(ctx, 0x008);
			ctx->cycles += 11;
			break;
		case 0xd0: /* RNC */
			if (get_flag(ctx, FLAG_C) == 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xd1: /* POP D */
			set_de(ctx, pop_word(ctx));
			ctx->cycles += 10;
			break;
		case 0xd2: /* JNC */
			if (get_flag(ctx, FLAG_C) == 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xd3: /* OUT */
			ctx->io_outb(ctx->opaque, fetch_byte(ctx), ctx->a);
			ctx->cycles += 10;
			break;
		case 0xd4: /* CNC */
			if (get_flag(ctx, FLAG_C) == 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xd5: /* PUSH D */
			push_word(ctx, get_de(ctx));
			ctx->cycles += 11;
			break;
		case 0xd6: /* SUI */
			op_sub(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xd7: /* RST 2 */
			op_rst(ctx, 0x0010);
			ctx->cycles += 11;
			break;
		case 0xd8: /* RC */
			if (get_flag(ctx, FLAG_C) != 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xd9: /* RET (Undocumented) */
			op_ret(ctx);
			ctx->cycles += 10;
			break;
		case 0xda: /* JC */
			if (get_flag(ctx, FLAG_C) != 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xdb: /* IN */
			ctx->a = ctx->io_inb(ctx, fetch_byte(ctx));
			ctx->cycles += 10;
			break;
		case 0xdc: /* CC */
			if (get_flag(ctx, FLAG_C) != 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xdd: /* CALL (Undocumented) */
			op_call(ctx);
			ctx->cycles += 17;
			break;
		case 0xde: /* SBI */
			op_sbb(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xdf: /* RST 3 */
			op_rst(ctx, 0x0018);
			ctx->cycles += 11;
			break;
		case 0xe0: /* RPO */
			if (get_flag(ctx, FLAG_P) == 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xe1: /* POP H */
			set_hl(ctx, pop_word(ctx));
			ctx->cycles += 10;
			break;
		case 0xe2: /* JPO */
			if (get_flag(ctx, FLAG_P) == 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xe3: /* XTHL */
			op_xthl(ctx);
			ctx->cycles += 18;
			break;
		case 0xe4: /* CPO */
			if (get_flag(ctx, FLAG_P) == 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xe5: /* PUSH H */
			push_word(ctx, get_hl(ctx));
			ctx->cycles += 11;
			break;
		case 0xe6: /* ANI */
			op_ana(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xe7: /* RST 4 */
			op_rst(ctx, 0x0020);
			ctx->cycles += 11;
			break;
		case 0xe8: /* RPE */
			if (get_flag(ctx, FLAG_P) != 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xe9: /* PCHL */
			ctx->pc = get_hl(ctx);
			ctx->cycles += 5;
			break;
		case 0xea: /* JPE */
			if (get_flag(ctx, FLAG_P) != 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xeb: /* XCHG */
			op_xchg(ctx);
			ctx->cycles += 5;
			break;
		case 0xec: /* CPE */
			if (get_flag(ctx, FLAG_P) != 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xed: /* CALL (Undocumented) */
			op_call(ctx);
			ctx->cycles += 17;
			break;
		case 0xee: /* XRI */
			op_xra(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xef: /* RST 5 */
			op_rst(ctx, 0x0028);
			ctx->cycles += 11;
			break;
		case 0xf0: /* RP */
			if (get_flag(ctx, FLAG_S) == 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xf1: /* POP PSW */
			set_psw(ctx, pop_word(ctx));
			/* Make sure the unused bits are set. */
			ctx->f |= 0x02;
			ctx->f &= ~0x08;
			ctx->f &= ~0x20;
			ctx->cycles += 10;
			break;
		case 0xf2: /* JP */
			if (get_flag(ctx, FLAG_S) == 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xf3: /* DI */
			ctx->int_enable = 0;
			ctx->cycles += 4;
			break;
		case 0xf4: /* CP */
			if (get_flag(ctx, FLAG_S) == 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xf5: /* PUSH PSW */
			/* Make sure the unused bits are set. */
			ctx->f |= 0x02;
			ctx->f &= ~0x08;
			ctx->f &= ~0x20;
			push_word(ctx, get_psw(ctx));
			ctx->cycles += 11;
			break;
		case 0xf6: /* ORI */
			op_ora(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xf7: /* RST 6 */
			op_rst(ctx, 0x0030);
			ctx->cycles += 11;
			break;
		case 0xf8: /* RM */
			if (get_flag(ctx, FLAG_S) != 0) {
				op_ret(ctx);
				ctx->cycles += 11;
			} else {
				ctx->cycles += 5;
			}
			break;
		case 0xf9: /* SPHL */
			ctx->sp = get_hl(ctx);
			ctx->cycles += 5;
			break;
		case 0xfa: /* JM */
			if (get_flag(ctx, FLAG_S) != 0)
				op_jmp(ctx);
			else
				ctx->pc += 2;
			ctx->cycles += 10;
			break;
		case 0xfb: /* EI */
			ctx->int_enable = 1;
			ctx->cycles += 4;
			break;
		case 0xfc: /* CM */
			if (get_flag(ctx, FLAG_S) != 0) {
				op_call(ctx);
				ctx->cycles += 17;
			} else {
				ctx->pc += 2;
				ctx->cycles += 11;
			}
			break;
		case 0xfd: /* CALL (Undocumented) */
			op_call(ctx);
			ctx->cycles += 17;
			break;
		case 0xfe: /* CPI */
			op_cmp(ctx, fetch_byte(ctx));
			ctx->cycles += 7;
			break;
		case 0xff: /* RST 7 */
			op_rst(ctx, 0x0038);
			ctx->cycles += 11;
			break;
		default:
			/* Unreachable */
			break;
	}
}

