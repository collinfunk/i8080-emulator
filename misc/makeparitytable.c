
#include <stdio.h>

int count_set_bits (int, int);
int is_even (int);

int
main (void)
{
  int i, set, parity;

  printf ("static const uint8_t parity_table[256] = {\n");
  for (i = 1; i <= 256; ++i)
    {
      set = count_set_bits (i - 1, 8);
      parity = is_even (set);
      switch (i & 15)
        {
        case 0:
          printf ("%d,\n", parity);
          break;
        case 1:
          printf ("\t%d, ", parity);
          break;
        default:
          printf ("%d, ", parity);
          break;
        }
    }
  printf ("};\n");

  return 0;
}

int
count_set_bits (int val, int nbits)
{
  int i, count;

  if (val == 0 || nbits == 0)
    return 0;

  for (i = count = 0; i < 8; ++i)
    {
      count += (val & 1) == 0 ? 0 : 1;
      val = (unsigned int) val >> 1;
    }

  return count;
}

int
is_even (int i)
{
  return (i & 1) != 0 ? 0 : 1;
}
