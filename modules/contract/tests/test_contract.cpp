/**
 * @file test_contract.cpp
 * @brief 契约解析/校验/生成测试（docs/36 第一批任务 5）。
 *
 * 覆盖：
 *  - XSD 错误用例的解析器等价实现：错拼标签/属性、重复 key、非法枚举、
 *    悬垂 parent（xs:keyref）、SYNC_DB 拒绝（决策 #5）
 *  - 语义规则：ID 分段、默认值类型、别名、继承环、错误码分区
 *  - 聚合报错（不遇错即停）
 *  - 解析往返 + 规范序列化不动点
 *  - schema_hash：SHA-256 已知向量 + 排版不变性 + 语义敏感性
 *  - 继承矩阵（祖先链展开）
 *  - 随仓契约目录（sdks/contract）必须零错误通过
 */

#include "apollo/contract/contract_hash.hpp"
#include "apollo/contract/contract_parser.hpp"
#include "apollo/contract/contract_writer.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using namespace apollo::contract;

namespace {

int g_failures = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (!(cond)) {                                                          \
            std::cerr << "FAIL: " << msg << " (" << __FILE__ << ":" << __LINE__ \
                      << ")\n";                                                 \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

size_t issuesContaining(const ParseResult& r, const std::string& needle) {
    size_t n = 0;
    for (const auto& i : r.issues) {
        if (i.toString().find(needle) != std::string::npos) ++n;
    }
    return n;
}

ParseResult parseExpectingError(const std::string& xml, const std::string& file,
                                const std::string& needle) {
    // 按根元素分派到对应单文件解析器（结构层错误必须出自正确的 root 校验）
    ParseResult r = xml.find("<messages") != std::string::npos
                        ? parseMessagesXml(xml, file)
                        : parseAttrsXml(xml, file);
    CHECK(r.errorCount() > 0, std::string("期望报错: ") + needle);
    CHECK(issuesContaining(r, needle) > 0,
          "错误信息应包含 \"" + needle + "\"，实际:\n" + r.report());
    return r;
}

/// 单文件解析 + 全量语义校验（四层漏斗：结构层(wrapper) + 语义层 ②）。
/// 语义断言（重复 key/分段/别名/默认值/继承/错误分区）都必须走这里——
/// parseAttrsXml 只做结构层，跨元素规则按设计留在 validateAndResolve。
ParseResult parseAttrsFull(const std::string& xml, const std::string& file = "attrs.xml") {
    ParseResult r = parseAttrsXml(xml, file);
    r.contract = validateAndResolve(std::move(r.contract), r.issues);
    return r;
}

ParseResult parseMessagesFull(const std::string& xml, const std::string& file = "messages.xml") {
    ParseResult r = parseMessagesXml(xml, file);
    r.contract = validateAndResolve(std::move(r.contract), r.issues);
    return r;
}

ParseResult parseEntitiesFull(const std::string& xml, const std::string& file = "entities.xml") {
    ParseResult r = parseEntitiesXml(xml, file);
    r.contract = validateAndResolve(std::move(r.contract), r.issues);
    return r;
}

ParseResult parseErrorsFull(const std::string& xml, const std::string& file = "errors.xml") {
    ParseResult r = parseErrorsXml(xml, file);
    r.contract = validateAndResolve(std::move(r.contract), r.issues);
    return r;
}

ParseResult expectSemanticError(const std::string& xml, const std::string& needle) {
    ParseResult r = parseAttrsFull(xml);
    CHECK(r.errorCount() > 0, std::string("期望语义报错: ") + needle);
    CHECK(issuesContaining(r, needle) > 0,
          "语义错误应包含 \"" + needle + "\"，实际:\n" + r.report());
    return r;
}

std::string readFileOrEmpty(const std::string& path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// ------------------------------------------------------------------ 结构层

void testMisspelledElementRejected() {
    // 错拼标签必须报错（决策 #3：对照 KBE/BW 只解析无校验的静默缺省）
    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<atr id="1" name="hp" type="int64"/>
</attrs>)",
        "attrs.xml", "未知元素");
}

void testMisspelledAttributeRejected() {
    // 持久化词汇（persist）不在白名单——结构上不可能进契约（决策 #5）
    ParseResult r = parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" persist="true"/>
</attrs>)",
        "attrs.xml", "未知属性");
    CHECK(issuesContaining(r, "persist") > 0, "错误信息应点名 persist");
}

