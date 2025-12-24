#pragma once

#include <cstdint>
#include <cstring>
#include <string>
#include <algorithm>
#include <stdexcept>

namespace apollo {
namespace game {

/// 公用值类型枚举
enum class EComValType : uint8_t {
    ECVT_NULL = 0,    ///< 空值
    ECVT_BOOL,        ///< bool
    ECVT_BYTE,        ///< uint8_t
    ECVT_INT,         ///< int32_t
    ECVT_WORD,        ///< uint16_t
    ECVT_DWORD,       ///< uint32_t
    ECVT_INT64,       ///< int64_t
    ECVT_DOUBLE,      ///< double
    ECVT_STRING,      ///< std::string
    ECVT_PTR          ///< void*
};

/**
 * @brief 公用值类型
 *
 * 支持多种数据类型的统一包装，对应服务端的 ComVal/uComVal
 * 用于属性系统的值存储和传输
 */
class ComVal {
public:
    // ========== 构造函数 ==========

    ComVal() : type_(EComValType::ECVT_NULL), int64_(0) {}

    // 基础类型构造
    ComVal(bool val) : type_(EComValType::ECVT_BOOL), bool_(val) {}
    ComVal(uint8_t val) : type_(EComValType::ECVT_BYTE), byte_(val) {}
    ComVal(int16_t val) : type_(EComValType::ECVT_INT), int_(val) {}
    ComVal(uint16_t val) : type_(EComValType::ECVT_WORD), word_(val) {}
    ComVal(int32_t val) : type_(EComValType::ECVT_INT), int_(val) {}
    ComVal(uint32_t val) : type_(EComValType::ECVT_DWORD), dword_(val) {}
    ComVal(int64_t val) : type_(EComValType::ECVT_INT64), int64_(val) {}
    ComVal(double val) : type_(EComValType::ECVT_DOUBLE), double_(val) {}

    // 字符串构造
    ComVal(const char* val) : type_(EComValType::ECVT_STRING), int64_(0) {
        if (val) {
            stringLen_ = static_cast<size_t>(strlen(val));
            if (stringLen_ > 0) {
                string_ = new char[stringLen_ + 1];
                std::memcpy(string_, val, stringLen_);
                string_[stringLen_] = '\0';
            } else {
                string_ = const_cast<char*>("");
            }
        } else {
            string_ = const_cast<char*>("");
            stringLen_ = 0;
        }
    }

    ComVal(const std::string& val) : type_(EComValType::ECVT_STRING), int64_(0) {
        stringLen_ = val.size();
        if (stringLen_ > 0) {
            string_ = new char[stringLen_ + 1];
            std::memcpy(string_, val.c_str(), stringLen_);
            string_[stringLen_] = '\0';
        } else {
            string_ = const_cast<char*>("");
        }
    }

    // 拷贝构造
    ComVal(const ComVal& other) : type_(EComValType::ECVT_NULL), int64_(0) {
        assign(other);
    }

    // 移动构造
    ComVal(ComVal&& other) noexcept : type_(other.type_), int64_(other.int64_) {
        if (other.type_ == EComValType::ECVT_STRING && other.stringLen_ > 0) {
            // 窃取字符串指针
            string_ = other.string_;
            other.string_ = const_cast<char*>("");
            other.stringLen_ = 0;
        }
        other.type_ = EComValType::ECVT_NULL;
        other.int64_ = 0;
    }

    // 析构函数
    ~ComVal() {
        clear();
    }

    // ========== 赋值运算符 ==========

    ComVal& operator=(const ComVal& other) {
        if (this != &other) {
            clear();
            assign(other);
        }
        return *this;
    }

    ComVal& operator=(ComVal&& other) noexcept {
        if (this != &other) {
            clear();
            type_ = other.type_;
            int64_ = other.int64_;

            if (other.type_ == EComValType::ECVT_STRING && other.stringLen_ > 0) {
                string_ = other.string_;
                other.string_ = const_cast<char*>("");
                other.stringLen_ = 0;
            }

            other.type_ = EComValType::ECVT_NULL;
            other.int64_ = 0;
        }
        return *this;
    }

    ComVal& operator=(bool val) { setBool(val); return *this; }
    ComVal& operator=(uint8_t val) { setByte(val); return *this; }
    ComVal& operator=(int16_t val) { setInt(val); return *this; }
    ComVal& operator=(uint16_t val) { setWord(val); return *this; }
    ComVal& operator=(int32_t val) { setInt(val); return *this; }
    ComVal& operator=(uint32_t val) { setDword(val); return *this; }
    ComVal& operator=(int64_t val) { setInt64(val); return *this; }
    ComVal& operator=(double val) { setDouble(val); return *this; }
    ComVal& operator=(const char* val) { setString(val); return *this; }
    ComVal& operator=(const std::string& val) { setString(val); return *this; }

