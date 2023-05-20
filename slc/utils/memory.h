#pragma once

#include "types.h"

void		init_alloc();
void*		allocate(word size);
void		release(void* ptr);
unsigned	get_total_allocated();
void		print_leaked();
