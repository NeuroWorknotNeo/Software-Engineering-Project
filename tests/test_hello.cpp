#include <gtest/gtest.h>

#include "hello/hello.hpp"

TEST(HelloTest, ReturnsHelloWorld) { EXPECT_EQ(hello::greeting(), "Hello World!"); }

TEST(HelloTest, IsNotEmpty) { EXPECT_FALSE(hello::greeting().empty()); }
