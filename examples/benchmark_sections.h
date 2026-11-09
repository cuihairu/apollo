/**
 * @file benchmark_sections.h
 * @brief 基准段共享计时器与入口声明（benchmark 拆分单元共用）
 */

#pragma once

#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>

// 类体沿用拆分前全文件的 using（调用点保持不限定名）
using namespace std;
using namespace std::chrono;

//==============================================================================
// 性能测试辅助类
//==============================================================================

class Benchmark {
public:
    struct Result {
        string name;
        int64_t iterations;
        double elapsedMs;
        double opsPerSecond;
        double avgTimeNs;

        void print() const {
            cout << left << setw(30) << name
                 << right << setw(12) << iterations
                 << setw(12) << fixed << setprecision(2) << elapsedMs << " ms"
                 << setw(15) << setprecision(0) << opsPerSecond << " ops/s"
                 << setw(12) << setprecision(2) << avgTimeNs << " ns/op"
                 << endl;
        }
    };

    static Result run(const string& name, int iterations, auto&& func) {
        // 预热
        for (int i = 0; i < std::min(100, iterations / 10); ++i) {
            func();
        }

        // 正式测试
        auto start = high_resolution_clock::now();

        for (int i = 0; i < iterations; ++i) {
            func();
        }

        auto end = high_resolution_clock::now();
        auto elapsed = duration_cast<microseconds>(end - start).count();

        Result result;
        result.name = name;
        result.iterations = iterations;
        result.elapsedMs = elapsed / 1000.0;
        result.opsPerSecond = (iterations * 1000000.0) / elapsed;
        result.avgTimeNs = (elapsed * 1000.0) / iterations;

        return result;
    }

    static void printHeader() {
        cout << left << setw(30) << "Benchmark"
             << right << setw(12) << "Iterations"
             << setw(12) << "Time"
             << setw(15) << "Ops/sec"
             << setw(12) << "Time/op"
             << endl;
        cout << string(79, '-') << endl;
    }
};

//==============================================================================
// 基准段入口（各拆分单元定义，main 按 CLI 选段调用）
//==============================================================================

void benchmarkBasicOperations();
void benchmarkContainers();
void benchmarkThreading();
void benchmarkAOI();
void benchmarkMessageProcessing();
void benchmarkMemoryPool();
void benchmarkLocks();
void benchmarkThreeModels();
void benchmarkAcceptanceFace();
