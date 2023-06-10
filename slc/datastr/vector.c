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
	if (!res) return 0;
	vector_init(res, element_size);
	return res;
}

byte		vector_init(Vector* v, word element_size)
{
	if (!v) return 0;
	v->element_size = element_size;
	v->capacity = 0;
	v->size = 0;
	v->data = 0;
	return 1;
}

void		vector_shut(Vector* v)
{
	if (v->data)
		release(v->data);
	if (v)
		release(v);
}

static byte vector_reallocate(Vector* v, word new_size)
{
	if (!v) return 0;
	char* buffer = (char*)allocate(multiply(new_size, v->element_size));
	if (!buffer) return 0;
	if (v->data)
	{
		copy(buffer, v->data, multiply(v->size, v->element_size));
		release(v->data);
	}
	v->data = buffer;
	v->capacity = new_size;
	return 1;
}

word		vector_size(Vector* v)
{
	if (!v) return 0;
	return v->size;
}

byte		vector_clear(Vector* v)
{
	if (!v) return 0;
	v->size = 0;
	return 1;
}

byte		vector_resize(Vector* v, word size)
{
	if (!v) return 0;
	if (size > v->capacity)
	{
		if (!vector_reallocate(v, size)) return 0;
	}
	v->size = size;
	return 1;
}

byte		vector_push(Vector* v, void* element)
{
	if (!v) return 0;
	if (v->size >= v->capacity)
	{
		word new_size = 10;
		if (v->size > 0)
			new_size = v->size << 1;
		if (!vector_reallocate(v, new_size)) return 0;
	}
	copy(v->data + multiply(v->size, v->element_size), element, v->element_size);
	v->size++;
	return 1;
}

byte		vector_pop(Vector* v, void* element)
{
	if (!v) return 0;
	if (v->size > 0)
	{
		v->size--;
		if (element)
			copy(element, v->data + multiply(v->size, v->element_size), v->element_size);
		return 1;
	}
	return 0;
}

void*		vector_access(Vector* v, word index)
{
	if (!v) return 0;
	if (index >= v->size) return 0;
	return v->data + multiply(index, v->element_size);
}

byte		vector_set(Vector* v, word index, void* element)
{
	if (v)
	{
		void* dst = vector_access(v, index);
		if (element && dst)
		{
			copy(dst, element, v->element_size);
			return 1;
		}
	}
	return 0;
}

byte		vector_get(Vector* v, word index, void* element)
{
	if (!v) return 0;
	void* src = vector_access(v, index);
	if (element && src)
	{
		copy(element, src, v->element_size);
		return 1;
	}
	return 0;
}

byte		vector_erase(Vector* v, word index)
{
	if (!v) return 0;
	if (index >= v->size) return 0;
	if (index < (v->size - 1))
	{
		copy(vector_access(v, index),
			 vector_access(v, index + 1),
			 multiply(v->element_size, (v->size - index - 1)));
	}
	vector_resize(v, v->size - 1);
	return 1;
}

byte		vector_erase_range(Vector* v, word begin, word end)
{
	if (!v) return 0;
	if (begin >= v->size || end>v->size || begin>=end) return 0;
	word n=end-begin;
	if (end < v->size)
	{
		copy(vector_access(v, begin), 
			 vector_access(v, end), 
			 multiply(v->element_size, (v->size - end)));
	}
	vector_resize(v, v->size-n);
	return 1;
}