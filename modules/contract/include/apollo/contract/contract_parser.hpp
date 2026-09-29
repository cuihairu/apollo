/**
 * @file contract_parser.hpp
 * @brief 契约严格解析器（xml-generation §5 四层漏斗的第 ② 层主体）。
 *
 * 职责：
 *  - 结构严格性：pugixml 遍历 + 白名单（元素/属性名不在白名单即报错）——
 *    「错拼标签必须报错，不许静默缺省」（决策 #3 对 KBEngine 只解析无校验的补强）。
 *    XSD（apollo.xsd，xmllint CI 门禁）与本层为双保险。
 *  - 语义规则（XSD 表达不了的）：别名解析/环、默认值类型校验、ID 分段、
 *    SYNC_DB 禁用、实体 parent 悬垂/环检测/生成期展开、存储词汇拒绝。
 *  - 聚合报错：收集全部问题一次返回（xml-generation §4 产物 3），不遇错即停。
 *
 * 本库只被 sdks/gen 与测试链接——运行时服务消费生成产物，永不链接解析器（决策 #4）。
 */

#pragma once

#include "apollo/contract/contract_model.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace apollo::contract {

/// 一条诊断（file:line + xpath 式路径 + 消息）
struct Issue {
    std::string file;
    int line = 0;
    std::string path;      ///< 如 /attrs/attr[@name='hp']
    std::string message;

    std::string toString() const;
};

/// 解析结果：模型 + 全量诊断。errorCount()==0 才允许生成。
struct ParseResult {
    Contract contract;
    std::vector<Issue> issues;

    size_t errorCount() const;
    bool ok() const { return errorCount() == 0; }
    /// 人读聚合输出（每行一条，带 file:line）
    std::string report() const;
};

/// 解析整个契约目录（attrs.xml / messages.xml / entities.xml / errors.xml / version）。
/// 任一文件缺失或 version 非法均计入 issues。
ParseResult parseContractDirectory(const std::string& dir);

/// 单文件解析（测试用；从内存 XML）。各函数只填充 Contract 对应部分。
ParseResult parseAttrsXml(std::string_view xml, const std::string& fileName);
ParseResult parseMessagesXml(std::string_view xml, const std::string& fileName);
ParseResult parseEntitiesXml(std::string_view xml, const std::string& fileName);
ParseResult parseErrorsXml(std::string_view xml, const std::string& fileName);

/// 跨文件语义校验 + 实体展开（parseContractDirectory 内部最后一步；单文件解析器
/// 不跑跨文件规则，测试可单独调用）。向 issues 追加，返回展开后的模型副本。
Contract validateAndResolve(Contract contract, std::vector<Issue>& issues);

/// 读取 version 文件内容（一行数字）。失败返回 0 并写 issue。
uint16_t parseVersionFile(std::string_view text, std::vector<Issue>& issues,
                          const std::string& fileName);

}  // namespace apollo::contract