void testPersistElementChildRejected() {
    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64"><persist column="c_hp"/></attr>
</attrs>)",
        "attrs.xml", "未知元素");
}

void testDuplicateKeysRejected() {
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64"/>
<attr id="1" name="max_hp" type="int64"/>
</attrs>)",
        "重复 attr id");

    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64"/>
<attr id="2" name="hp" type="int64"/>
</attrs>)",
        "重复 attr name");
}

void testIllegalEnumsRejected() {
    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" sync="PROPX"/>
</attrs>)",
        "attrs.xml", "sync 非法 token");

    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" channel="attr"/>
</attrs>)",
        "attrs.xml", "channel 非法值");

    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" predict="client_guessed"/>
</attrs>)",
        "attrs.xml", "predict 非法 token");

    parseExpectingError(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2C" channel="movement"/>
</messages>)",
        "messages.xml", "dir 非法值");
}

void testSyncDbRejected() {
    // 决策 #5 落点：SYNC_DB 是存储语义，契约里显式拒绝并指路 storage.xml
    parseExpectingError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" sync="PROP DB"/>
</attrs>)",
        "attrs.xml", "存储语义禁止进契约");
}

void testWrongRootRejected() {
    ParseResult r = parseAttrsXml(
        R"(<?xml version="1.0"?><messages version="1"/>)", "attrs.xml");
    CHECK(r.errorCount() > 0, "错根元素应报错");
    CHECK(issuesContaining(r, "根元素不匹配") > 0, "应报告根元素不匹配");
}

void testAggregatedErrors() {
    // 一份坏文件应一次报全，不遇错即停（xml-generation §4 产物 3）
    ParseResult r = parseAttrsFull(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" sync="NOPE"/>
<attr id="1" name="hp" type="vec7"/>
<attr id="2" name="hp" type="int64"/>
</attrs>)");
    CHECK(r.errorCount() >= 3, "聚合报错应 >= 3 条，实际 " + std::to_string(r.errorCount()));
}

// ------------------------------------------------------------------ 语义层

void testIdSegmentRule() {
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="900" name="orphan" type="int64"/>
</attrs>)",
        "不在任何分段");
}

void testDefaultValueTypeCheck() {
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" default="abc"/>
</attrs>)",
        "与类型");
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="online" type="bool" default="yes"/>
</attrs>)",
        "与类型");
    // 合法默认值不应报错
    ParseResult ok = parseAttrsFull(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" default="-5"/>
<attr id="2" name="online" type="bool" default="false"/>
</attrs>)");
    CHECK(ok.ok(), "合法默认值误报:\n" + ok.report());
}

void testAliasRules() {
    // 别名套别名拒绝
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<alias name="percent" type="int64"/>
<alias name="ratio" type="percent"/>
</attrs>)",
        "别名套别名");
    // 未知 underlying 拒绝
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<alias name="percent" type="vec7"/>
</attrs>)",
        "别名套别名");
    // 遮蔽基元名拒绝
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<alias name="int64" type="int32"/>
</attrs>)",
        "不得遮蔽基元");
    // 合法别名可用于 attr.type
    ParseResult ok = parseAttrsFull(
        R"(<?xml version="1.0"?><attrs version="1">
<alias name="percent" type="int64"/>
<attr id="10" name="speed" type="percent"/>
</attrs>)");
    CHECK(ok.ok(), "合法别名误报:\n" + ok.report());
    CHECK(ok.contract.attrs[0].type == WireType::Int64, "别名应解析为底层基元类型");
}

void testPredictCombinations() {
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" predict="none client_predicted"/>
</attrs>)",
        "不允许 none 与其它权限位并存");
    expectSemanticError(
        R"(<?xml version="1.0"?><attrs version="1">
<attr id="1" name="hp" type="int64" predict="client_predicted"/>
</attrs>)",
        "必须可同步");
}

void testErrorCodeRanges() {
    ParseResult r = parseErrorsFull(R"(<?xml version="1.0"?><errors version="1">
<error code="500" name="weird"/>
</errors>)");
    CHECK(r.errorCount() > 0, "错误码分区应报错");
    CHECK(issuesContaining(r, "超出分区") > 0, "应提示错误码分区（docs/19 §4.3）");
}

// ------------------------------------------------------------ 分域与绑定

