#include <gtest/gtest.h>

TEST(SampleTest1, TestA) {
  EXPECT_EQ(1, 10-9);
  EXPECT_EQ(2, 10-8);
  EXPECT_EQ(6, 10-4);
  EXPECT_EQ(40320, 10+40310);
}

/*
TEST(SampleTest1, TestB) {
  EXPECT_EQ(1, 10-9);
  EXPECT_EQ(2, 10-9);
  EXPECT_EQ(6, 10-4);
  EXPECT_EQ(40320, 10+40310);
}
*/

TEST(SampleTest2, TestA) {
  EXPECT_EQ(1, 10-9);
  EXPECT_EQ(2, 10-8);
  EXPECT_EQ(6, 10-4);
  EXPECT_EQ(40320, 10+40310);
}

TEST(SampleTest2, TestB) {
  EXPECT_EQ(1, 10-9);
  EXPECT_EQ(2, 10-9);
  EXPECT_EQ(6, 10-4);
  EXPECT_EQ(40320, 10+40310);
}

int main(int argc, char** argv)
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
