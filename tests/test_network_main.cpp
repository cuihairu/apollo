/**
 * @file test_network_main.cpp
 * @brief 网络层测试入口
 */

int main(int argc, char* argv[]);

// 声明测试函数
int test_buffer_main();
int test_network_main();

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "buffer") {
            return test_buffer_main();
        } else if (arg == "network") {
            return test_network_main();
        } else {
            std::cout << "Usage: " << argv[0] << " [buffer|network]" << std::endl;
            return 1;
        }
    }

    // 运行所有测试
    std::cout << "Running all network tests..." << std::endl;
    std::cout << std::endl;

    int result = 0;

    std::cout << "\n=== Buffer Tests ===" << std::endl;
    result |= test_buffer_main();

    std::cout << "\n=== Network Tests ===" << std::endl;
    result |= test_network_main();

    return result;
}