void testMsgDomainRules() {
    // domain 必填（§11.3：内外分域是硬规则，不许静默缺省）
    ParseResult r = parseMessagesXml(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement"/>
</messages>)",
        "messages.xml");
    CHECK(issuesContaining(r, "缺少必填属性 'domain'") > 0, "缺 domain 应报必填错误");

    // 非法域值拒绝
    r = parseMessagesXml(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement" domain="both"/>
</messages>)",
        "messages.xml");
    CHECK(issuesContaining(r, "domain 非法值") > 0, "非法 domain 应报错");

    // 非法绑定值拒绝
    r = parseMessagesXml(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client" binding="hybrid"/>
</messages>)",
        "messages.xml");
    CHECK(issuesContaining(r, "binding 非法值") > 0, "非法 binding 应报错");

    // 域分段（§11.3 ①：client 1-899 / internal 900+）
    r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="900" name="leak" dir="C2S" channel="control" domain="client"/>
</messages>)");
    CHECK(issuesContaining(r, "超出 client 域分段") > 0, "client 域 id 900+ 应报分段错误");

    r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="100" name="inner" dir="P2P" channel="control" domain="internal"/>
</messages>)");
    CHECK(issuesContaining(r, "超出 internal 域分段") > 0, "internal 域 id <900 应报分段错误");

    // 合法分域零报错（段内自由增删）
    r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="899" name="edge_client" dir="S2C" channel="control" domain="client"/>
<msg id="900" name="edge_internal" dir="P2P" channel="control" domain="internal"/>
</messages>)");
    CHECK(r.ok(), "分段边界值合法:\n" + r.report());
}

void testBindingDefaultByChannel() {
    // 缺省按通道（§10.6 v3）：events=reflect，movement/attributes/control=native
    ParseResult r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client"/>
<msg id="100" name="chat" dir="C2S" channel="events" domain="client"/>
<msg id="900" name="zone_sync" dir="P2P" channel="events" domain="internal"/>
</messages>)");
    CHECK(r.ok(), "合法消息误报:\n" + r.report());
    CHECK(r.contract.msgs[0].binding == MsgBinding::Native, "movement 缺省 native");
    CHECK(r.contract.msgs[1].binding == MsgBinding::Reflect, "events 缺省 reflect");
    CHECK(r.contract.msgs[2].binding == MsgBinding::Reflect, "internal events 同样缺省 reflect");

    // 显式声明可覆盖缺省（框架族例外走显式 binding，评审把好关）
    r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="100" name="big_event" dir="C2S" channel="events" domain="client" binding="native"/>
</messages>)");
    CHECK(r.ok(), "显式 binding 覆盖应合法:\n" + r.report());
    CHECK(r.contract.msgs[0].binding == MsgBinding::Native, "显式 native 覆盖 events 缺省");
}

void testCrossDomainReferenceRejected() {
    // §11.3 ④：client 域消息引用 internal 域消息名 → 专属禁令诊断（非泛化未知类型）
    ParseResult r = parseMessagesFull(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="900" name="session_blob" dir="P2P" channel="events" domain="internal"/>
<msg id="100" name="invoke" dir="C2S" channel="events" domain="client">
  <field name="payload" type="session_blob"/>
</msg>
</messages>)");
    CHECK(issuesContaining(r, "跨域引用禁令") > 0,
          "client→internal 引用应报跨域禁令，实际:\n" + r.report());
}

void testDomainHashes() {
    // §11.3 ②：双域 hash——输入按域过滤，internal-only 变更不改 client_hash
    // （握手稳定/客户端包字节不变），client 域变更不改 internal_hash。
    // bundle 划分：client 面 = client 域消息 + attrs + errors；
    // internal 面 = internal 域消息 + entities；手工 version 不进域 bundle
    // （全量身份含 version 仍由 schema_hash 锚定）。
    std::string shippedAttrs = readFileOrEmpty(std::string(APOLLO_CONTRACT_DIR) + "/attrs.xml");
    ParseResult pa = parseAttrsXml(shippedAttrs, "attrs.xml");
    CHECK(pa.errorCount() == 0, "域 hash 测试的 attrs 基线应零错误");

    auto build = [&](const std::string& msgsXml, const std::string& entitiesXml) {
        ParseResult r = parseMessagesXml(msgsXml, "messages.xml");
        ParseResult re = parseEntitiesXml(entitiesXml, "entities.xml");
        CHECK(re.errorCount() == 0, "entities 基线应零错误");
        r.contract.aliases = pa.contract.aliases;
        r.contract.attrs = pa.contract.attrs;
        r.contract.errors = pa.contract.errors;
        r.contract.entities = re.contract.entities;
        r.contract = validateAndResolve(std::move(r.contract), r.issues);
        CHECK(r.ok(), "域 hash 测试基线应零错误:\n" + r.report());
        return r.contract;
    };

    const std::string msgsBase =
        R"(<?xml version="1.0"?><messages version="2">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client"/>
<msg id="900" name="zone_sync" dir="P2P" channel="events" domain="internal"/>
</messages>)";
    const std::string msgsMoreInternal =
        R"(<?xml version="1.0"?><messages version="2">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client"/>
