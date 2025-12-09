#include "gtest/gtest.h"
#include "apollo/net/protobuf_message.hpp"
#include "apollo/net/rpc.hpp"
#include "apollo/net/reactor.hpp"
#include <thread>
#include <chrono>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}