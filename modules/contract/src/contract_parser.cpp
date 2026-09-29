/**
 * @file contract_parser.cpp
 * @brief 契约严格解析实现。结构白名单 + 语义规则 + 聚合报错。
 *
 * 分层（xml-generation §5）：
 *  - 结构层（strictWalk）：元素/属性白名单——错拼即错，等价于 apollo.xsd 的无通配符
 *    内容模型（XSD 门禁在 CI；本层兜底使本地无 xmllint 也不放行，决策 #3）。
 *  - 语义层（validateAndResolve）：XSD 表达不了的跨元素规则（别名/默认值/分段/
 *    SYNC_DB 禁用/继承 DAG 环检测——§16.7.2：BigWorld/KBEngine 都没做，必须自己写）。
 */

#include "apollo/contract/contract_parser.hpp"

#include <pugixml.hpp>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace apollo::contract {

namespace {

// ---------------------------------------------------------------- utilities

int lineOf(const std::string& buffer, ptrdiff_t offset) {
    if (offset < 0 || offset > static_cast<ptrdiff_t>(buffer.size())) return 0;
    return static_cast<int>(std::count(buffer.begin(), buffer.begin() + offset, '\n')) + 1;
}

std::vector<std::string> splitList(std::string_view s) {
    std::vector<std::string> out;
    std::istringstream ss{std::string(s)};
    std::string tok;
    while (ss >> tok) out.push_back(tok);
    return out;
}

bool isIdentifier(const std::string& s) {
    if (s.empty() || s[0] < 'a' || s[0] > 'z') return false;
    return std::all_of(s.begin() + 1, s.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

// 实体类型名 PascalCase（BW/KBE 惯例）；生成物中只作字符串字面量与数组后缀，
// 不作裸枚举成员，与 attr/msg/error 的小写 Identifier 分治。
bool isEntityName(const std::string& s) {
    if (s.empty() || s[0] < 'A' || s[0] > 'Z') return false;
    return std::all_of(s.begin() + 1, s.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}

void diag(std::vector<Issue>& issues, const std::string& file, int line,
          const std::string& path, const std::string& message) {
    issues.push_back(Issue{file, line, path, message});
}

/// 属性白名单条目
struct AttrSpec {
    const char* name;
    bool required;
};

/// 元素白名单：属性集 + 子元素集。strictWalk 据此拒绝一切未知拼写。
struct ElemSpec {
    const char* name;
    std::initializer_list<AttrSpec> attrs;
    std::initializer_list<const char*> children;
};

/// 结构严格遍历：未知元素/属性报错，必填属性缺失报错。
void strictWalk(pugi::xml_node node, const ElemSpec& spec, const std::string& file,
                const std::string& buffer, const std::string& path,
                std::vector<Issue>& issues) {
    int line = lineOf(buffer, node.offset_debug());

    // 属性白名单
    std::map<std::string, pugi::xml_attribute> seen;
    for (pugi::xml_attribute a : node.attributes()) {
        std::string an = a.name();
        if (an == "xmlns" || an.rfind("xmlns:", 0) == 0) continue;  // 命名空间声明放行
        bool known = false;
        for (const auto& as : spec.attrs) {
            if (an == as.name) { known = true; break; }
        }
        if (!known) {
            diag(issues, file, line, path,
                 "未知属性 '" + an + "'（<" + spec.name + "> 不允许；若为持久化语义 "
                 "(persist/column/table/storage/db/save/archive)：契约禁止存储字段，"
                 "落库声明属服务端私有 storage.xml——决策 #5）");
        }
        seen[an] = a;
    }
    for (const auto& as : spec.attrs) {
        if (as.required && !seen.count(as.name)) {
            diag(issues, file, line, path,
                 std::string("缺少必填属性 '") + as.name + "'（元素 <" + spec.name + ">）");
        }
    }

    // 子元素白名单
    for (pugi::xml_node c : node.children()) {
        if (c.type() != pugi::node_element) continue;
        std::string cn = c.name();
        bool known = false;
        for (const char* allowed : spec.children) {
            if (cn == allowed) { known = true; break; }
        }
        if (!known) {
            int cline = lineOf(buffer, c.offset_debug());
            diag(issues, file, cline, path + "/" + cn,
                 "未知元素 <" + cn + ">（<" + spec.name + "> 只允许白名单子元素——"
                 "错拼不静默缺省，决策 #3）");
        }
    }
}

std::optional<std::string> attrValue(pugi::xml_node n, const char* name) {
    pugi::xml_attribute a = n.attribute(name);
    if (a.empty()) return std::nullopt;
    return std::string(a.value());
}

bool parseUint16(const std::string& s, uint16_t& out) {
    uint32_t v = 0;
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
    if (ec != std::errc() || p != s.data() + s.size() || v > 0xFFFF) return false;
    out = static_cast<uint16_t>(v);
    return true;
}

bool parseInt32(const std::string& s, int32_t& out) {
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc() && p == s.data() + s.size();
}

// ------------------------------------------------------------- enum tables

const std::unordered_map<std::string, uint8_t>& syncTokenMap() {
    static const std::unordered_map<std::string, uint8_t> m = {
        {"APPR", kSyncAppr},   {"PROP", kSyncProp},   {"SELF", kSyncSelf},
        {"TEAM", kSyncTeam},   {"GUILD", kSyncGuild}, {"WORLD", kSyncWorld},
        {"IMMEDIATE", kSyncImmediate},
    };
    return m;
}

const std::unordered_set<std::string>& channelSet() {
    static const std::unordered_set<std::string> s = {
        "movement", "attributes", "events", "control"};
    return s;
}

const std::unordered_set<std::string>& predictSet() {
    static const std::unordered_set<std::string> s = {
        "none", "client_predicted", "server_authoritative"};
    return s;
}

const std::unordered_map<std::string, WireType>& primitiveMap() {
    static const std::unordered_map<std::string, WireType> m = {
        {"bool", WireType::Bool},     {"int32", WireType::Int32},
        {"int64", WireType::Int64},   {"float", WireType::Float},
        {"double", WireType::Double}, {"string", WireType::String},
        {"bytes", WireType::Bytes},   {"string[]", WireType::StringList},
        {"int64[]", WireType::Int64List},
    };
    return m;
}

// ------------------------------------------------------------ per-file impl

struct FileParse {
    Contract contract;  // 部分填充
    std::vector<Issue> issues;
    std::string buffer;
};

void parseAttrsDoc(pugi::xml_node root, FileParse& fp, const std::string& file);
void parseMessagesDoc(pugi::xml_node root, FileParse& fp, const std::string& file);
void parseEntitiesDoc(pugi::xml_node root, FileParse& fp, const std::string& file);
void parseErrorsDoc(pugi::xml_node root, FileParse& fp, const std::string& file);

FileParse loadDoc(std::string_view xml, const std::string& file, const char* expectedRoot) {
    FileParse fp;
    fp.buffer = std::string(xml);
    pugi::xml_document doc;
    pugi::xml_parse_result res = doc.load_string(fp.buffer.c_str(), pugi::parse_default);
    if (!res) {
        diag(fp.issues, file, lineOf(fp.buffer, res.offset), "/",
             std::string("XML 语法错误: ") + res.description());
        return fp;
    }
    pugi::xml_node root = doc.document_element();
    if (!root) {
        diag(fp.issues, file, 1, "/", "缺少根元素");
        return fp;
    }
    std::string rootName = root.name();
    if (rootName != expectedRoot) {
        diag(fp.issues, file, lineOf(fp.buffer, root.offset_debug()), "/" + rootName,
             std::string("根元素不匹配：期望 <") + expectedRoot + ">，实得 <" + rootName + ">");
        return fp;
    }
    if (rootName == "attrs") parseAttrsDoc(root, fp, file);
    else if (rootName == "messages") parseMessagesDoc(root, fp, file);
    else if (rootName == "entities") parseEntitiesDoc(root, fp, file);
    else parseErrorsDoc(root, fp, file);
    return fp;
}

void parseAttrsDoc(pugi::xml_node root, FileParse& fp, const std::string& file) {
    const ElemSpec rootSpec{"attrs", {{"version", true}}, {"alias", "attr"}};
    const ElemSpec aliasSpec{"alias", {{"name", true}, {"type", true}, {"desc", false}}, {}};
    const ElemSpec attrSpec{
        "attr",
        {{"id", true}, {"name", true}, {"type", true}, {"sync", false}, {"channel", false},
         {"predict", false}, {"default", false}, {"desc", false}},
        {}};

    strictWalk(root, rootSpec, file, fp.buffer, "/attrs", fp.issues);
    if (auto v = attrValue(root, "version")) {
        if (!parseUint16(*v, fp.contract.version)) {
            diag(fp.issues, file, 1, "/attrs", "version 必须是 0-65535 整数: '" + *v + "'");
        }
    }

    for (pugi::xml_node n : root.children()) {
        if (n.type() != pugi::node_element) continue;
        std::string cn = n.name();
        int line = lineOf(fp.buffer, n.offset_debug());
        if (cn == "alias") {
            strictWalk(n, aliasSpec, file, fp.buffer, "/attrs/alias", fp.issues);
            TypeAlias alias;
            alias.name = attrValue(n, "name").value_or("");
            alias.underlying = attrValue(n, "type").value_or("");
            alias.desc = attrValue(n, "desc").value_or("");
            if (!alias.name.empty() && !isIdentifier(alias.name)) {
                diag(fp.issues, file, line, "/attrs/alias",
                     "alias name 必须匹配 [a-z][a-z0-9_]*: '" + alias.name + "'");
            }
            fp.contract.aliases.push_back(std::move(alias));
        } else if (cn == "attr") {
            std::string path = "/attrs/attr";
            if (auto nm = attrValue(n, "name")) path += "[@name='" + *nm + "']";
            strictWalk(n, attrSpec, file, fp.buffer, path, fp.issues);
            AttrDef def;
            def.sourceFile = file;
            std::string ids = attrValue(n, "id").value_or("");
            if (!parseUint16(ids, def.id)) {
                diag(fp.issues, file, line, path, "id 必须是 0-65535 整数: '" + ids + "'");
            }
            def.name = attrValue(n, "name").value_or("");
            if (!def.name.empty() && !isIdentifier(def.name)) {
                diag(fp.issues, file, line, path,
                     "attr name 必须匹配 [a-z][a-z0-9_]*: '" + def.name + "'");
            }
            def.typeName = attrValue(n, "type").value_or("");
            if (auto sy = attrValue(n, "sync")) {
                uint8_t mask = 0;
                for (const auto& tok : splitList(*sy)) {
                    if (tok == "DB" || tok == "SYNC_DB") {
                        diag(fp.issues, file, line, path,
                             "sync 含 'DB'——存储语义禁止进契约（SYNC_DB 0x04 属 storage.xml，"
                             "决策 #5：def 只答线上怎么传，落库归 storage.xml）");
                        continue;
                    }
                    auto it = syncTokenMap().find(tok);
                    if (it == syncTokenMap().end()) {
                        diag(fp.issues, file, line, path,
                             "sync 非法 token '" + tok +
                                 "'（合法：APPR PROP SELF TEAM GUILD WORLD IMMEDIATE）");
                        continue;
                    }
                    mask |= it->second;
                }
                def.syncMask = mask;
            }
            if (auto ch = attrValue(n, "channel")) {
                if (!channelSet().count(*ch)) {
                    diag(fp.issues, file, line, path,
                         "channel 非法值 '" + *ch +
                             "'（合法：movement attributes events control）");
                }
                def.channel = *ch;
            }
            if (auto pr = attrValue(n, "predict")) {
                def.predict = splitList(*pr);
                for (const auto& tok : def.predict) {
                    if (!predictSet().count(tok)) {
                        diag(fp.issues, file, line, path,
                             "predict 非法 token '" + tok +
                                 "'（合法：none client_predicted server_authoritative）");
                    }
                }
            }
            def.defaultValue = attrValue(n, "default").value_or("");
            def.desc = attrValue(n, "desc").value_or("");
            fp.contract.attrs.push_back(std::move(def));
        }
    }
}

void parseMessagesDoc(pugi::xml_node root, FileParse& fp, const std::string& file) {
    const ElemSpec rootSpec{"messages", {{"version", true}}, {"msg"}};
    const ElemSpec msgSpec{
        "msg",
        {{"id", true}, {"name", true}, {"dir", true}, {"channel", true}, {"domain", true},
         {"binding", false}, {"desc", false}},
        {"field"}};
    const ElemSpec fieldSpec{"field", {{"name", true}, {"type", true}, {"desc", false}}, {}};

    strictWalk(root, rootSpec, file, fp.buffer, "/messages", fp.issues);
    if (auto v = attrValue(root, "version")) {
        if (!parseUint16(*v, fp.contract.version)) {
            diag(fp.issues, file, 1, "/messages",
                 "version 必须是 0-65535 整数: '" + *v + "'");
        }
    }

    for (pugi::xml_node n : root.children()) {
        if (n.type() != pugi::node_element || std::string(n.name()) != "msg") continue;
        int line = lineOf(fp.buffer, n.offset_debug());
        std::string path = "/messages/msg";
        if (auto nm = attrValue(n, "name")) path += "[@name='" + *nm + "']";
        strictWalk(n, msgSpec, file, fp.buffer, path, fp.issues);

        MsgDef def;
        def.sourceFile = file;
        std::string ids = attrValue(n, "id").value_or("");
        if (!parseUint16(ids, def.id)) {
            diag(fp.issues, file, line, path, "id 必须是 0-65535 整数: '" + ids + "'");
        }
        def.name = attrValue(n, "name").value_or("");
        if (!def.name.empty() && !isIdentifier(def.name)) {
            diag(fp.issues, file, line, path,
                 "msg name 必须匹配 [a-z][a-z0-9_]*: '" + def.name + "'");
        }
        std::string dir = attrValue(n, "dir").value_or("");
        if (dir == "C2S") def.dir = Direction::C2S;
        else if (dir == "S2C") def.dir = Direction::S2C;
        else if (dir == "P2P") def.dir = Direction::P2P;
        else {
            diag(fp.issues, file, line, path,
                 "dir 非法值 '" + dir + "'（合法：C2S S2C P2P）");
        }
        def.channel = attrValue(n, "channel").value_or("");
        if (!def.channel.empty() && !channelSet().count(def.channel)) {
            diag(fp.issues, file, line, path,
                 "channel 非法值 '" + def.channel +
                     "'（合法：movement attributes events control）");
        }
        // 域（sdk-contract §11.3：必填——单值声明，段约束在语义层）
        std::string domain = attrValue(n, "domain").value_or("");
        if (domain == "client") def.domain = MsgDomain::Client;
        else if (domain == "internal") def.domain = MsgDomain::Internal;
        else {
            diag(fp.issues, file, line, path,
                 "domain 非法值 '" + domain + "'（合法：client internal——内外分域必填，"
                 "sdk-contract §11.3）");
        }
        // 绑定路线（§10.6 v3）：缺省按通道（movement/attributes/control=native，
        // events=reflect）；显式声明可覆盖缺省（框架族例外评审把好关）
        if (auto bd = attrValue(n, "binding")) {
            if (*bd == "native") def.binding = MsgBinding::Native;
            else if (*bd == "reflect") def.binding = MsgBinding::Reflect;
            else {
                diag(fp.issues, file, line, path,
                     "binding 非法值 '" + *bd +
                         "'（合法：native reflect；缺省按通道：movement/attributes/"
                         "control=native，events=reflect）");
            }
        } else {
            def.binding = defaultBindingForChannel(def.channel);
        }
        def.desc = attrValue(n, "desc").value_or("");

        std::set<std::string> fieldNames;
        for (pugi::xml_node f : n.children()) {
            if (f.type() != pugi::node_element || std::string(f.name()) != "field") continue;
            strictWalk(f, fieldSpec, file, fp.buffer, path + "/field", fp.issues);
            FieldDef fd;
            fd.name = attrValue(f, "name").value_or("");
            fd.typeName = attrValue(f, "type").value_or("");
            fd.desc = attrValue(f, "desc").value_or("");
            if (!fd.name.empty() && !isIdentifier(fd.name)) {
                int fline = lineOf(fp.buffer, f.offset_debug());
                diag(fp.issues, file, fline, path + "/field",
                     "field name 必须匹配 [a-z][a-z0-9_]*: '" + fd.name + "'");
            }
            if (!fieldNames.insert(fd.name).second) {
                int fline = lineOf(fp.buffer, f.offset_debug());
                diag(fp.issues, file, fline, path + "/field[@name='" + fd.name + "']",
                     "重复字段名 '" + fd.name + "'");
            }
            def.fields.push_back(std::move(fd));
        }
        fp.contract.msgs.push_back(std::move(def));
    }
}

void parseEntitiesDoc(pugi::xml_node root, FileParse& fp, const std::string& file) {
    const ElemSpec rootSpec{"entities", {{"version", true}}, {"entity"}};
    const ElemSpec entitySpec{"entity", {{"id", true}, {"parent", false}, {"desc", false}}, {}};

    strictWalk(root, rootSpec, file, fp.buffer, "/entities", fp.issues);
    if (auto v = attrValue(root, "version")) {
        if (!parseUint16(*v, fp.contract.version)) {
            diag(fp.issues, file, 1, "/entities",
                 "version 必须是 0-65535 整数: '" + *v + "'");
        }
    }

    for (pugi::xml_node n : root.children()) {
        if (n.type() != pugi::node_element || std::string(n.name()) != "entity") continue;
        int line = lineOf(fp.buffer, n.offset_debug());
        std::string name = attrValue(n, "id").value_or("");
        std::string path = "/entities/entity[@id='" + name + "']";
        strictWalk(n, entitySpec, file, fp.buffer, path, fp.issues);
        EntityDef def;
        def.sourceFile = file;
        def.name = name;
        if (!def.name.empty() && !isEntityName(def.name)) {
            diag(fp.issues, file, line, path,
                 "entity id 必须匹配 [A-Z][A-Za-z0-9_]*（PascalCase）: '" + def.name + "'");
        }
        def.parent = attrValue(n, "parent").value_or("");
        if (!def.parent.empty() && !isEntityName(def.parent)) {
            diag(fp.issues, file, line, path,
                 "entity parent 必须匹配 [A-Z][A-Za-z0-9_]*（PascalCase）: '" + def.parent + "'");
        }
        def.desc = attrValue(n, "desc").value_or("");
        if (def.parent == def.name && !def.name.empty()) {
            diag(fp.issues, file, line, path, "entity 不能以自身为 parent（自环）");
        }
        fp.contract.entities.push_back(std::move(def));
    }
}

void parseErrorsDoc(pugi::xml_node root, FileParse& fp, const std::string& file) {
    const ElemSpec rootSpec{"errors", {{"version", true}}, {"error"}};
    const ElemSpec errorSpec{"error", {{"code", true}, {"name", true}, {"desc", false}}, {}};

    strictWalk(root, rootSpec, file, fp.buffer, "/errors", fp.issues);
    if (auto v = attrValue(root, "version")) {
        if (!parseUint16(*v, fp.contract.version)) {
            diag(fp.issues, file, 1, "/errors",
                 "version 必须是 0-65535 整数: '" + *v + "'");
        }
    }

    for (pugi::xml_node n : root.children()) {
        if (n.type() != pugi::node_element || std::string(n.name()) != "error") continue;
        int line = lineOf(fp.buffer, n.offset_debug());
        std::string name = attrValue(n, "name").value_or("");
        std::string path = "/errors/error[@name='" + name + "']";
        strictWalk(n, errorSpec, file, fp.buffer, path, fp.issues);
        ErrorDef def;
        def.sourceFile = file;
        std::string code = attrValue(n, "code").value_or("");
        if (!parseInt32(code, def.code)) {
            diag(fp.issues, file, line, path, "code 必须是 32 位整数: '" + code + "'");
        }
        def.name = name;
        if (!def.name.empty() && !isIdentifier(def.name)) {
            diag(fp.issues, file, line, path,
                 "error name 必须匹配 [a-z][a-z0-9_]*: '" + def.name + "'");
        }
        def.desc = attrValue(n, "desc").value_or("");
        fp.contract.errors.push_back(std::move(def));
    }
}

// ---------------------------------------------------- default value checks

bool defaultFitsType(const std::string& v, WireType t) {
    switch (t) {
        case WireType::Bool:
            return v == "true" || v == "false";
        case WireType::Int32: {
            int32_t dummy = 0;
            return parseInt32(v, dummy);
        }
        case WireType::Int64: {
            int64_t dummy = 0;
            auto [p, ec] = std::from_chars(v.data(), v.data() + v.size(), dummy);
            return ec == std::errc() && p == v.data() + v.size();
        }
        case WireType::Float:
        case WireType::Double: {
            if (v.empty()) return false;
            try {
                size_t pos = 0;
                (void)std::stod(v, &pos);
                return pos == v.size();
            } catch (...) {
                return false;
            }
        }
        case WireType::String:
            return true;  // 任意非换行文本
        case WireType::StringList:
        case WireType::Int64List: {
            if (v.empty()) return true;
            int64_t dummy = 0;
            for (const auto& item : splitList(v.empty() ? "" : v)) {
                if (t == WireType::Int64List) {
                    auto [p, ec] = std::from_chars(item.data(), item.data() + item.size(), dummy);
                    if (ec != std::errc() || p != item.data() + item.size()) return false;
                }
            }
            return true;
        }
        case WireType::Bytes:
            return false;  // bytes 不允许 default（无文本表示）
    }
    return false;
}

bool inAnySegment(uint16_t id) {
    for (const auto& seg : kAttrSegments) {
        if (id >= seg.lo && id <= seg.hi) return true;
    }
    return false;
}

/// 继承 DAG：parent 解析 + 环检测 + 祖先链展开（§16.7.2 生成期拍平的解析侧半步）
void resolveEntities(Contract& c, std::vector<Issue>& issues) {
    std::map<std::string, size_t> byName;
    for (size_t i = 0; i < c.entities.size(); ++i) {
        if (!byName.emplace(c.entities[i].name, i).second) {
            diag(issues, c.entities[i].sourceFile, 0,
                 "/entities/entity[@id='" + c.entities[i].name + "']",
                 "重复实体名 '" + c.entities[i].name + "'");
        }
    }

    // DFS 三色标记环检测；visited 集合缓存祖先链（记忆化）
    std::vector<int> state(c.entities.size(), 0);  // 0=未访 1=在栈 2=完成
    std::vector<std::vector<std::string>> chains(c.entities.size());
    std::function<bool(size_t)> visit = [&](size_t i) -> bool {
        state[i] = 1;
        const std::string& parent = c.entities[i].parent;
        if (!parent.empty()) {
            auto it = byName.find(parent);
            if (it == byName.end()) {
                diag(issues, c.entities[i].sourceFile, 0,
                     "/entities/entity[@id='" + c.entities[i].name + "']",
                     "parent '" + parent + "' 不存在（悬垂引用，xs:keyref 同拦）");
            } else if (state[it->second] == 1) {
                diag(issues, c.entities[i].sourceFile, 0,
                     "/entities/entity[@id='" + c.entities[i].name + "']",
                     "继承环：'" + c.entities[i].name + "' → '" + parent + "'（§16.7.2 环检测）");
            } else if (state[it->second] == 0) {
                if (!visit(it->second)) {
                    // 下游已报环；本链不完整但继续
                }
            }
            if (it != byName.end() && state[it->second] == 2) {
                chains[i] = chains[it->second];
                chains[i].push_back(parent);
            }
        }
        state[i] = 2;
        return true;
    };
    for (size_t i = 0; i < c.entities.size(); ++i) {
        if (state[i] == 0) visit(i);
    }
    for (size_t i = 0; i < c.entities.size(); ++i) {
        c.entities[i].ancestors = chains[i];
    }
}

}  // namespace

// ------------------------------------------------------------ public API

ParseResult parseAttrsXml(std::string_view xml, const std::string& file) {
    FileParse fp = loadDoc(xml, file, "attrs");
    ParseResult r;
    r.contract = std::move(fp.contract);
    r.issues = std::move(fp.issues);
    return r;
}

ParseResult parseMessagesXml(std::string_view xml, const std::string& file) {
    FileParse fp = loadDoc(xml, file, "messages");
    ParseResult r;
    r.contract = std::move(fp.contract);
    r.issues = std::move(fp.issues);
    return r;
}

ParseResult parseEntitiesXml(std::string_view xml, const std::string& file) {
    FileParse fp = loadDoc(xml, file, "entities");
    ParseResult r;
    r.contract = std::move(fp.contract);
    r.issues = std::move(fp.issues);
    return r;
}

ParseResult parseErrorsXml(std::string_view xml, const std::string& file) {
    FileParse fp = loadDoc(xml, file, "errors");
    ParseResult r;
    r.contract = std::move(fp.contract);
    r.issues = std::move(fp.issues);
    return r;
}

uint16_t parseVersionFile(std::string_view text, std::vector<Issue>& issues,
                          const std::string& fileName) {
    std::istringstream ss{std::string(text)};
    std::string token;
    ss >> token;
    if (token.empty()) {
        diag(issues, fileName, 1, "/version", "version 文件为空（期望一行数字）");
        return 0;
    }
    uint16_t v = 0;
    if (!parseUint16(token, v)) {
        diag(issues, fileName, 1, "/version", "version 必须是 0-65535 整数: '" + token + "'");
        return 0;
    }
    std::string extra;
    if (ss >> extra) {
        diag(issues, fileName, 1, "/version", "version 文件含多余内容: '" + extra + "'");
    }
    return v;
}

Contract validateAndResolve(Contract c, std::vector<Issue>& issues) {
    // ---- 别名：唯一、不遮蔽基元、underlying 必须是基元 ----
    std::map<std::string, WireType> typeTable;
    for (const auto& [name, wt] : primitiveMap()) typeTable.emplace(name, wt);
    std::set<std::string> seenAlias;
    for (const auto& alias : c.aliases) {
        if (!seenAlias.insert(alias.name).second) {
            diag(issues, "attrs.xml", 0, "/attrs/alias[@name='" + alias.name + "']",
                 "重复别名 '" + alias.name + "'");
            continue;
        }
        if (primitiveMap().count(alias.name)) {
            diag(issues, "attrs.xml", 0, "/attrs/alias[@name='" + alias.name + "']",
                 "别名不得遮蔽基元类型名 '" + alias.name + "'");
            continue;
        }
        auto it = primitiveMap().find(alias.underlying);
        if (it == primitiveMap().end()) {
            diag(issues, "attrs.xml", 0, "/attrs/alias[@name='" + alias.name + "']",
                 "别名 underlying 必须是基元（不允许别名套别名）: '" + alias.underlying + "'");
            continue;
        }
        typeTable[alias.name] = it->second;
    }

    // ---- 属性：id/name 唯一、分段、类型解析、默认值、sync/predict 组合 ----
    std::set<uint16_t> seenIds;
    std::set<std::string> seenNames;
    for (auto& a : c.attrs) {
        std::string path = "/attrs/attr[@name='" + a.name + "']";
        if (!seenIds.insert(a.id).second) {
            diag(issues, a.sourceFile, 0, path,
                 "重复 attr id " + std::to_string(a.id) + "（xs:key 同拦——C-35 分段冲突无法入库）");
        }
        if (!seenNames.insert(a.name).second) {
            diag(issues, a.sourceFile, 0, path, "重复 attr name '" + a.name + "'");
        }
        if (!inAnySegment(a.id)) {
            diag(issues, a.sourceFile, 0, path,
                 "attr id " + std::to_string(a.id) + " 不在任何分段（docs/05 §6.1：1-899 按类分段）");
        }
        auto it = typeTable.find(a.typeName);
        if (it == typeTable.end()) {
            diag(issues, a.sourceFile, 0, path,
                 "未知类型 '" + a.typeName + "'（基元或 attrs.xml 已声明别名）");
        } else {
            a.type = it->second;
        }
        if (!a.defaultValue.empty()) {
            if (it != typeTable.end() && !defaultFitsType(a.defaultValue, it->second)) {
                diag(issues, a.sourceFile, 0, path,
                     "default '" + a.defaultValue + "' 与类型 '" + a.typeName + "' 不符");
            }
        }
        bool hasNone = false, hasOther = false;
        for (const auto& tok : a.predict) {
            if (tok == "none") hasNone = true;
            else if (tok != "none") hasOther = true;
        }
        if (hasNone && hasOther) {
            diag(issues, a.sourceFile, 0, path, "predict 不允许 none 与其它权限位并存");
        }
        if (!hasNone && a.predict.empty() == false && a.syncMask == kSyncNone) {
            diag(issues, a.sourceFile, 0, path,
                 "predict 属性必须可同步（sync 为空 = SYNC_NONE，预测无从生效）");
        }
    }

    // ---- 消息：id/name 唯一、域分段（§11.3 ①）、字段类型解析（含跨域禁令 ④）----
    std::set<uint16_t> seenMsgIds;
    std::set<std::string> seenMsgNames;
    std::map<std::string, MsgDomain> msgDomainByName;
    for (const auto& m : c.msgs) msgDomainByName.emplace(m.name, m.domain);
    for (auto& m : c.msgs) {
        std::string path = "/messages/msg[@name='" + m.name + "']";
        if (!seenMsgIds.insert(m.id).second) {
            diag(issues, m.sourceFile, 0, path, "重复 msg id " + std::to_string(m.id));
        }
        if (!seenMsgNames.insert(m.name).second) {
            diag(issues, m.sourceFile, 0, path, "重复 msg name '" + m.name + "'");
        }
        const MsgSegment& seg =
            kMsgSegments[m.domain == MsgDomain::Client ? 0 : 1];
        if (m.id < seg.lo || m.id > seg.hi) {
            diag(issues, m.sourceFile, 0, path,
                 "msg id " + std::to_string(m.id) + " 超出 " + msgDomainToString(m.domain) +
                     " 域分段（" + std::to_string(seg.lo) + "-" + std::to_string(seg.hi) +
                     "；client/internal 段内自由增删、互不推动排布——sdk-contract §11.3，"
                     "XSD 表达不了跨属性规则故落本层）");
        }
        for (auto& f : m.fields) {
            auto it = typeTable.find(f.typeName);
            if (it == typeTable.end()) {
                // 跨域引用禁令的先行形态（§11.3 ④）：嵌套消息类型 P2+ 才开放，
                // 当前引用另一 msg 名即「未知类型」；client 域引用 internal 域消息
                // 名给专属诊断——禁令先进校验器，不靠评审自觉。
                auto ref = msgDomainByName.find(f.typeName);
                if (ref != msgDomainByName.end() && ref->second == MsgDomain::Internal &&
                    m.domain == MsgDomain::Client) {
                    diag(issues, m.sourceFile, 0, path + "/field[@name='" + f.name + "']",
                         "跨域引用禁令：client 域消息 '" + m.name + "' 不得引用 internal 域 '" +
                             f.typeName + "'（sdk-contract §11.3 ④）");
                } else {
                    diag(issues, m.sourceFile, 0, path + "/field[@name='" + f.name + "']",
                         "未知类型 '" + f.typeName + "'");
                }
            } else {
                f.type = it->second;
            }
        }
    }

    // ---- 错误码：唯一 + 分区（docs/19 §4.3：0 成功 / 1000-1999 客户端 / 2000+ 服务器）----
    std::set<int32_t> seenCodes;
    std::set<std::string> seenErrNames;
    for (const auto& e : c.errors) {
        std::string path = "/errors/error[@name='" + e.name + "']";
        if (!seenCodes.insert(e.code).second) {
            diag(issues, e.sourceFile, 0, path,
                 "重复错误码 " + std::to_string(e.code) + "（xs:key 同拦）");
        }
        if (!seenErrNames.insert(e.name).second) {
            diag(issues, e.sourceFile, 0, path, "重复错误名 '" + e.name + "'");
        }
        if (e.code != 0 && (e.code < 1000 || e.code > 2999)) {
            diag(issues, e.sourceFile, 0, path,
                 "错误码 " + std::to_string(e.code) +
                     " 超出分区（0 成功 / 1000-1999 客户端 / 2000-2999 服务器，docs/19 §4.3）");
        }
    }

    // ---- 实体：悬垂/环/祖先链 ----
    resolveEntities(c, issues);

    // ---- 规范排序（生成物确定性的前提）----
    std::sort(c.aliases.begin(), c.aliases.end(),
              [](const TypeAlias& a, const TypeAlias& b) { return a.name < b.name; });
    std::sort(c.attrs.begin(), c.attrs.end(),
              [](const AttrDef& a, const AttrDef& b) { return a.id < b.id; });
    std::sort(c.msgs.begin(), c.msgs.end(),
              [](const MsgDef& a, const MsgDef& b) { return a.id < b.id; });
    std::sort(c.entities.begin(), c.entities.end(),
              [](const EntityDef& a, const EntityDef& b) { return a.name < b.name; });
    std::sort(c.errors.begin(), c.errors.end(),
              [](const ErrorDef& a, const ErrorDef& b) { return a.code < b.code; });
    return c;
}

ParseResult parseContractDirectory(const std::string& dir) {
    ParseResult r;
    r.contract.sourceDir = dir;

    auto readFile = [&](const char* name) -> std::optional<std::string> {
        std::string path = dir + "/" + name;
        std::ifstream in{path, std::ios::binary};
        if (!in) {
            diag(r.issues, path, 0, "/", "无法读取契约文件");
            return std::nullopt;
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    };

    Contract& c = r.contract;
    if (auto text = readFile("version")) {
        c.version = parseVersionFile(*text, r.issues, dir + "/version");
    }

    if (auto text = readFile("attrs.xml")) {
        ParseResult p = parseAttrsXml(*text, dir + "/attrs.xml");
        r.issues.insert(r.issues.end(), p.issues.begin(), p.issues.end());
        c.aliases = std::move(p.contract.aliases);
        c.attrs = std::move(p.contract.attrs);
        if (p.contract.version != 0 && p.contract.version != c.version) {
            diag(r.issues, dir + "/attrs.xml", 1, "/attrs",
                 "attrs.xml version=" + std::to_string(p.contract.version) +
                     " 与 version 文件 " + std::to_string(c.version) + " 不一致");
        }
    }
    if (auto text = readFile("messages.xml")) {
        ParseResult p = parseMessagesXml(*text, dir + "/messages.xml");
        r.issues.insert(r.issues.end(), p.issues.begin(), p.issues.end());
        c.msgs = std::move(p.contract.msgs);
        if (p.contract.version != 0 && p.contract.version != c.version) {
            diag(r.issues, dir + "/messages.xml", 1, "/messages",
                 "messages.xml version 与 version 文件不一致");
        }
    }
    if (auto text = readFile("entities.xml")) {
        ParseResult p = parseEntitiesXml(*text, dir + "/entities.xml");
        r.issues.insert(r.issues.end(), p.issues.begin(), p.issues.end());
        c.entities = std::move(p.contract.entities);
        if (p.contract.version != 0 && p.contract.version != c.version) {
            diag(r.issues, dir + "/entities.xml", 1, "/entities",
                 "entities.xml version 与 version 文件不一致");
        }
    }
    if (auto text = readFile("errors.xml")) {
        ParseResult p = parseErrorsXml(*text, dir + "/errors.xml");
        r.issues.insert(r.issues.end(), p.issues.begin(), p.issues.end());
        c.errors = std::move(p.contract.errors);
        if (p.contract.version != 0 && p.contract.version != c.version) {
            diag(r.issues, dir + "/errors.xml", 1, "/errors",
                 "errors.xml version 与 version 文件不一致");
        }
    }

    r.contract = validateAndResolve(std::move(c), r.issues);
    return r;
}

// ------------------------------------------------------------ Issue/ParseResult

std::string Issue::toString() const {
    std::ostringstream os;
    os << file << ":" << line << ": " << path << ": " << message;
    return os.str();
}

// 当前漏斗没有警告级：每条 Issue 都是错误（fail-fast 聚合）。将来引入警告时
// 再加 severity 字段并在此过滤。
size_t ParseResult::errorCount() const { return issues.size(); }

std::string ParseResult::report() const {
    std::ostringstream os;
    for (const auto& i : issues) os << i.toString() << "\n";
    return os.str();
}

}  // namespace apollo::contract