<msg id="900" name="zone_sync" dir="P2P" channel="events" domain="internal"/>
<msg id="901" name="rebalance" dir="P2P" channel="events" domain="internal"/>
</messages>)";
    const std::string msgsMoreClient =
        R"(<?xml version="1.0"?><messages version="2">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client"/>
<msg id="11" name="emote" dir="C2S" channel="events" domain="client"/>
<msg id="900" name="zone_sync" dir="P2P" channel="events" domain="internal"/>
</messages>)";
    const std::string entitiesEmpty = R"(<?xml version="1.0"?><entities version="2"/>)";
    const std::string entitiesOne =
        R"(<?xml version="1.0"?><entities version="2"><entity id="Region"/></entities>)";

    Contract c1 = build(msgsBase, entitiesEmpty);
    Contract c2 = build(msgsMoreInternal, entitiesEmpty);  // +internal 消息
    Contract c3 = build(msgsMoreClient, entitiesEmpty);    // +client 消息
    Contract c4 = c1;                                      // attr 语义变更
    c4.attrs[0].defaultValue = "2000";
    Contract c5 = build(msgsBase, entitiesOne);            // +entity
    Contract c6 = c1;                                      // 仅手工版本号
    c6.version = 3;

    const std::string ver = "apollo-gen 0.2.0";
    auto ch = [&](const Contract& c) { return computeDomainHash(c, ver, MsgDomain::Client); };
    auto ih = [&](const Contract& c) { return computeDomainHash(c, ver, MsgDomain::Internal); };

    CHECK(ch(c1).size() == 64 && ih(c1).size() == 64, "域 hash 应为 64 位十六进制");
    CHECK(ch(c1) != ih(c1), "client/internal bundle 输入不同，hash 必互异");

    CHECK(ch(c2) == ch(c1), "internal-only 增消息不得改 client_hash（握手稳定）");
    CHECK(ih(c2) != ih(c1), "internal 增消息必改 internal_hash");
    CHECK(ch(c3) != ch(c1), "client 增消息必改 client_hash");
    CHECK(ih(c3) == ih(c1), "client-only 增消息不得改 internal_hash");

    CHECK(ch(c4) != ch(c1), "attr 语义变更必改 client_hash（attrs 进 client bundle）");
    CHECK(ih(c4) == ih(c1), "attr 变更不得改 internal_hash");
    CHECK(ch(c5) == ch(c1), "entity 变更不得改 client_hash（entities 进 internal bundle）");
    CHECK(ih(c5) != ih(c1), "entity 变更必改 internal_hash");

    CHECK(ch(c6) == ch(c1) && ih(c6) == ih(c1),
          "手工 version 变更不得改域 hash（否则 internal-only 发版会推动客户端包）");
    CHECK(computeSchemaHash(c6, ver) != computeSchemaHash(c1, ver),
          "version 变更必改全量 schema_hash（记账仍有效）");

    CHECK(ch(c1) != computeDomainHash(c1, "apollo-gen 0.3.0", MsgDomain::Client),
          "生成器版本参与域 hash");
}

