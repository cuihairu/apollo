/**
 * @file benchmark.cpp
 * @brief Apollo 框架性能测试
 *
 * 测试各模块的性能表现
 */
#include "benchmark_sections.h"

#include <iostream>

using namespace std;
using namespace std::chrono;

//==============================================================================
// 主程序
//==============================================================================

int main(int argc, char** argv) {
    // --models：只跑三模型场景段（微基准段全量约 10+ 分钟，选段供快速
    // 验证与静时机复测）；--acceptance：只跑受理面段（校准复测走这条，
    // 免等场景段——§5 环境纪律的对照基线复测口径）
    const bool models_only =
        (argc > 1 && std::string(argv[1]) == "--models");
    if (models_only) {
        benchmarkThreeModels();
        benchmarkAcceptanceFace();
        return 0;
    }
    if (argc > 1 && std::string(argv[1]) == "--acceptance") {
        benchmarkAcceptanceFace();
        return 0;
    }

    cout << "========================================" << endl;
    cout << "=== Apollo Framework Benchmark ===" << endl;
    cout << "========================================" << endl;
    cout << "\nCompiler: ";
#if defined(_MSC_VER)
    cout << "MSVC " << _MSC_VER << endl;
#elif defined(__GNUC__)
    cout << "GCC " << __GNUC__ << "." << __GNUC_MINOR__ << "." << __GNUC_PATCHLEVEL__ << endl;
#elif defined(__clang__)
    cout << "Clang " << __clang_major__ << "." << __clang_minor__ << "." << __clang_PATCHLEVEL__ << endl;
#else
    cout << "Unknown" << endl;
#endif

    cout << "Build: " << (sizeof(void*) == 8 ? "x64" : "x86") << endl;
    cout << "Date: " << __DATE__ << " " << __TIME__ << endl;

    benchmarkBasicOperations();
    benchmarkContainers();
    benchmarkThreading();
    benchmarkAOI();
    benchmarkMessageProcessing();
    benchmarkMemoryPool();
    benchmarkLocks();
    benchmarkThreeModels();
    benchmarkAcceptanceFace();

    cout << "\n========================================" << endl;
    cout << "=== Benchmark Complete ===" << endl;
    cout << "========================================" << endl;

    return 0;
}
