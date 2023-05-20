#pragma once

#include "types.h"

typedef struct vector_ Vector;

Vector*		vector_new(word element_size);
void		vector_shut(Vector*);
void		vector_init(Vector*, word element_size);
void		vector_shut(Vector*);
word		vector_size(Vector*);
void		vector_clear(Vector*);
void		vector_resize(Vector*, word size);
void		vector_push(Vector*, void* element);
void		vector_pop(Vector*, void* element);
void		vector_get(Vector*, word index, void* element);
void*		vector_access(Vector*, word index);
