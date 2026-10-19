cmake_minimum_required(VERSION 3.20)

# InterfacesApp 可执行文件（todo L45 批④）：平台/渠道回调的 HTTP/HTTPS
# 入口进程壳。只依赖 net M1 内核三件（link 缝 + TLS/WS 底座与编解码），
# 不链 gateway——与 gateway 游戏入口互不进入对方的连接模型（docs/index.md
# §接口分工）。ws 出站业务回执面 L50 接线时再开。
add_executable(interfaces
    src/main.cpp
    src/http_ingress.cpp
)

target_include_directories(interfaces
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(interfaces
    PRIVATE
        apollo::base
        apollo::core
        apollo::runtime
        apollo::net_link
        apollo_net_tls
        apollo_net_ws
)

target_compile_features(interfaces PRIVATE cxx_std_20)

# Windows：link TCP/TLS 底座不进 M1 批——入口工厂退回 null 桩（运行期
# 壳仍可起，只是不受理连接）；ws2_32 预留给底座启用批。
if(WIN32)
    target_link_libraries(interfaces PRIVATE ws2_32 mswsock)
endif()

apollo_package_crash_handler(interfaces)
