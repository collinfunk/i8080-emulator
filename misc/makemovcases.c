
#include <ctype.h>
#include <stdio.h>

/*
 * MOV dst8 src8 instructions given in the form:
 * 01 | dst | src
 */

static int bits_to_reg (int);
static void print_hlt (void);
static void print_mov_r8_mem (int);
static void print_mov_mem_r8 (int);
static void print_mov_r8_r8 (int);

int
main (void)
{
  int i;

  for (i = 0x40; i < 0x80; ++i)
    {
      if (i == 0x76)
        print_hlt ();
      else if ((i & 7) == 6)
        print_mov_r8_mem (i);
      else if (((i >> 3) & 7) == 6)
        print_mov_mem_r8 (i);
      else
        print_mov_r8_r8 (i);
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
print_hlt (void)
{
  printf ("\t\tcase 0x76: /* HLT */\n");
  printf ("\t\t\tctx->halted = 1;\n");
  printf ("\t\t\tctx->cycles += 7;\n");
  printf ("\t\t\tbreak;\n");
}

static void
print_mov_r8_mem (int val)
{
  int r;

  r = bits_to_reg ((val >> 3) & 7);
  printf ("\t\tcase 0x%02x: /* MOV %c, M */\n", val, toupper (r));
  printf ("\t\t\tctx->%c = read_byte(ctx, get_hl(ctx));\n", r);
  printf ("\t\t\tctx->cycles += 7;\n");
  printf ("\t\t\tbreak;\n");
}

static void
print_mov_mem_r8 (int val)
{
  int r;

  r = bits_to_reg (val & 7);
  printf ("\t\tcase 0x%02x: /* MOV M, %c */\n", val, toupper (r));
  printf ("\t\t\twrite_byte(ctx, get_hl(ctx), ctx->%c);\n", r);
  printf ("\t\t\tctx->cycles += 7;\n");
  printf ("\t\t\tbreak;\n");
}

static void
print_mov_r8_r8 (int val)
{
  int rd, rs;

  rs = bits_to_reg (val & 7);
  rd = bits_to_reg ((val >> 3) & 7);
  printf ("\t\tcase 0x%02x: /* MOV %c, %c */\n", val, toupper (rd),
          toupper (rs));
  printf ("\t\t\tctx->%c = ctx->%c;\n", rd, rs);
  printf ("\t\t\tctx->cycles += 5;\n");
  printf ("\t\t\tbreak;\n");
}