    // ========== 类型获取 ==========

    EComValType getType() const { return type_; }

    // 类型检查
    bool isNull() const { return type_ == EComValType::ECVT_NULL; }
    bool isBool() const { return type_ == EComValType::ECVT_BOOL; }
    isInt8_t() const { return type_ == EComValType::ECVT_BYTE; }
    isInt16() const { return type_ == EComValType::ECVT_INT; }
    isUInt16() const { return type_ == EComValType::ECVT_WORD; }
    isInt32() const { return type_ == EComValType::ECVT_INT; }
    isUInt32() const { return type_ == EComValType::ECVT_DWORD; }
    isInt64() const { return type_ == EComValType::ECVT_INT64; }
    isDouble() const { return type_ == EComValType::ECVT_DOUBLE; }
    isString() const { return type_ == EComValType::ECVT_STRING; }

    // ========== 值获取 ==========

    bool getBool(bool defaultValue = false) const {
        return type_ == EComValType::ECVT_BOOL ? bool_ : defaultValue;
    }

    uint8_t getByte(uint8_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_BYTE ? byte_ : defaultValue;
    }

    int16_t getShort(int16_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_INT ? int_ : defaultValue;
    }

    uint16_t getWord(uint16_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_WORD ? word_ : defaultValue;
    }

    int32_t getInt(int32_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_INT ? int_ : defaultValue;
    }

    uint32_t getDword(uint32_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_DWORD ? dword_ : defaultValue;
    }

    int64_t getInt64(int64_t defaultValue = 0) const {
        return type_ == EComValType::ECVT_INT64 ? int64_ : defaultValue;
    }

    double getDouble(double defaultValue = 0.0) const {
        return type_ == EComValType::ECVT_DOUBLE ? double_ : defaultValue;
    }

    const char* getString(const char* defaultValue = "") const {
        return type_ == EComValType::ECVT_STRING ? string_ : defaultValue;
    }

    std::string getStringStd(const std::string& defaultValue = "") const {
        return type_ == EComValType::ECVT_STRING ? std::string(string_, stringLen_) : defaultValue;
    }

    // ========== 值设置 ==========

    void setNull() {
        clear();
        type_ = EComValType::ECVT_NULL;
    }

    void setBool(bool val) {
        clear();
        type_ = EComValType::ECVT_BOOL;
        bool_ = val;
    }

    void setByte(uint8_t val) {
        clear();
        type_ = EComValType::ECVT_BYTE;
        byte_ = val;
    }

    void setInt(int32_t val) {
        clear();
        type_ = EComValType::ECVT_INT;
        int_ = val;
    }

    void setWord(uint16_t val) {
        clear();
        type_ = EComValType::ECVT_WORD;
        word_ = val;
    }

    void setDword(uint32_t val) {
        clear();
        type_ = EComValType::ECVT_DWORD;
        dword_ = val;
    }

    void setInt64(int64_t val) {
        clear();
        type_ = EComValType::ECVT_INT64;
        int64_ = val;
    }

    void setDouble(double val) {
        clear();
        type_ = EComValType::ECVT_DOUBLE;
        double_ = val;
    }

    void setString(const char* val) {
        clear();
        type_ = EComValType::ECVT_STRING;
        if (val) {
            stringLen_ = static_cast<size_t>(strlen(val));
            if (stringLen_ > 0) {
                string_ = new char[stringLen_ + 1];
                std::memcpy(string_, val, stringLen_);
                string_[stringLen_] = '\0';
            } else {
                string_ = const_cast<char*>("");
            }
        } else {
            string_ = const_cast<char*>("");
            stringLen_ = 0;
        }
    }

    void setString(const std::string& val) {
        clear();
        type_ = EComValType::ECVT_STRING;
        stringLen_ = val.size();
        if (stringLen_ > 0) {
            string_ = new char[stringLen_ + 1];
            std::memcpy(string_, val.c_str(), stringLen_);
            string_[stringLen_] = '\0';
        } else {
            string_ = const_cast<char*>("");
        }
    }

    // ========== 运算符 ==========

    bool operator==(const ComVal& other) const {
        if (type_ != other.type_) return false;

        switch (type_) {
            case EComValType::ECVT_DOUBLE:
                return std::abs(double_ - other.double_) < 0.00001;
            case EComValType::ECVT_STRING:
                return (stringLen_ == other.stringLen_) &&
                       (stringLen_ == 0 || std::memcmp(string_, other.string_, stringLen_) == 0);
            default:
                return int64_ == other.int64_;
        }
    }

