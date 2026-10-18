#pragma once

// world 共用二进制分角色（L27）：manager = 管理端（缺省——无参数或未指定
// 角色即管理端），cell = 执行端（--role=cell）。解析只认 manager/cell 两个
// 值；未知角色与缺值（--role 后无参或下一参形如选项）都显式拒绝——返回
// false 并填 err，由调用方退出非零，绝不静默回退。cell 角色不派生子进程
// （自动拉起/扩缩容/监督接续随 P2 管理/执行设计，不在本入口递归派生）。

#include <string>

namespace world_app {

// 纯解析（可直测）：遍历 argv 认 --role <v> 与 --role=<v>，重复指定以后者
// 为准（显式后到覆盖，不叠加）。成功 true + role 填缺省或显式值。
inline bool parse_world_role(int argc, char* argv[], std::string& role,
                             std::string& err) {
    role = "manager";
    for (int i = 1; i < argc; i++) {
        const std::string arg = argv[i];
        std::string value;
        if (arg.rfind("--role=", 0) == 0) {
            value = arg.substr(7);
        } else if (arg == "--role") {
            if (i + 1 >= argc) {
                err = "--role requires a value (manager|cell)";
                return false;
            }
            value = argv[++i];
        } else {
            continue;
        }
        if (value != "manager" && value != "cell") {
            err = "unknown role '" + value + "' (expected manager|cell)";
            return false;
        }
        role = value;
    }
    return true;
}

} // namespace world_app
