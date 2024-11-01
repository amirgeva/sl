#pragma once

#include "types.h"
#include <datastr/vector.h>

typedef struct function_address_
{
	word start,stop;
} FunctionAddress;

void opt_init(Vector* functions, Vector* global_addresses);
void opt_exec(const char* filename);
void opt_shut();
