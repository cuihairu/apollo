/**
 * @file contract_writer.hpp
 * @brief 契约规范化序列化（canonical XML）——支撑「解析往返」与「diff 稳定」。
 *
 * 规范形态：紧凑一行一属性；可选属性为空即省略；sync/predict 为 xs:list（空白分隔）；
 * 元素按 id（或 name/code）升序。往返测试断言 parse(write(parse(x))) == parse(x)，
 * 且 write 是不动点：write(parse(write(c))) == write(c)。
 */

#pragma once

#include "apollo/contract/contract_model.hpp"

#include <string>

namespace apollo::contract {

/// 序列化为规范 attrs.xml / messages.xml / entities.xml / errors.xml 全文。
/// 只使用模型数据（与输入格式无关），输出恒定。
std::string writeAttrsXml(const Contract& c);
std::string writeMessagesXml(const Contract& c);
std::string writeEntitiesXml(const Contract& c);
std::string writeErrorsXml(const Contract& c);

/// schema_hash 的输入串：四个规范文件全文 + version 行拼接（顺序固定）。
std::string canonicalBundle(const Contract& c);

}  // namespace apollo::contract