void testIncludeAggregation() {
    // §11.4 include 聚合：读取层展开为零一棵文档树——XSD 校验与 schema_hash
    // 一律对聚合后整体（物理分文件不影响 hash 稳定性）。关键性质：分文件目录
    // 与单文件目录解析出的模型与 schema_hash 完全相等。
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "apollo_contract_include";
    fs::remove_all(tmp);
    fs::create_directories(tmp);
    auto write = [&](const std::string& name, const std::string& content) {
        std::ofstream(tmp / name) << content;
    };
    auto writeBase = [&](const std::string& messagesXml) {
        write("version", "2\n");
        write("attrs.xml",
              "<?xml version=\"1.0\"?><attrs version=\"2\">\n"
              "<attr id=\"1\" name=\"hp\" type=\"int64\" sync=\"PROP\" default=\"1000\"/>\n"
              "</attrs>\n");
        write("messages.xml", messagesXml);
        write("entities.xml", "<?xml version=\"1.0\"?><entities version=\"2\"/>");
        write("errors.xml", "<?xml version=\"1.0\"?><errors version=\"2\"/>");
    };
    const std::string msgMove =
        "<msg id=\"10\" name=\"move\" dir=\"C2S\" channel=\"movement\" domain=\"client\"/>";
    const std::string msgZone =
        "<msg id=\"900\" name=\"zone_sync\" dir=\"P2P\" channel=\"events\" "
        "domain=\"internal\"/>";
    const std::string hdr = "<?xml version=\"1.0\"?>";

    // ---- 等价性：单文件 vs 分文件（hash 必须相等）----
    writeBase(hdr + "<messages version=\"2\">\n" + msgMove + "\n" + msgZone + "\n</messages>\n");
    ParseResult mono = parseContractDirectory(tmp.string());
    CHECK(mono.ok(), "单文件基线应零错误:\n" + mono.report());

    writeBase(hdr + "<messages version=\"2\">\n" + msgMove +
              "\n<include href=\"messages_internal.xml\"/>\n</messages>\n");
    write("messages_internal.xml", hdr + "<messages version=\"2\">\n" + msgZone + "\n</messages>\n");
    ParseResult split = parseContractDirectory(tmp.string());
    CHECK(split.ok(), "分文件应零错误:\n" + split.report());
    CHECK(split.contract.msgs == mono.contract.msgs, "聚合后消息表应与单文件相等");
    CHECK(computeSchemaHash(split.contract, "apollo-gen 0.2.0") ==
              computeSchemaHash(mono.contract, "apollo-gen 0.2.0"),
          "物理分文件不得改变 schema_hash（§11.4：hash 对聚合后整体）");

    // 纯 include 根（分域组织的自然形态）也应等价
    writeBase(hdr + "<messages version=\"2\">\n"
              "<include href=\"messages_client.xml\"/>\n"
              "<include href=\"messages_internal.xml\"/>\n</messages>\n");
    write("messages_client.xml", hdr + "<messages version=\"2\">\n" + msgMove + "\n</messages>\n");
    ParseResult byDomain = parseContractDirectory(tmp.string());
    CHECK(byDomain.ok(), "纯 include 根应零错误:\n" + byDomain.report());
    CHECK(byDomain.contract.msgs == mono.contract.msgs, "双分片聚合应与单文件相等");

    // ---- 错误族 ----
    auto expectDirError = [&](const std::string& needle) {
        ParseResult r = parseContractDirectory(tmp.string());
        CHECK(issuesContaining(r, needle) > 0,
              "目录解析应报 \"" + needle + "\"，实际:\n" + r.report());
    };
    // 环
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"a.xml\"/>\n</messages>\n");
    write("a.xml", hdr + "<messages version=\"2\">\n<include href=\"b.xml\"/>\n</messages>\n");
    write("b.xml", hdr + "<messages version=\"2\">\n<include href=\"a.xml\"/>\n</messages>\n");
    expectDirError("include 环");
    // 缺文件
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"ghost.xml\"/>\n</messages>\n");
    expectDirError("无法读取/解析 include 文件");
    // 根元素不匹配
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"wrong_root.xml\"/>\n</messages>\n");
    write("wrong_root.xml",
          hdr + "<attrs version=\"2\">\n<attr id=\"1\" name=\"hp\" type=\"int64\"/>\n</attrs>\n");
    expectDirError("include 根元素不匹配");
    // version 不一致
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"v3.xml\"/>\n</messages>\n");
    write("v3.xml", hdr + "<messages version=\"3\">\n" + msgZone + "\n</messages>\n");
    expectDirError("include version 不一致");
    // 深层 include 不展开（由 strictWalk 白名单拦）
    writeBase(hdr + "<messages version=\"2\">\n" + msgMove +
              "\n<include href=\"messages_internal.xml\"/>\n</messages>\n");
    write("messages_internal.xml",
          hdr + "<messages version=\"2\">\n" + msgZone +
          "\n<msg id=\"11\" name=\"emote\" dir=\"C2S\" channel=\"events\" domain=\"client\">\n"
          "<include href=\"messages_client.xml\"/>\n</msg>\n</messages>\n");
    write("messages_client.xml", hdr + "<messages version=\"2\"/>\n");
    expectDirError("未知元素");
    // 重复 include：拼接两份 → 重复 key 由既有规则拦
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"dup.xml\"/>\n"
              "<include href=\"dup.xml\"/>\n</messages>\n");
    write("dup.xml", hdr + "<messages version=\"2\">\n" + msgZone + "\n</messages>\n");
    expectDirError("重复 msg id");
    // 缺 href
    writeBase(hdr + "<messages version=\"2\">\n<include/>\n</messages>\n");
    expectDirError("include 缺少必填属性 'href'");
    // include 携带未知属性
    writeBase(hdr + "<messages version=\"2\">\n<include href=\"dup.xml\" src=\"x\"/>\n</messages>\n");
    expectDirError("include 未知属性");

    fs::remove_all(tmp);
}

