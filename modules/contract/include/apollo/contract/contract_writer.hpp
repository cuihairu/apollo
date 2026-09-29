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

/// 双域 hash 的输入串（sdk-contract §11.3 ②）：输入按域过滤——client bundle =
/// client 域消息 + attrs + errors（客户端产物面语义，与 semantic.json/客户端 bin
/// 的内容一一对应）；internal bundle = internal 域消息 + entities（服务端私有面）。
/// 手工 version 不进域 bundle（version 随任何变更走，若入 bundle 则 internal-only
/// 变更也推动 client_hash，破坏「握手稳定/客户端包字节不变」不变量）；全量身份
/// （含 version）仍由 canonicalBundle 锚定。
std::string canonicalDomainBundle(const Contract& c, MsgDomain domain);

}  // namespace apollo::contract