    bool operator!=(const ComVal& other) const {
        return !(*this == other);
    }

    bool operator<(const ComVal& other) const {
        if (type_ != other.type_) return false;

        switch (type_) {
            case EComValType::ECVT_DOUBLE:
                return double_ < other.double_;
            case EComValType::ECVT_STRING:
                return std::lexicographical_compare(string_, string_ + stringLen_,
                                                     other.string_, other.string_ + other.stringLen_);
            default:
                return int64_ < other.int64_;
        }
    }

    bool operator<=(const ComVal& other) const { return !(other < *this); }
    bool operator>(const ComVal& other) const { return other < *this; }
    bool operator>=(const ComVal& other) const { return !(*this < other); }

    // ========== 算术运算符 ==========

    ComVal operator+(const ComVal& other) const {
        if (type_ != other.type_) throw std::runtime_error("Type mismatch in addition");

        switch (type_) {
            case EComValType::ECVT_INT: return ComVal(int_ + other.int_);
            case EComValType::ECVT_DWORD: return ComVal(dword_ + other.dword_);
            case EComValType::ECVT_INT64: return ComVal(int64_ + other.int64_);
            case EComValType::ECVT_DOUBLE: return ComVal(double_ + other.double_);
            case EComValType::ECVT_STRING:
                return ComVal(getStringStd() + other.getStringStd());
            default:
                throw std::runtime_error("Addition not supported for this type");
        }
    }

    ComVal operator-(const ComVal& other) const {
        if (type_ != other.type_) throw std::runtime_error("Type mismatch in subtraction");

        switch (type_) {
            case EComValType::ECVT_INT: return ComVal(int_ - other.int_);
            case EComValType::ECVT_DWORD: return ComVal(dword_ - other.dword_);
            case EComValType::ECVT_INT64: return ComVal(int64_ - other.int64_);
            case EComValType::ECVT_DOUBLE: return ComVal(double_ - other.double_);
            default:
                throw std::runtime_error("Subtraction not supported for this type");
        }
    }

    ComVal operator*(const ComVal& other) const {
        if (type_ != other.type_) throw std::runtime_error("Type mismatch in multiplication");

        switch (type_) {
            case EComValType::ECVT_INT: return ComVal(int_ * other.int_);
            case EComValType::ECVT_DWORD: return ComVal(dword_ * other.dword_);
            case EComValType::ECVT_INT64: return ComVal(int64_ * other.int64_);
            case EComValType::ECVT_DOUBLE: return ComVal(double_ * other.double_);
            default:
                throw std::runtime_error("Multiplication not supported for this type");
        }
    }

    ComVal operator/(const ComVal& other) const {
        if (type_ != other.type_) throw std::runtime_error("Type mismatch in division");

        switch (type_) {
            case EComValType::ECVT_INT: return ComVal(int_ / other.int_);
            case EComValType::ECVT_DWORD: return ComVal(dword_ / other.dword_);
            case EComValType::ECVT_INT64: return ComVal(int64_ / other.int64_);
            case EComValType::ECVT_DOUBLE: return ComVal(double_ / other.double_);
            default:
                throw std::runtime_error("Division not supported for this type");
        }
    }

    // ========== 序列化 ==========

    /**
     * @brief 序列化到缓冲区
     * @param buf 输出缓冲区
     * @param bufSize 缓冲区大小
     * @param offset 起始偏移量，写入后更新
     * @return 是否成功
     */
    bool saveToBuff(char* buf, int bufSize, int& offset) const {
        if (!buf || offset < 0) return false;

        // 写入类型
        if (offset + 1 > bufSize) return false;
        buf[offset++] = static_cast<char>(type_);

        switch (type_) {
            case EComValType::ECVT_BOOL:
            case EComValType::ECVT_BYTE:
                if (offset + 1 > bufSize) return false;
                buf[offset++] = byte_;
                break;

            case EComValType::ECVT_WORD:
                if (offset + 2 > bufSize) return false;
                std::memcpy(buf + offset, &word_, 2);
                offset += 2;
                break;

            case EComValType::ECVT_INT:
                if (offset + 4 > bufSize) return false;
                std::memcpy(buf + offset, &int_, 4);
                offset += 4;
                break;

            case EComValType::ECVT_DWORD:
                if (offset + 4 > bufSize) return false;
                std::memcpy(buf + offset, &dword_, 4);
                offset += 4;
                break;

            case EComValType::ECVT_INT64:
                if (offset + 8 > bufSize) return false;
                std::memcpy(buf + offset, &int64_, 8);
                offset += 8;
                break;

            case EComValType::ECVT_DOUBLE:
                if (offset + 8 > bufSize) return false;
                std::memcpy(buf + offset, &double_, 8);
                offset += 8;
                break;

            case EComValType::ECVT_STRING:
                if (offset + 2 + stringLen_ > bufSize) return false;
                uint16_t len = static_cast<uint16_t>(stringLen_);
                std::memcpy(buf + offset, &len, 2);
                offset += 2;
                if (stringLen_ > 0) {
                    std::memcpy(buf + offset, string_, stringLen_);
                    offset += stringLen_;
                }
                break;

            default:
                break;
        }

        return true;
    }