// ------------------------------------------------------------------ 继承矩阵

void testInheritanceMatrix() {
    ParseResult r = parseEntitiesFull(
        R"(<?xml version="1.0"?><entities version="1">
<entity id="Monster"/>
<entity id="NPC" parent="Monster"/>
<entity id="Avatar" parent="Monster"/>
<entity id="Player" parent="Avatar"/>
</entities>)");
    CHECK(r.ok(), "合法继承误报:\n" + r.report());
    const auto& es = r.contract.entities;
    auto find = [&](const std::string& n) -> const EntityDef* {
        for (const auto& e : es)
            if (e.name == n) return &e;
        return nullptr;
    };
    CHECK(find("Monster")->ancestors.empty(), "根实体无祖先");
    CHECK(find("NPC")->ancestors.size() == 1 && find("NPC")->ancestors[0] == "Monster",
          "NPC 祖先链 = [Monster]");
    CHECK(find("Player")->ancestors.size() == 2 &&
              find("Player")->ancestors[0] == "Monster" &&
              find("Player")->ancestors[1] == "Avatar",
          "Player 祖先链 = [Monster, Avatar]（根在前）");
}

void testInheritanceCycleRejected() {
    ParseResult r = parseEntitiesFull(
        R"(<?xml version="1.0"?><entities version="1">
<entity id="A" parent="B"/>
<entity id="B" parent="A"/>
</entities>)");
    CHECK(r.errorCount() > 0, "继承环必须报错（§16.7.2：两家 .def 都没做）");
    CHECK(issuesContaining(r, "继承环") > 0, "应点名继承环");
}

void testInheritanceDanglingParentRejected() {
    ParseResult r = parseEntitiesFull(
        R"(<?xml version="1.0"?><entities version="1">
<entity id="A" parent="Ghost"/>
</entities>)");
    CHECK(r.errorCount() > 0, "悬垂 parent 必须报错");
    CHECK(issuesContaining(r, "不存在") > 0, "应提示 parent 不存在");
}

// ------------------------------------------------------------ 往返与 hash

void testRoundTripAndFixpoint() {
    std::string shipped = readFileOrEmpty(std::string(APOLLO_CONTRACT_DIR) + "/attrs.xml");
    CHECK(!shipped.empty(), "随仓 attrs.xml 应可读");
    ParseResult p1 = parseAttrsFull(shipped);
    CHECK(p1.ok(), "随仓 attrs.xml 必须零错误:\n" + p1.report());

    std::string canonical = writeAttrsXml(p1.contract);
    ParseResult p2 = parseAttrsFull(canonical);
    CHECK(p2.ok(), "规范输出必须可回读:\n" + p2.report());
    CHECK(p2.contract.attrs == p1.contract.attrs, "往返后属性表应相等");
    CHECK(p2.contract.aliases == p1.contract.aliases, "往返后别名表应相等");
    CHECK(p2.contract.version == p1.contract.version, "往返后 version 应相等");

    // 不动点：write(parse(write(c))) == write(c)
    CHECK(writeAttrsXml(p2.contract) == canonical, "规范序列化应为不动点");

    // 打乱排版（换行/注释/乱序）后重解析 → 语义等价（hash 与往返一致）
    std::string shuffled =
        R"(<?xml version="1.0"?><attrs version="1">
<!-- 注释与空白差异不应影响语义 -->
<attr id="20" name="gold" type="int64" sync="SELF"   default="0" />
<attr id="1" name="hp" type="int64" sync="PROP" default="1000"/>
</attrs>)";
    ParseResult p3 = parseAttrsFull(shuffled);
    CHECK(p3.ok(), "乱序样本应可解析:\n" + p3.report());
    std::string c3 = writeAttrsXml(p3.contract);
    ParseResult p4 = parseAttrsFull(c3);
    CHECK(p4.ok() && p4.contract.attrs == p3.contract.attrs, "乱序样本往返应等价");
}

