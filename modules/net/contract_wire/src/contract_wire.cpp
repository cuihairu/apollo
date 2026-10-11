#include "apollo/net/wire/contract_wire.hpp"

#include <cstring>

namespace apollo::net::wire {

namespace {

using apollo::contract::gen::kMsgs;
using apollo::contract::gen::WireType;

constexpr std::size_t kU32 = 4;

void put_u32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}

void put_u64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF));
    }
}

void put_text(std::vector<std::uint8_t>& out, const std::string& text) {
    put_u32(out, static_cast<std::uint32_t>(text.size()));
    out.insert(out.end(), text.begin(), text.end());
}

bool get_u32(const std::uint8_t* data, std::size_t size, std::size_t& off, std::uint32_t& v) {
    if (size - off < kU32) {
        return false;
    }
    std::memcpy(&v, data + off, kU32);
    off += kU32;
    return true;
}

bool get_u64(const std::uint8_t* data, std::size_t size, std::size_t& off, std::uint64_t& v) {
    if (size - off < 8) {
        return false;
    }
    std::memcpy(&v, data + off, 8);
    off += 8;
    return true;
}

bool get_text(const std::uint8_t* data, std::size_t size, std::size_t& off, std::string& text) {
    std::uint32_t len = 0;
    if (!get_u32(data, size, off, len)) {
        return false;
    }
    if (size - off < len) {
        return false;
    }
    text.assign(reinterpret_cast<const char*>(data + off), len);
    off += len;
    return true;
}

bool encode_field(const apollo::contract::gen::MsgField& field, const FieldValue& value,
                  std::vector<std::uint8_t>& out) {
    switch (field.type) {
        case WireType::Bool:
            if (value.integer != 0 && value.integer != 1) {
                return false;
            }
            out.push_back(static_cast<std::uint8_t>(value.integer));
            return true;
        case WireType::Int32:
            if (value.integer < INT32_MIN || value.integer > INT32_MAX) {
                return false;
            }
            put_u32(out, static_cast<std::uint32_t>(static_cast<std::int32_t>(value.integer)));
            return true;
        case WireType::Int64:
            put_u64(out, static_cast<std::uint64_t>(value.integer));
            return true;
        case WireType::Float: {
            const float f = static_cast<float>(value.real);
            std::uint32_t bits = 0;
            std::memcpy(&bits, &f, 4);
            put_u32(out, bits);
            return true;
        }
        case WireType::Double: {
            std::uint64_t bits = 0;
            std::memcpy(&bits, &value.real, 8);
            put_u64(out, bits);
            return true;
        }
        case WireType::String:
            put_text(out, value.text);
            return true;
        case WireType::Bytes:
            put_u32(out, static_cast<std::uint32_t>(value.bytes.size()));
            out.insert(out.end(), value.bytes.begin(), value.bytes.end());
            return true;
        case WireType::Int64List:
            put_u32(out, static_cast<std::uint32_t>(value.integers.size()));
            for (const auto v : value.integers) {
                put_u64(out, static_cast<std::uint64_t>(v));
            }
            return true;
        case WireType::StringList:
            put_u32(out, static_cast<std::uint32_t>(value.strings.size()));
            for (const auto& s : value.strings) {
                put_text(out, s);
            }
            return true;
    }
    return false;
}

