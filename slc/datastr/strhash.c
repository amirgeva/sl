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

struct str_hash_
{
	TextNodePtr Root[256];
	word		LastID;
};

//static TextNodePtr Root[256];
//static word LastID = 0;

static word allocate_id(StrHash* sh)
{
	return ++sh->LastID;
}

static void destroy_node(TextNodePtr ptr)
{
	if (!ptr) return;
	destroy_node(ptr->next);
	release(ptr);
}

StrHash* sh_init()
{
	StrHash* sh = (StrHash*)allocate(sizeof(StrHash));
	sh->LastID = 0;
	for (word i = 0; i < 256; ++i)
		sh->Root[i] = 0;
	return sh;
}

void sh_shut(StrHash* sh)
{
	for (word i = 0; i < 256; ++i)
	{
		destroy_node(sh->Root[i]);
		sh->Root[i] = 0;
	}
	release(sh);
}

word sh_get(StrHash* sh, const char* text)
{
	byte sum = checksum(text);
	TextNodePtr next = sh->Root[sum];
	if (next)
	{
		for (TextNodePtr cur = next; cur; cur = cur->next)
		{
			if (compare_n(text, cur->text, MAX_LENGTH) == 0) return cur->id;
		}
	}
	sh->Root[sum] = allocate(sizeof(TextNode));
	sh->Root[sum]->id = allocate_id(sh);
	copy_n(sh->Root[sum]->text, text, MAX_LENGTH);
	sh->Root[sum]->next = next;
	return sh->Root[sum]->id;
}

byte sh_text(StrHash* sh, char* text, word id)
{
	for (word i = 0; i < 256; ++i)
	{
		TextNodePtr ptr = sh->Root[i];
		while (ptr)
		{
			if (ptr->id == id)
			{
				copy_n(text, ptr->text, MAX_LENGTH);
				return 1;
			}
			ptr = ptr->next;
		}
	}
	return 0;
}

word sh_temp(StrHash* sh)
{
	return allocate_id(sh);
}
