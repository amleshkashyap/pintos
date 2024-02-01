#include <stdio.h>
#include <syscall.h>

int
main (int argc, char **argv)
{
  int i;
  printf("Executing echo\n");

  for (i = 0; i < argc; i++) {
    printf ("%x, %s\n", argv[i], argv[i]);
  }
  printf ("\n");

  return EXIT_SUCCESS;
}