bool decode_field(WireType type, const std::uint8_t* data, std::size_t size, std::size_t& off,
                  FieldValue& value) {
    switch (type) {
        case WireType::Bool: {
            if (size - off < 1) {
                return false;
            }
            value.integer = data[off++];
            return true;
        }
        case WireType::Int32: {
            std::uint32_t raw = 0;
            if (!get_u32(data, size, off, raw)) {
                return false;
            }
            value.integer = static_cast<std::int32_t>(raw);
            return true;
        }
        case WireType::Int64: {
            std::uint64_t raw = 0;
            if (!get_u64(data, size, off, raw)) {
                return false;
            }
            value.integer = static_cast<std::int64_t>(raw);
            return true;
        }
        case WireType::Float: {
            std::uint32_t bits = 0;
            if (!get_u32(data, size, off, bits)) {
                return false;
            }
            float f = 0;
            std::memcpy(&f, &bits, 4);
            value.real = f;
            return true;
        }
        case WireType::Double: {
            std::uint64_t bits = 0;
            if (!get_u64(data, size, off, bits)) {
                return false;
            }
            std::memcpy(&value.real, &bits, 8);
            return true;
        }
        case WireType::String:
            return get_text(data, size, off, value.text);
        case WireType::Bytes: {
            std::uint32_t len = 0;
            if (!get_u32(data, size, off, len) || size - off < len) {
                return false;
            }
            value.bytes.assign(data + off, data + off + len);
            off += len;
            return true;
        }
        case WireType::Int64List: {
            std::uint32_t count = 0;
            if (!get_u32(data, size, off, count)) {
                return false;
            }
            if ((size - off) / 8 < count) {
                return false;
            }
            value.integers.resize(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                std::uint64_t raw = 0;
                if (!get_u64(data, size, off, raw)) {
                    return false;
                }
                value.integers[i] = static_cast<std::int64_t>(raw);
            }
            return true;
        }
        case WireType::StringList: {
            std::uint32_t count = 0;
            if (!get_u32(data, size, off, count)) {
                return false;
            }
            value.strings.resize(count);
            for (std::uint32_t i = 0; i < count; ++i) {
                if (!get_text(data, size, off, value.strings[i])) {
                    return false;
                }
            }
            return true;
        }
    }
    return false;
}

}  // namespace

const apollo::contract::gen::MsgMeta* find_msg(std::uint16_t msg_id) {
    for (const auto& meta : kMsgs) {
        if (static_cast<std::uint16_t>(meta.id) == msg_id) {
            return &meta;
        }
    }
    return nullptr;
}

bool encode_payload(std::uint16_t msg_id, const std::vector<FieldValue>& fields,
                    std::vector<std::uint8_t>& out) {
    out.clear();
    const auto* meta = find_msg(msg_id);
    if (meta == nullptr || fields.size() != meta->fieldCount) {
        return false;
    }
    out.reserve(64);
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (!encode_field(meta->fields[i], fields[i], out)) {
            out.clear();
            return false;
        }
    }
    return true;
}

std::vector<std::uint8_t> append_envelope(std::uint16_t msg_id,
                                          const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> out;
    out.reserve(2 + payload.size());
    out.push_back(static_cast<std::uint8_t>(msg_id & 0xFF));
    out.push_back(static_cast<std::uint8_t>((msg_id >> 8) & 0xFF));
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

bool decode_envelope(const std::uint8_t* data, std::size_t size, std::uint16_t& msg_id,
                     std::vector<FieldValue>& fields) {
    fields.clear();
    if (size < 2) {
        return false;
    }
    msg_id = static_cast<std::uint16_t>(data[0]) | static_cast<std::uint16_t>(data[1] << 8);
    const bool ok = decode_payload(msg_id, data + 2, size - 2, fields);
    if (!ok) {
        fields.clear();
    }
    return ok;
}

bool decode_payload(std::uint16_t msg_id, const std::uint8_t* data, std::size_t size,
                    std::vector<FieldValue>& fields) {
    fields.clear();
    const auto* meta = find_msg(msg_id);
    if (meta == nullptr) {
        return false;
    }
    fields.resize(meta->fieldCount);
    std::size_t off = 0;
    for (std::size_t i = 0; i < meta->fieldCount; ++i) {
        if (!decode_field(meta->fields[i].type, data, size, off, fields[i])) {
            fields.clear();
            return false;
        }
    }
    if (off != size) {
        fields.clear();
        return false;
    }
    return true;
}

}  // namespace apollo::net::wire
