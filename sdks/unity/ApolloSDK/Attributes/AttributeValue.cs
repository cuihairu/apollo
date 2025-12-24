using System;
using System.Globalization;

namespace ApolloSDK.Attributes
{
    /// <summary>
    /// 属性值类型枚举
    /// </summary>
    public enum AttributeType : byte
    {
        None = 0,
        Bool = 1,
        Byte = 2,
        Short = 3,
        Int = 4,
        Long = 5,
        Float = 6,
        Double = 7,
        String = 8
    }

    /// <summary>
    /// 属性值包装结构体，支持多种类型
    /// 对应服务端的 ComVal 结构
    /// </summary>
    public struct AttributeValue : IEquatable<AttributeValue>, IComparable<AttributeValue>
    {
        // 类型
        public AttributeType Type { get; private set; }

        // 值存储
        private bool _boolValue;
        private byte _byteValue;
        private short _shortValue;
        private int _intValue;
        private long _longValue;
        private float _floatValue;
        private double _doubleValue;
        private string _stringValue;

        #region 构造函数

        public AttributeValue(bool value) { Type = AttributeType.Bool; _boolValue = value; }
        public AttributeValue(byte value) { Type = AttributeType.Byte; _byteValue = value; }
        public AttributeValue(short value) { Type = AttributeType.Short; _shortValue = value; }
        public AttributeValue(int value) { Type = AttributeType.Int; _intValue = value; }
        public AttributeValue(long value) { Type = AttributeType.Long; _longValue = value; }
        public AttributeValue(float value) { Type = AttributeType.Float; _floatValue = value; }
        public AttributeValue(double value) { Type = AttributeType.Double; _doubleValue = value; }
        public AttributeValue(string value) { Type = AttributeType.String; _stringValue = value; }

        #endregion

        #region 类型转换

        // 隐式转换
        public static implicit operator AttributeValue(bool value) => new AttributeValue(value);
        public static implicit operator AttributeValue(byte value) => new AttributeValue(value);
        public static implicit operator AttributeValue(short value) => new AttributeValue(value);
        public static implicit operator AttributeValue(int value) => new AttributeValue(value);
        public static implicit operator AttributeValue(long value) => new AttributeValue(value);
        public static implicit operator AttributeValue(float value) => new AttributeValue(value);
        public static implicit operator AttributeValue(double value) => new AttributeValue(value);
        public static implicit operator AttributeValue(string value) => new AttributeValue(value);

        // 显式转换回基本类型
        public bool ToBool() => Type == AttributeType.Bool ? _boolValue : throw new InvalidCastException();
        public byte ToByte() => Type == AttributeType.Byte ? _byteValue : throw new InvalidCastException();
        public short ToShort() => Type == AttributeType.Short ? _shortValue : throw new InvalidCastException();
        public int ToInt() => Type == AttributeType.Int ? _intValue : throw new InvalidCastException();
        public long ToLong() => Type == AttributeType.Long ? _longValue : throw new InvalidCastException();
        public float ToFloat() => Type == AttributeType.Float ? _floatValue : throw new InvalidCastException();
        public double ToDouble() => Type == AttributeType.Double ? _doubleValue : throw new InvalidCastException();
        public string ToStringValue() => Type == AttributeType.String ? _stringValue : throw new InvalidCastException();

        // 安全转换，带默认值
        public bool GetBool(bool defaultValue = false) => Type == AttributeType.Bool ? _boolValue : defaultValue;
        public byte GetByte(byte defaultValue = 0) => Type == AttributeType.Byte ? _byteValue : defaultValue;
        public short GetShort(short defaultValue = 0) => Type == AttributeType.Short ? _shortValue : defaultValue;
        public int GetInt(int defaultValue = 0) => Type == AttributeType.Int ? _intValue : defaultValue;
        public long GetLong(long defaultValue = 0) => Type == AttributeType.Long ? _longValue : defaultValue;
        public float GetFloat(float defaultValue = 0f) => Type == AttributeType.Float ? _floatValue : defaultValue;
        public double GetDouble(double defaultValue = 0d) => Type == AttributeType.Double ? _doubleValue : defaultValue;
        public string GetString(string defaultValue = null) => Type == AttributeType.String ? _stringValue : defaultValue;

        #endregion

        #region 运算符重载

        public static AttributeValue operator +(AttributeValue left, AttributeValue right)
        {
            if (left.Type != right.Type) throw new InvalidOperationException("Type mismatch");

            switch (left.Type)
            {
                case AttributeType.Int: return left._intValue + right._intValue;
                case AttributeType.Long: return left._longValue + right._longValue;
                case AttributeType.Float: return left._floatValue + right._floatValue;
                case AttributeType.Double: return left._doubleValue + right._doubleValue;
                case AttributeType.String: return left._stringValue + right._stringValue;
                default: throw new InvalidOperationException($"Operator + not supported for type {left.Type}");
            }
        }

