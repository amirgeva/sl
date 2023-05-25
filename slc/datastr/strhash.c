#include <string.h>
#include "strhash.h"
#include "memory.h"

#define MAX_LENGTH 16

void copy_n(char* dst, const char* src, int n)
{
	for (int i = 0; i < n; ++i, ++src, ++dst)
	{
		*dst = *src;
		if (*src == 0) break;
	}
}

int compare_n(const char* a, const char* b, int n)
{
	for (int i = 0; i < n; ++i)
	{
		if (*a < *b) return -1;
		if (*a > *b) return 1;
		if (*a == 0 && *b == 0) return 0;
	}
	return 0;
}

static byte checksum(const char* text)
{
	const byte* bytes = (const byte*)text;
	byte res = 0xFF;
	for (; *bytes; ++bytes)
		res += *bytes;
	return res;
}

typedef struct text_node
{
	word id;
	char text[MAX_LENGTH];
	struct text_node* next;
} TextNode;

typedef TextNode* TextNodePtr;

static TextNodePtr Root[256];
static word LastID = 0;

static word allocate_id()
{
	return ++LastID;
}

static void destroy_node(TextNodePtr ptr)
{
	if (!ptr) return;
	destroy_node(ptr->next);
	release(ptr);
}

void sh_init()
{
	LastID = 0;
	for (word i = 0; i < 256; ++i)
		Root[i] = 0;
}

void sh_shut()
{
	for (word i = 0; i < 256; ++i)
	{
		destroy_node(Root[i]);
		Root[i] = 0;
	}
}

word sh_get(const char* text)
{
	byte sum = checksum(text);
	TextNodePtr next = Root[sum];
	if (next)
	{
		for (TextNodePtr cur = next; cur; cur = cur->next)
		{
			if (compare_n(text, cur->text, MAX_LENGTH) == 0) return cur->id;
		}
	}
	Root[sum] = allocate(sizeof(TextNode));
	Root[sum]->id = allocate_id();
	copy_n(Root[sum]->text, text, MAX_LENGTH);
	Root[sum]->next = next;
	return Root[sum]->id;
}

byte sh_text(char* text, word sh)
{
	for (word i = 0; i < 256; ++i)
	{
		TextNodePtr ptr = Root[i];
		while (ptr)
		{
			if (ptr->id == sh)
			{
				copy_n(text, ptr->text, MAX_LENGTH);
				return 1;
			}
			ptr = ptr->next;
		}
	}
	return 0;
}

word sh_temp()
{
	return allocate_id();
}
