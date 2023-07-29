#include "gtest/gtest.h"
extern "C" {
#include <vector.h>
#include <memory.h>
#include <utils.h>
}

struct Initializer
{
	Initializer()
	{
		alloc_init();
	}
	~Initializer()
	{
		alloc_shut();
	}
};

void* null=0;

TEST(memory, realloc)
{
	Vector* v=vector_new(1);
	EXPECT_TRUE(v);
	for (word a = 0; a < 0x1000; ++a)
	{
		EXPECT_TRUE(vector_push(v,&a));
		EXPECT_TRUE(verify_heap());
	}
}

TEST(memory, alloc_free)
{
	for (word a = 0; a < 0x8000; ++a)
	{
		void* ptr=allocate(256);
		EXPECT_NE(ptr,null);
		release(ptr);
	}
}

TEST(memory, edge_cases)
{
	EXPECT_EQ(allocate(0),null);
	EXPECT_EQ(allocate(0x8000),null);
}

int main(int argc, char* argv[])
{
	Initializer init;
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}

