
#include <stdint.h>
#include <stddef.h>

#include "i8080.h"

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
}

