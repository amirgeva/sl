#include <vector.h>
#include <memory.h>
#include <utils.h>

void copy(void* dest, void* src, int size)
{
	if (size > 0)
	{
		char* cdst = (char*)dest;
		char* csrc = (char*)src;
		for (int i = 0; i < size; ++i)
			*cdst++ = *csrc++;
	}
}

struct vector_
{
	word	element_size;
	word	capacity;
	word	size;
	char*	data;
};

Vector* vector_new(word element_size)
{
	Vector* res = (Vector*)allocate(sizeof(Vector));
	vector_init(res, element_size);
	return res;
}

void		vector_init(Vector* v, word element_size)
{
	v->element_size = element_size;
	v->capacity = 0;
	v->size = 0;
	v->data = 0;
}

void		vector_shut(Vector* v)
{
	release(v->data);
	release(v);
}

static void vector_reallocate(Vector* v, word new_size)
{
	char* buffer = (char*)allocate(multiply(new_size, v->element_size));
	if (v->data)
	{
		copy(buffer, v->data, multiply(v->size, v->element_size));
		release(v->data);
	}
	v->data = buffer;
	v->capacity = new_size;
}

word		vector_size(Vector* v)
{
	return v->size;
}

void		vector_clear(Vector* v)
{
	v->size = 0;
}

void		vector_resize(Vector* v, word size)
{
	if (size > v->capacity)
		vector_reallocate(v, size);
	v->size = size;
}

void		vector_push(Vector* v, void* element)
{
	if (v->size >= v->capacity)
	{
		word new_size = 10;
		if (v->size > 0)
			new_size = v->size << 1;
		vector_reallocate(v, new_size);
	}
	copy(v->data + multiply(v->size, v->element_size), element, v->element_size);
	v->size++;
}

void		vector_pop(Vector* v, void* element)
{
	if (v->size > 0)
	{
		v->size--;
		if (element)
			copy(element, v->data + multiply(v->size, v->element_size), v->element_size);
	}
}

void*		vector_access(Vector* v, word index)
{
	if (index >= v->size) return 0;
	return v->data + multiply(index, v->element_size);
}

void		vector_get(Vector* v, word index, void* element)
{
	void* src = vector_access(v, index);
	if (element && src)
		copy(element, src, v->element_size);
}
