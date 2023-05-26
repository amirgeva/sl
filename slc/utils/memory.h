#pragma once

#include "types.h"

void		alloc_init();
void		alloc_shut();
void*		allocate(word size);
void		release(void* ptr);
unsigned	get_total_allocated();
unsigned	get_max_allocated();
void		print_leaked();
byte		verify_heap();

