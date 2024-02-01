#include <syscall.h>
#include <stdio.h>

int main (int, char *[]);
void _start (int argc, char *argv[]);

void
_start (int argc, char *argv[]) 
{
  // ASSERT (argc == 500); 
  printf("_start () %d, %x\n", argc, argv);
  for (int i = 0; i < argc; i++) {
    printf("argv[%d]: %s\n", i, argv[i]);
  }
  
  exit (main (argc, argv));
}
