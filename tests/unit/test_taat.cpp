#include <algorithm>
#include <random>
#include <sstream>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <irkit/index/vector_inverted_list.hpp>

namespace {

TEST(taat, sanity_test)
{
    ASSERT_EQ(1, 1);
}

};  // namespace

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
