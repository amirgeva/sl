#ifdef DEV
#include <stdio.h>
#include <stdlib.h>
#endif
#include "optimizer.h"
#include <vector.h>

Vector* line_starts=0;

void load_line_starts()
{
	FILE* f = fopen("line_offsets.log","r");
	if (f)
	{
		while (!feof(f))
		{
			word line, offset;
			int rc = fscanf(f, "%hx %hx", &line, &offset);
			if (rc == 2)
			{
				vector_push(line_starts, &offset);
			}
		}
		fclose(f);
	}
}

int main(int argc, char* argv[])
{
	line_starts = vector_new(2);
	vector_reserve(line_starts, 1024);
	load_line_starts();
	vector_shut(line_starts);
	return 0;
}