void testMessagesRoundTripAndFixpoint() {
    // 单文件解析不带 attrs.xml 的别名表——先取随仓 aliases 注入再全量校验
    // （heartbeat.client_ms 用了 alias timestamp；目录级解析天然无此问题）
    std::string shippedAttrs = readFileOrEmpty(std::string(APOLLO_CONTRACT_DIR) + "/attrs.xml");
    ParseResult pa = parseAttrsXml(shippedAttrs, "attrs.xml");
    auto parseMsgsWithAliases = [&](const std::string& xml) {
        ParseResult r = parseMessagesXml(xml, "messages.xml");
        r.contract.aliases = pa.contract.aliases;
        r.contract = validateAndResolve(std::move(r.contract), r.issues);
        return r;
    };

    std::string shipped = readFileOrEmpty(std::string(APOLLO_CONTRACT_DIR) + "/messages.xml");
    CHECK(!shipped.empty(), "随仓 messages.xml 应可读");
    ParseResult p1 = parseMsgsWithAliases(shipped);
    CHECK(p1.ok(), "随仓 messages.xml 必须零错误:\n" + p1.report());

    std::string canonical = writeMessagesXml(p1.contract);
    ParseResult p2 = parseMsgsWithAliases(canonical);
    CHECK(p2.ok(), "规范输出必须可回读:\n" + p2.report());
    CHECK(p2.contract.msgs == p1.contract.msgs, "往返后消息表应相等（含 domain/binding）");
    // 不动点：write(parse(write(c))) == write(c)——binding 写解析后值的前提
    CHECK(writeMessagesXml(p2.contract) == canonical, "规范序列化应为不动点");

    // 显式 binding 输入与缺省 binding 输入规范形态一致（缺省展开后同型）
    ParseResult p3 = parseMsgsWithAliases(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client" binding="native"/>
</messages>)");
    ParseResult p4 = parseMsgsWithAliases(
        R"(<?xml version="1.0"?><messages version="1">
<msg id="10" name="move" dir="C2S" channel="movement" domain="client"/>
</messages>)");
    CHECK(writeMessagesXml(p3.contract) == writeMessagesXml(p4.contract),
          "缺省 binding 展开后规范形态应与显式声明一致");
}

