#include "memory.h"


static word total_allocated = 0;
static word heap=0x7000;
static word free_block = 0;

byte check_heap(word size)
{
	return (heap - size) > 0;
}

#ifdef WIN32

byte static_heap[0x7000];

#else

static byte* static_heap = (byte*)0x8000;

#endif

word get_offset(void* ptr)
{
	return (word)(((byte*)ptr) - static_heap);
}

void* get_pointer(word offset)
{
	return static_heap + offset;
}

void init_alloc()
{}



void* allocate(word size)
{
	if (!check_heap(size)) return 0;
	heap -= (size + 2);
	word* header = (word*)get_pointer(heap);
	*header = size;
	total_allocated += size;
	return header + 1;
}

void	release(void* ptr)
{
	if (!ptr) return;
	word* header = (word*)ptr;
	--header;
	total_allocated -= *header;
	word offset = get_offset(header);
	if (offset == heap)
		heap += *header + 2;
	else
	{
		header[1] = free_block;
		free_block = offset;
	}
}

unsigned get_total_allocated()
{
	return total_allocated;
}

