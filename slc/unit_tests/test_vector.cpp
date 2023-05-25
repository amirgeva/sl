#include "gtest/gtest.h"
extern "C" {
#include <vector.h>
#include <memory.h>
#include <utils.h>
}



TEST(datastr, mult)
{
	for (word a = 0; a < 25; ++a)
	{
		for (word b = 0; b < 35; ++b)
		{
			EXPECT_EQ(multiply(a, b), a * b);
		}
	}
}


TEST(datastr, vector)
{
	void* null = 0;
	Vector* empty = 0;
	Vector* v = vector_new(sizeof(int));
	EXPECT_NE(v, empty);
	EXPECT_EQ(vector_size(v), 0);
	for(int i = 3;i<8;++i)
		vector_push(v, &i);
	EXPECT_EQ(vector_size(v), 5);
	for (word i = 0; i < vector_size(v); ++i)
	{
		int value;
		vector_get(v, i, &value);
		EXPECT_EQ(value, 3 + i);
	}
	EXPECT_EQ(vector_access(v, 20), null);
	for (unsigned i = 4; vector_size(v) > 0; --i)
	{
		vector_pop(v, 0);
		EXPECT_EQ(vector_size(v), i);
	}
	// Force multiple reallocs
	for (int i = 0; i < 1000; ++i)
		vector_push(v, &i);
	vector_shut(v);
	// Check for leaks
	EXPECT_EQ(get_total_allocated(), 0);
}

int main(int argc, char* argv[])
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

