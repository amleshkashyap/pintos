#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>

void * get_user_page (bool);
void free_user_page (void *);

#endif /* vm/page.h */
