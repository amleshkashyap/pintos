#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include <stdbool.h>

void syscall_init (void);
typedef int pid_t;


/* File Operations */
bool create (const char *, unsigned);
int open (const char *);
int filesize (int);
int read (int, void *, unsigned);
void seek (int, unsigned);
unsigned tell (int);
int write (int, const void *, unsigned);
void close (int);
bool remove (const char *);

/* Execution */
pid_t exec (const char *);
int wait (pid_t);
void exit (int);
void halt (void);

#endif /* userprog/syscall.h */
