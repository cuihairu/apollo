/**
 * @file contract_writer.cpp
 * @brief 规范序列化：模型 → 紧凑 XML（一行一属性、固定属性顺序、升序元素）。
 * 输出只依赖模型数据，与输入排版无关——这是 diff 稳定与 hash 稳定的前提。
 */

#include "apollo/contract/contract_writer.hpp"

#include <algorithm>
#include <sstream>
#include <vector>

namespace apollo::contract {

namespace {

// 规范序不信任调用方：即便 Contract 未经 validateAndResolve 排序（手工构造、
// 部分填充），写出前仍在局部副本上排一遍——canonicalBundle/hash 的稳定性
// 只依赖模型内容，不依赖输入顺序。
std::vector<const AttrDef*> sortedAttrs(const Contract& c) {
    std::vector<const AttrDef*> v;
    v.reserve(c.attrs.size());
    for (const auto& a : c.attrs) v.push_back(&a);
    std::sort(v.begin(), v.end(),
              [](const AttrDef* x, const AttrDef* y) { return x->id < y->id; });
    return v;
}

std::vector<const MsgDef*> sortedMsgs(const Contract& c) {
    std::vector<const MsgDef*> v;
    v.reserve(c.msgs.size());
    for (const auto& m : c.msgs) v.push_back(&m);
    std::sort(v.begin(), v.end(),
              [](const MsgDef* x, const MsgDef* y) { return x->id < y->id; });
    return v;
}

std::vector<const EntityDef*> sortedEntities(const Contract& c) {
    std::vector<const EntityDef*> v;
    v.reserve(c.entities.size());
    for (const auto& e : c.entities) v.push_back(&e);
    std::sort(v.begin(), v.end(), [](const EntityDef* x, const EntityDef* y) {
        return x->name < y->name;
    });
    return v;
}

std::vector<const ErrorDef*> sortedErrors(const Contract& c) {
    std::vector<const ErrorDef*> v;
    v.reserve(c.errors.size());
    for (const auto& e : c.errors) v.push_back(&e);
    std::sort(v.begin(), v.end(),
              [](const ErrorDef* x, const ErrorDef* y) { return x->code < y->code; });
    return v;
}

std::vector<const TypeAlias*> sortedAliases(const Contract& c) {
    std::vector<const TypeAlias*> v;
    v.reserve(c.aliases.size());
    for (const auto& a : c.aliases) v.push_back(&a);
    std::sort(v.begin(), v.end(), [](const TypeAlias* x, const TypeAlias* y) {
        return x->name < y->name;
    });
    return v;
}

std::string escapeAttr(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

/// 可选字符串属性：空则整段省略
void optAttr(std::ostringstream& os, const char* name, const std::string& v) {
    if (!v.empty()) os << ' ' << name << "=\"" << escapeAttr(v) << "\"";
}

}  // namespace

std::string writeAttrsXml(const Contract& c) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<attrs version=\"" << c.version << "\">\n";
    for (const auto* alp : sortedAliases(c)) {
        const auto& alias = *alp;
        os << "  <alias name=\"" << alias.name << "\" type=\"" << alias.underlying << "\"";
        optAttr(os, "desc", alias.desc);
        os << "/>\n";
    }
    for (const auto* ap : sortedAttrs(c)) {
        const auto& a = *ap;
        os << "  <attr id=\"" << a.id << "\" name=\"" << a.name << "\" type=\"" << a.typeName
           << "\"";
        if (a.syncMask != kSyncNone) {
            os << " sync=\"";
            bool first = true;
            for (const auto& tok : syncMaskToTokens(a.syncMask)) {
                if (!first) os << ' ';
                os << tok;
                first = false;
            }
            os << '"';
        }
        if (a.channel != "attributes") os << " channel=\"" << a.channel << "\"";
        if (!a.predict.empty()) {
            os << " predict=\"";
            bool first = true;
            for (const auto& tok : a.predict) {
                if (!first) os << ' ';
                os << tok;
                first = false;
            }
            os << '"';
        }
        optAttr(os, "default", a.defaultValue);
        optAttr(os, "desc", a.desc);
        os << "/>\n";
    }
    os << "</attrs>\n";
    return os.str();
}

std::string writeMessagesXml(const Contract& c) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<messages version=\"" << c.version << "\">\n";
    for (const auto* mp : sortedMsgs(c)) {
        const auto& m = *mp;
        os << "  <msg id=\"" << m.id << "\" name=\"" << m.name << "\" dir=\""
           << directionToString(m.dir) << "\" channel=\"" << m.channel << "\"";
        optAttr(os, "desc", m.desc);
        os << ">\n";
        for (const auto& f : m.fields) {
            os << "    <field name=\"" << f.name << "\" type=\"" << f.typeName << "\"";
            optAttr(os, "desc", f.desc);
            os << "/>\n";
        }
        os << "  </msg>\n";
    }
    os << "</messages>\n";
    return os.str();
}

std::string writeEntitiesXml(const Contract& c) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<entities version=\"" << c.version << "\">\n";
    for (const auto* ep : sortedEntities(c)) {
        const auto& e = *ep;
        os << "  <entity id=\"" << e.name << "\"";
        optAttr(os, "parent", e.parent);
        optAttr(os, "desc", e.desc);
        os << "/>\n";
    }
    os << "</entities>\n";
    return os.str();
}

std::string writeErrorsXml(const Contract& c) {
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    os << "<errors version=\"" << c.version << "\">\n";
    for (const auto* ep : sortedErrors(c)) {
        const auto& e = *ep;
        os << "  <error code=\"" << e.code << "\" name=\"" << e.name << "\"";
        optAttr(os, "desc", e.desc);
        os << "/>\n";
    }
    os << "</errors>\n";
    return os.str();
}

std::string canonicalBundle(const Contract& c) {
    std::ostringstream os;
    os << writeAttrsXml(c) << "\n";
    os << writeMessagesXml(c) << "\n";
    os << writeEntitiesXml(c) << "\n";
    os << writeErrorsXml(c) << "\n";
    os << "version:" << c.version << "\n";
    return os.str();
}

}  // namespace apollo::contract