    /**
     * @brief 从缓冲区反序列化
     * @param buf 输入缓冲区
     * @param bufSize 缓冲区大小
     * @param offset 起始偏移量，读取后更新
     * @return 是否成功
     */
    bool fromBuff(const char* buf, int bufSize, int& offset) {
        if (!buf || bufSize < 1 || offset < 0) return false;

        clear();

        // 读取类型
        type_ = static_cast<EComValType>(buf[offset++]);

        switch (type_) {
            case EComValType::ECVT_BOOL:
            case EComValType::ECVT_BYTE:
                if (offset + 1 > bufSize) return false;
                byte_ = buf[offset++];
                break;

            case EComValType::ECVT_WORD:
                if (offset + 2 > bufSize) return false;
                std::memcpy(&word_, buf + offset, 2);
                offset += 2;
                break;

            case EComValType::ECVT_INT:
                if (offset + 4 > bufSize) return false;
                std::memcpy(&int_, buf + offset, 4);
                offset += 4;
                break;

            case EComValType::ECVT_DWORD:
                if (offset + 4 > bufSize) return false;
                std::memcpy(&dword_, buf + offset, 4);
                offset += 4;
                break;

            case EComValType::ECVT_INT64:
                if (offset + 8 > bufSize) return false;
                std::memcpy(&int64_, buf + offset, 8);
                offset += 8;
                break;

            case EComValType::ECVT_DOUBLE:
                if (offset + 8 > bufSize) return false;
                std::memcpy(&double_, buf + offset, 8);
                offset += 8;
                break;

            case EComValType::ECVT_STRING: {
                if (offset + 2 > bufSize) return false;
                uint16_t len;
                std::memcpy(&len, buf + offset, 2);
                offset += 2;

                if (offset + len > bufSize) return false;
                stringLen_ = len;

                if (len > 0) {
                    string_ = new char[len + 1];
                    std::memcpy(string_, buf + offset, len);
                    string_[len] = '\0';
                    offset += len;
                } else {
                    string_ = const_cast<char*>("");
                }
                break;
            }

            default:
                type_ = EComValType::ECVT_NULL;
                break;
        }

        return true;
    }

    // ========== 其他 ==========

    void reset() {
        clear();
        type_ = EComValType::ECVT_NULL;
        int64_ = 0;
    }

    std::string toString() const {
        switch (type_) {
            case EComValType::ECVT_BOOL: return bool_ ? "true" : "false";
            case EComValType::ECVT_BYTE: return std::to_string(static_cast<int>(byte_));
            case EComValType::ECVT_INT: return std::to_string(int_);
            case EComValType::ECVT_WORD: return std::to_string(word_);
            case EComValType::ECVT_DWORD: return std::to_string(dword_);
            case EComValType::ECVT_INT64: return std::to_string(int64_);
            case EComValType::ECVT_DOUBLE: return std::to_string(double_);
            case EComValType::ECVT_STRING: return std::string(string_, stringLen_);
            default: return "null";
        }
    }

private:
    void clear() {
        if (type_ == EComValType::ECVT_STRING && stringLen_ > 0 && string_) {
            delete[] string_;
        }
        type_ = EComValType::ECVT_NULL;
        int64_ = 0;
    }

    void assign(const ComVal& other) {
        type_ = other.type_;
        int64_ = other.int64_;

        if (other.type_ == EComValType::ECVT_STRING && other.stringLen_ > 0) {
            stringLen_ = other.stringLen_;
            string_ = new char[stringLen_ + 1];
            std::memcpy(string_, other.string_, stringLen_);
            string_[stringLen_] = '\0';
        }
    }

    EComValType type_;

    union {
        bool    bool_;
        uint8_t byte_;
        int16_t short_;
        int32_t int_;
        uint16_t word_;
        uint32_t dword_;
        int64_t  int64_;
        double   double_;
        struct {
            char* string_;
            size_t stringLen_;
        };
        void* ptr_;
    };
};

/// 空值常量
extern const ComVal NullComVal;

} // namespace game
} // namespace apollo