void testSha256KnownVector() {
    CHECK(Sha256::hex("abc") ==
              "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "SHA-256(\"abc\") 已知向量");
    CHECK(Sha256::hex("") ==
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
          "SHA-256(\"\") 已知向量");
    // 跨块边界（>64 字节）
    std::string long_input(200, 'x');
    CHECK(Sha256::hex(long_input).size() == 64, "长输入仍产出 64 位十六进制");
}

void testSchemaHashStability() {
    std::string shipped = readFileOrEmpty(std::string(APOLLO_CONTRACT_DIR) + "/attrs.xml");
    ParseResult p1 = parseAttrsFull(shipped);
    std::string h1 = computeSchemaHash(p1.contract, "apollo-gen 0.1.0");

    // 排版变化不改 hash（规范化内容为输入）
    ParseResult p2 = parseAttrsFull(writeAttrsXml(p1.contract));
    std::string h2 = computeSchemaHash(p2.contract, "apollo-gen 0.1.0");
    CHECK(h1 == h2, "排版变化不应改变 schema_hash");
    CHECK(h1.size() == 64, "hash 应为 64 位十六进制");

    // 语义变化必改 hash（哪怕手工版本号没动）
    Contract mutated = p1.contract;
    mutated.attrs[0].defaultValue = "2000";
    std::string h3 = computeSchemaHash(mutated, "apollo-gen 0.1.0");
    CHECK(h1 != h3, "语义变化必须改变 schema_hash（防漏改版本号）");

    // 生成器版本参与 hash
    std::string h4 = computeSchemaHash(p1.contract, "apollo-gen 0.2.0");
    CHECK(h1 != h4, "生成器版本变化应反映在 hash");
}

// ------------------------------------------------------------ 随仓契约目录

void testShippedContractDirectory() {
    ParseResult r = parseContractDirectory(APOLLO_CONTRACT_DIR);
    if (!r.ok()) std::cerr << r.report();
    CHECK(r.ok(), "随仓契约目录必须零错误通过");
    CHECK(r.contract.version == 2, "随仓 version 应为 2（v2 = 消息分域批）");
    CHECK(r.contract.attrs.size() >= 20, "随仓属性应 >= 20 条");
    CHECK(r.contract.msgs.size() == 6, "随仓消息应为 6 条（P1-1 增换幕面 scene_transfer/scene_transfer_result）");
    CHECK(r.contract.errors.size() >= 6, "随仓错误码应 >= 6 条");
    for (const auto& m : r.contract.msgs) {
        CHECK(m.domain == MsgDomain::Client, "随仓消息全为 client 域: " + m.name);
        CHECK(m.binding == MsgBinding::Native,
              "随仓六条全为框架固定消息族（native）: " + m.name);
    }
    for (const auto& a : r.contract.attrs) {
        CHECK((a.syncMask & kSyncDbBanned) == 0,
              "任何属性都不得携带 SYNC_DB 位（决策 #5）: " + a.name);
    }
    // 决策 #5 的机器可验形态：模型本身无持久化成员——此处按名字再核一道
    for (const auto& a : r.contract.attrs) {
        CHECK(a.name.find("persist") == std::string::npos &&
                  a.name.find("storage") == std::string::npos,
              "属性名不得携带存储词汇: " + a.name);
    }
}

void testContractDirectoryErrors() {
    namespace fs = std::filesystem;
    fs::path tmp = fs::temp_directory_path() / "apollo_contract_test_dir";
    fs::create_directories(tmp);
    // 缺文件场景
    {
        ParseResult r = parseContractDirectory(tmp.string());
        CHECK(r.errorCount() > 0, "缺文件必须报错");
        CHECK(issuesContaining(r, "无法读取") >= 1, "应点名无法读取");
    }
    // version 不一致场景
    {
        std::ofstream(tmp / "version") << "2\n";
        std::ofstream(tmp / "attrs.xml")
            << "<?xml version=\"1.0\"?><attrs version=\"9\"/>";
        ParseResult r = parseContractDirectory(tmp.string());
        CHECK(issuesContaining(r, "不一致") > 0, "文件间 version 不一致应报错");
        ParseResult v = parseAttrsXml(readFileOrEmpty(tmp / "attrs.xml"), "attrs.xml");
        CHECK(v.errorCount() == 0, "单文件 version=9 本身合法");
    }
    // version 文件坏值
    {
        std::ofstream(tmp / "version") << "abc\n";
        std::ofstream(tmp / "attrs.xml") << "<?xml version=\"1.0\"?><attrs version=\"1\"/>";
        ParseResult r = parseContractDirectory(tmp.string());
        CHECK(issuesContaining(r, "0-65535") > 0, "坏 version 应报 0-65535");
    }
    fs::remove_all(tmp);
}

}  // namespace

int main() {
    testMisspelledElementRejected();
    testMisspelledAttributeRejected();
    testPersistElementChildRejected();
    testDuplicateKeysRejected();
    testIllegalEnumsRejected();
    testSyncDbRejected();
    testWrongRootRejected();
    testAggregatedErrors();
    testIdSegmentRule();
    testDefaultValueTypeCheck();
    testAliasRules();
    testPredictCombinations();
    testErrorCodeRanges();
    testMsgDomainRules();
    testBindingDefaultByChannel();
    testCrossDomainReferenceRejected();
    testDomainHashes();
    testIncludeAggregation();
    testInheritanceMatrix();
    testInheritanceCycleRejected();
    testInheritanceDanglingParentRejected();
    testRoundTripAndFixpoint();
    testMessagesRoundTripAndFixpoint();
    testSha256KnownVector();
    testSchemaHashStability();
    testShippedContractDirectory();
    testContractDirectoryErrors();

    if (g_failures == 0) {
        std::cout << "apollo_contract_tests: ALL PASS\n";
        return 0;
    }
    std::cout << "apollo_contract_tests: " << g_failures << " FAILURE(S)\n";
    return 1;
}
