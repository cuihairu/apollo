cmake_minimum_required(VERSION 3.20)

# LoggerApp——日志收集进程壳（ADR-013 立项，term-contract §1.3 契约代码
# 标识 LoggerApp；目录/二进制名 logger 同 machined 运维件口径）。
# v1 = 只读汇聚：scan 检索子命令 + follow 跟随（tail 拉，logger-app.md
# §3(a)）；push 出口留接缝不实现（collector 面 M1 后）。
add_executable(logger
    src/main.cpp
    src/logger_app.cpp
    src/structured_line.cpp
)

target_include_directories(logger
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(logger
    PRIVATE
        apollo::core
        apollo::runtime
)

target_compile_features(logger PRIVATE cxx_std_20)
apollo_package_crash_handler(logger)

if(WIN32)
    target_link_libraries(logger PRIVATE ws2_32)
endif()
if(MSVC)
    target_compile_options(logger PRIVATE /utf-8)
endif()
