
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

static inline void
op_add(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_adc(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_sub(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_sbb(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_ana(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_xra(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_ora(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
}

static inline void
op_cmp(struct i8080 *ctx, uint8_t val)
{
	/* TODO */
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
