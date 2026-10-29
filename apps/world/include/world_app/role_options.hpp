#pragma once

// world 管理端角色（L104 收口片一）：manager 是唯一受理角色（缺省——无参
// 数或未指定角色即管理端）。执行端不再由本二进制委托——`--role=cell` 由
// cellServer 独立入口承担，此处显式拒绝并指引（不再保留双角色兼容分支）。
// 解析只认 manager；未知角色与缺值（--role 后无参或下一参形如选项）都显式
// 拒绝——返回 false 并填 err，由调用方退出非零，绝不静默回退。

#include <string>

namespace world_app {

// 纯解析（可直测）：遍历 argv 认 --role <v> 与 --role=<v>。manager 是唯一
// 受理值；cell 与未知值均拒绝。重复指定不叠加——遇到 cell 即返回失败，
// 不存在「后到覆盖」语义（cell 已移出本入口，任何一次出现都是错误）。
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
                err = "--role requires a value (manager)";
                return false;
            }
            value = argv[++i];
        } else {
            continue;
        }
        if (value == "cell") {
            err = "role 'cell' moved to the cellServer entry";
            return false;
        }
        if (value != "manager") {
            err = "unknown role '" + value + "' (expected manager)";
            return false;
        }
        role = value;
    }
    return true;
}

} // namespace world_app
