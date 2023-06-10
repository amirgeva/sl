#pragma once

#include "types.h"

typedef struct vector_ Vector;

Vector*		vector_new(word element_size);
void		vector_shut(Vector*);
byte		vector_init(Vector*, word element_size);
word		vector_size(Vector*);
byte		vector_clear(Vector*);
byte		vector_resize(Vector*, word size);
byte		vector_push(Vector*, void* element);
byte		vector_pop(Vector*, void* element);
byte		vector_set(Vector* v, word index, void* element);
byte		vector_get(Vector*, word index, void* element);
void*		vector_access(Vector*, word index);
byte		vector_erase(Vector*, word index);
byte		vector_erase_range(Vector*, word begin, word end);
