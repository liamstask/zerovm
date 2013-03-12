/*
 * this sample is a simple demonstration of zerovm.
 * the "hello world" program as usual.
 */
#include "include/zvmlib.h"

int main()
{
  /* write to stdout channel */
  printf("\033[1mhello from dev/stdout!\033[0m\n");

  /* write to stderr channel */
  fprintf(STDERR, "\033[36mhello from dev/stderr!\033[0m\n");

  return 0;
}
