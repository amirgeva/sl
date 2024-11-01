#include "memory.h"
#ifdef DEV
#include <stdio.h>
static FILE* logfile=0;
#endif

#define HEAP_SIZE 0x7000
static word max_allocated=0;
static word total_allocated = 0;
static word heap=0;
static word free_block = 0xFFFF;

byte check_heap(word size)
{
	return (heap + size) <= HEAP_SIZE;
}

#ifdef DEV

byte static_heap[HEAP_SIZE];

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

void alloc_init()
{
#ifdef DEV
	logfile=fopen("alloc.log","w");
#endif
}

void alloc_shut()
{
#ifdef DEV
	if (logfile)
		fclose(logfile);
#endif
}

void* find_free_block(word size)
{
	word best=0xFFFF;
	word best_prev=0xFFFF;
	word best_diff=0xFFFF;
	word prev = 0xFFFF;
	word current = free_block;
	word* best_ptr=0;
	while (current != 0xFFFF)
	{
		word* ptr=(word*)get_pointer(current);
		word block_size = *ptr;
		if (block_size >= size)
		{
			word diff=block_size-size;
			if (diff == 0 || diff >= (2 * sizeof(word)))
			{
				if (diff < best_diff)
				{
					best_ptr=ptr;
					best_diff = diff;
					best = current;
					best_prev = prev;
					if (best_diff==0) break;
				}
			}
		}
		prev=current;
		current=ptr[1];
	}
	if (best == 0xFFFF) return 0;
	if (best_diff > 0)
	{
		// Split block
		word* left_over = (word*)get_pointer(best+size);
		left_over[0]=*best_ptr - size;
		left_over[1]=best_ptr[1];
		best_ptr[1]=best+size;
	}
	if (best_prev != 0xFFFF)
	{
		word* prev_ptr = (word*)get_pointer(best_prev);
		prev_ptr[1]=best_ptr[1];
	}
	else // First block in the list
		free_block = best_ptr[1];
	return best_ptr;
}

void* allocate(word size)
{
	if (size==0) return 0;
	if (size<sizeof(word))
		size=sizeof(word); // minimum allocation is sizeof(word)
	size += sizeof(word);
	void* best_free_block=find_free_block(size);
	if (!best_free_block)
	{
		if (!check_heap(size)) return 0;
		best_free_block = get_pointer(heap);
		heap += size;
	}
	word* header = (word*)best_free_block;
	*header = size;
	total_allocated += size;
	if (total_allocated > max_allocated)
	{
		max_allocated=total_allocated;
	}
	word offset= get_offset(best_free_block);
#ifdef DEV
	if (logfile)
		fprintf(logfile,"A %hd %hd\n",size,offset);
#endif
	return header + 1;
}

void sort_free_blocks()
{
	// Bubble sort list by offset
	word swaps=1;
	while (swaps > 0)
	{
		swaps=0;
		word prev=0xFFFF;
		word current = free_block;
		while (current != 0xFFFF)
		{
			word* cur_ptr=(word*)get_pointer(current);
			word next=cur_ptr[1];
			if (next<current)
			{
				word* next_ptr=(word*)get_pointer(next);
				if (prev==0xFFFF) free_block=next;
				else
				{
					word* prev_ptr=(word*)get_pointer(prev);
					prev_ptr[1]=next;
				}
				cur_ptr[1]=next_ptr[1];
				next_ptr[1]=current;
				swaps++;
			}
			prev=current;
			current=next;
		}
	}
}

void unite_free_blocks()
{
	word current = free_block;
	while (current != 0xFFFF)
	{
		word* cur_ptr=(word*)get_pointer(current);
		while ((current + cur_ptr[0]) == cur_ptr[1]) // Consecutive blocks
		{
			word* next_ptr=(word*)get_pointer(cur_ptr[1]);
			cur_ptr[0]+=next_ptr[0];
			cur_ptr[1]=next_ptr[1];
		}
		current=cur_ptr[1];
	}
}

void defrag()
{
	sort_free_blocks();
	unite_free_blocks();
}

void	release(void* ptr)
{
	if (!ptr) return;
	word* header = (word*)ptr;
	--header;
#ifdef DEV
	if (logfile)
		fprintf(logfile,"R %hd %hd\n",header[0],get_offset(header));
#endif
	word size=*header;
	total_allocated -= size;
	word offset = get_offset(header);
	if ((offset + size) == heap)
	{
		heap -= size;
	}
	else
	{
		header[1] = free_block;
		free_block = offset;
		defrag();
	}
}

byte verify_heap()
{
	word next_free_block=free_block;
	word sum_free_blocks=0;
	while (next_free_block != 0xFFFF)
	{
		word* block=(word*)get_pointer(next_free_block);
		if (!block) break;
		sum_free_blocks+=*block;
		next_free_block=block[1];
	}
	return (total_allocated + sum_free_blocks == heap) ? 1 : 0;
}

unsigned get_total_allocated()
{
	return total_allocated;
}

unsigned get_max_allocated()
{
	return max_allocated;
}

void print_leaked()
{
#ifdef DEV
	printf("%d bytes leaked\n", total_allocated);
#endif

}
