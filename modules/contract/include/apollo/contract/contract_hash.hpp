/**
 * @file contract_hash.hpp
 * @brief schema_hash（sdk-contract §6）：SHA-256(契约规范内容 + 生成器版本)。
 *
 * hash 基于规范化序列化而非原始文件字节——改排版不改语义时 hash 不变，改语义
 * （哪怕漏改手工版本号）hash 必变，握手即暴露。
 */

#pragma once

#include "apollo/contract/contract_model.hpp"

#include <array>
#include <cstdint>
#include <string>

namespace apollo::contract {

/// 自包含 SHA-256（无 OpenSSL 依赖——契约工具链不把加密库拖进链接图）。
class Sha256 {
public:
    Sha256();
    void update(const void* data, size_t len);
    void update(std::string_view s) { update(s.data(), s.size()); }
    std::array<uint8_t, 32> finish();

    static std::string hex(std::string_view data);

private:
    void processBlock(const uint8_t* block);
    uint32_t state_[8];
    uint64_t totalLen_;
    uint8_t buffer_[64];
    size_t bufferLen_;
};

/// schema_hash = SHA-256(canonicalBundle(contract) + generatorVersion)。返回十六进制。
std::string computeSchemaHash(const Contract& c, const std::string& generatorVersion);

/// 双域 hash（sdk-contract §11.3 ②）= SHA-256(canonicalDomainBundle + generatorVersion)。
/// 算法同上，输入按域过滤——internal-only 变更不改 client_hash（客户端握手稳定、
/// 客户端包字节不变），client 域变更不改 internal_hash；握手用 client_hash，
/// internal_hash 只做服务端部署期同批断言。
std::string computeDomainHash(const Contract& c, const std::string& generatorVersion,
                              MsgDomain domain);

}  // namespace apollo::contract