        public static AttributeValue operator -(AttributeValue left, AttributeValue right)
        {
            if (left.Type != right.Type) throw new InvalidOperationException("Type mismatch");

            switch (left.Type)
            {
                case AttributeType.Int: return left._intValue - right._intValue;
                case AttributeType.Long: return left._longValue - right._longValue;
                case AttributeType.Float: return left._floatValue - right._floatValue;
                case AttributeType.Double: return left._doubleValue - right._doubleValue;
                default: throw new InvalidOperationException($"Operator - not supported for type {left.Type}");
            }
        }

        public static AttributeValue operator *(AttributeValue left, AttributeValue right)
        {
            if (left.Type != right.Type) throw new InvalidOperationException("Type mismatch");

            switch (left.Type)
            {
                case AttributeType.Int: return left._intValue * right._intValue;
                case AttributeType.Long: return left._longValue * right._longValue;
                case AttributeType.Float: return left._floatValue * right._floatValue;
                case AttributeType.Double: return left._doubleValue * right._doubleValue;
                default: throw new InvalidOperationException($"Operator * not supported for type {left.Type}");
            }
        }

        public static AttributeValue operator /(AttributeValue left, AttributeValue right)
        {
            if (left.Type != right.Type) throw new InvalidOperationException("Type mismatch");

            switch (left.Type)
            {
                case AttributeType.Int: return left._intValue / right._intValue;
                case AttributeType.Long: return left._longValue / right._longValue;
                case AttributeType.Float: return left._floatValue / right._floatValue;
                case AttributeType.Double: return left._doubleValue / right._doubleValue;
                default: throw new InvalidOperationException($"Operator / not supported for type {left.Type}");
            }
        }

        #endregion

        #region 比较

        public bool Equals(AttributeValue other)
        {
            if (Type != other.Type) return false;

            switch (Type)
            {
                case AttributeType.Bool: return _boolValue == other._boolValue;
                case AttributeType.Byte: return _byteValue == other._byteValue;
                case AttributeType.Short: return _shortValue == other._shortValue;
                case AttributeType.Int: return _intValue == other._intValue;
                case AttributeType.Long: return _longValue == other._longValue;
                case AttributeType.Float: return Math.Abs(_floatValue - other._floatValue) < 0.0001f;
                case AttributeType.Double: return Math.Abs(_doubleValue - other._doubleValue) < 0.0001d;
                case AttributeType.String: return _stringValue == other._stringValue;
                default: return false;
            }
        }

        public override bool Equals(object obj) => obj is AttributeValue other && Equals(other);

        public override int GetHashCode()
        {
            switch (Type)
            {
                case AttributeType.Bool: return _boolValue.GetHashCode();
                case AttributeType.Byte: return _byteValue.GetHashCode();
                case AttributeType.Short: return _shortValue.GetHashCode();
                case AttributeType.Int: return _intValue.GetHashCode();
                case AttributeType.Long: return _longValue.GetHashCode();
                case AttributeType.Float: return _floatValue.GetHashCode();
                case AttributeType.Double: return _doubleValue.GetHashCode();
                case AttributeType.String: return _stringValue?.GetHashCode() ?? 0;
                default: return 0;
            }
        }

        public int CompareTo(AttributeValue other)
        {
            if (Type != other.Type) throw new InvalidOperationException("Cannot compare different types");

            switch (Type)
            {
                case AttributeType.Bool: return _boolValue.CompareTo(other._boolValue);
                case AttributeType.Byte: return _byteValue.CompareTo(other._byteValue);
                case AttributeType.Short: return _shortValue.CompareTo(other._shortValue);
                case AttributeType.Int: return _intValue.CompareTo(other._intValue);
                case AttributeType.Long: return _longValue.CompareTo(other._longValue);
                case AttributeType.Float: return _floatValue.CompareTo(other._floatValue);
                case AttributeType.Double: return _doubleValue.CompareTo(other._doubleValue);
                case AttributeType.String: return string.Compare(_stringValue, other._stringValue, StringComparison.Ordinal);
                default: return 0;
            }
        }

        #endregion

        #region 序列化

