#include "utils.h"



word multiply(word a, word b)
{
	if (a == 0 || b == 0) return 0;
	word res = 0;
	while (b > 0)
	{
		if (b & 1) res += a;
		b = b >> 1;
		a = a << 1;
	}
	return res;
}

