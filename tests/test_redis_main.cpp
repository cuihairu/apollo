#include "gtest/gtest.h"
#include "apollo/db/redis.hpp"
#include <chrono>
#include <thread>

using namespace apollo::db;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}