        /// <summary>
        /// 从字节数组反序列化
        /// 对应服务端 ComVal::FromBuff
        /// </summary>
        public static AttributeValue FromBytes(byte[] buffer, ref int offset)
        {
            if (offset + 1 > buffer.Length) throw new ArgumentOutOfRangeException(nameof(buffer));

            AttributeType type = (AttributeType)buffer[offset++];
            AttributeValue result;

            switch (type)
            {
                case AttributeType.Bool:
                    result = new AttributeValue(BitConverter.ToBoolean(buffer, offset));
                    offset += 1;
                    break;
                case AttributeType.Byte:
                    result = new AttributeValue(buffer[offset]);
                    offset += 1;
                    break;
                case AttributeType.Short:
                    result = new AttributeValue(BitConverter.ToInt16(buffer, offset));
                    offset += 2;
                    break;
                case AttributeType.Int:
                    result = new AttributeValue(BitConverter.ToInt32(buffer, offset));
                    offset += 4;
                    break;
                case AttributeType.Long:
                    result = new AttributeValue(BitConverter.ToInt64(buffer, offset));
                    offset += 8;
                    break;
                case AttributeType.Float:
                    result = new AttributeValue(BitConverter.ToSingle(buffer, offset));
                    offset += 4;
                    break;
                case AttributeType.Double:
                    result = new AttributeValue(BitConverter.ToDouble(buffer, offset));
                    offset += 8;
                    break;
                case AttributeType.String:
                    short strLen = BitConverter.ToInt16(buffer, offset);
                    offset += 2;
                    result = new AttributeValue(System.Text.Encoding.UTF8.GetString(buffer, offset, strLen));
                    offset += strLen;
                    break;
                default:
                    result = new AttributeValue();
                    break;
            }

            return result;
        }

        /// <summary>
        /// 序列化到字节数组
        /// 对应服务端 ComVal::SaveToBuff
        /// </summary>
        public byte[] ToBytes()
        {
            switch (Type)
            {
                case AttributeType.Bool:
                    return new byte[] { (byte)Type, _boolValue ? (byte)1 : (byte)0 };
                case AttributeType.Byte:
                    return new byte[] { (byte)Type, _byteValue };
                case AttributeType.Short:
                    return new byte[] { (byte)Type }.Concat(BitConverter.GetBytes(_shortValue)).ToArray();
                case AttributeType.Int:
                    return new byte[] { (byte)Type }.Concat(BitConverter.GetBytes(_intValue)).ToArray();
                case AttributeType.Long:
                    return new byte[] { (byte)Type }.Concat(BitConverter.GetBytes(_longValue)).ToArray();
                case AttributeType.Float:
                    return new byte[] { (byte)Type }.Concat(BitConverter.GetBytes(_floatValue)).ToArray();
                case AttributeType.Double:
                    return new byte[] { (byte)Type }.Concat(BitConverter.GetBytes(_doubleValue)).ToArray();
                case AttributeType.String:
                    byte[] strBytes = System.Text.Encoding.UTF8.GetBytes(_stringValue ?? "");
                    short strLen = (short)strBytes.Length;
                    byte[] result = new byte[3 + strLen];
                    result[0] = (byte)Type;
                    Array.Copy(BitConverter.GetBytes(strLen), 0, result, 1, 2);
                    Array.Copy(strBytes, 0, result, 3, strLen);
                    return result;
                default:
                    return new byte[] { (byte)AttributeType.None };
            }
        }

        #endregion

        public override string ToString()
        {
            switch (Type)
            {
                case AttributeType.Bool: return _boolValue.ToString();
                case AttributeType.Byte: return _byteValue.ToString();
                case AttributeType.Short: return _shortValue.ToString();
                case AttributeType.Int: return _intValue.ToString();
                case AttributeType.Long: return _longValue.ToString();
                case AttributeType.Float: return _floatValue.ToString("F2", CultureInfo.InvariantCulture);
                case AttributeType.Double: return _doubleValue.ToString("F2", CultureInfo.InvariantCulture);
                case AttributeType.String: return _stringValue ?? "";
                default: return "None";
            }
        }

        /// <summary>
        /// 克隆
        /// </summary>
        public AttributeValue Clone()
        {
            switch (Type)
            {
                case AttributeType.Bool: return new AttributeValue(_boolValue);
                case AttributeType.Byte: return new AttributeValue(_byteValue);
                case AttributeType.Short: return new AttributeValue(_shortValue);
                case AttributeType.Int: return new AttributeValue(_intValue);
                case AttributeType.Long: return new AttributeValue(_longValue);
                case AttributeType.Float: return new AttributeValue(_floatValue);
                case AttributeType.Double: return new AttributeValue(_doubleValue);
                case AttributeType.String: return new AttributeValue(_stringValue);
                default: return new AttributeValue();
            }
        }
    }
}
