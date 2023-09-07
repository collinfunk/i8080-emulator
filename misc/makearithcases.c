
#include <ctype.h>
#include <stdio.h>

/*
 * Register/Memory to accumulator functions in the form:
 * 10 | op | src
 */

static const char optable[8][4]
    = { "add", "adc", "sub", "sbb", "ana", "xra", "ora", "cmp" };

static int bits_to_reg (int);
static void make_prototypes (void);
static void print_upper (const char *);
static void print_src_mem (const char *, int);
static void print_src_reg (const char *, int);

int
main (void)
{
  int i, d;
  char *s;
  make_prototypes ();

  for (i = 0x80; i < 0xc0; ++i)
    {
      d = bits_to_reg (i & 7);
      s = (char *) optable[(i >> 3) & 7];
      printf ("\t\tcase 0x%02x: /* ", i);
      print_upper (s);
      printf (" %c */\n", toupper (d));
      if ((i & 7) == 6)
        print_src_mem (s, d);
      else
        print_src_reg (s, d);
    }

  return 0;
}

static int
bits_to_reg (int val)
{
  switch (val)
    {
    case 0:
      return 'b';
    case 1:
      return 'c';
    case 2:
      return 'd';
    case 3:
      return 'e';
    case 4:
      return 'h';
    case 5:
      return 'l';
    case 6:
      return 'm';
    case 7:
      return 'a';
    }
}

static void
print_upper (const char *s)
{
  const char *p;
  for (p = s; *p != '\0'; ++p)
    putchar (toupper (*p));
}

static void
make_prototypes (void)
{
  int i;

  for (i = 0; i < 8; ++i)
    {
      printf ("static inline void\n");
      printf ("op_%s(struct i8080 *ctx, uint8_t val)\n", optable[i]);
      printf ("{\n\t/* TODO */\n");
      printf ("}\n\n");
    }
}

static void
print_src_mem (const char *op, int val)
{
  (void) val;
  printf ("\t\t\top_%s(ctx, read_byte(ctx, get_hl(ctx)));\n", op);
  printf ("\t\t\tctx->cycles += 7;\n");
  printf ("\t\t\tbreak;\n");
}

static void
print_src_reg (const char *op, int val)
{
  printf ("\t\t\top_%s(ctx, ctx->%c);\n", op, val);
  printf ("\t\t\tctx->cycles += 4;\n");
  printf ("\t\t\tbreak;\n");
}
