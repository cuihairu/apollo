using System;

namespace ApolloSDK
{
    /// <summary>
    /// 传输协议类型
    /// </summary>
    public enum TransportProtocol
    {
        Tcp,
        WebSocket,
        Kcp
    }

    /// <summary>
    /// 加密模式
    /// </summary>
    public enum EncryptionMode
    {
        None,
        Aes128,
        Aes256,
        ChaCha20
    }

    /// <summary>
    /// 日志级别
    /// </summary>
    public enum LogLevel
    {
        Error,
        Warning,
        Info,
        Debug,
        Trace
    }

    /// <summary>
    /// Apollo客户端配置
    /// </summary>
    public class ApolloClientConfig
    {
        /// <summary>
        /// 服务器地址
        /// </summary>
        public string ServerAddress { get; set; } = "localhost";

        /// <summary>
        /// 服务器端口
        /// </summary>
        public int Port { get; set; } = 7700;

        /// <summary>
        /// 传输协议
        /// </summary>
        public TransportProtocol Protocol { get; set; } = TransportProtocol.Tcp;

        /// <summary>
        /// 心跳间隔（秒）
        /// </summary>
        public int HeartbeatInterval { get; set; } = 15;

        /// <summary>
        /// 心跳超时时间（秒）
        /// </summary>
        public int HeartbeatTimeout { get; set; } = 45;

        /// <summary>
        /// 连接超时时间（毫秒）
        /// </summary>
        public int ConnectionTimeout { get; set; } = 10000;

        /// <summary>
        /// 加密模式
        /// </summary>
        public EncryptionMode Encryption { get; set; } = EncryptionMode.Aes256;

        /// <summary>
        /// 加密密钥
        /// </summary>
        public string EncryptionKey { get; set; } = "";

        /// <summary>
        /// 资源槽位（用于灰度发布）
        /// </summary>
        public string ResourceSlot { get; set; } = "A";

        /// <summary>
        /// 日志级别
        /// </summary>
        public LogLevel LogLevel { get; set; } = LogLevel.Info;

        /// <summary>
        /// 日志文件路径
        /// </summary>
        public string LogFilePath { get; set; } = "apollo_sdk.log";

        /// <summary>
        /// 是否启用日志上传
        /// </summary>
        public bool EnableLogUpload { get; set; } = false;

        /// <summary>
        /// 日志上传地址
        /// </summary>
        public string LogUploadUrl { get; set; } = "";

        /// <summary>
        /// 最大重连次数
        /// </summary>
        public int MaxReconnectAttempts { get; set; } = 5;

        /// <summary>
        /// 重连延迟（毫秒）
        /// </summary>
        public int ReconnectDelay { get; set; } = 3000;

        /// <summary>
        /// 是否启用指数退避重连
        /// </summary>
        public bool UseExponentialBackoff { get; set; } = true;

        /// <summary>
        /// 消息压缩阈值（字节）
        /// </summary>
        public int CompressionThreshold { get; set; } = 1024;

        /// <summary>
        /// 是否启用消息压缩
        /// </summary>
        public bool EnableCompression { get; set; } = true;

        /// <summary>
        /// 接收缓冲区大小
        /// </summary>
        public int ReceiveBufferSize { get; set; } = 65536;

        /// <summary>
        /// 发送缓冲区大小
        /// </summary>
        public int SendBufferSize { get; set; } = 65536;

        /// <summary>
        /// 是否启用诊断模式
        /// </summary>
        public bool EnableDiagnostics { get; set; } = false;

        /// <summary>
        /// 诊断消息缓存数量
        /// </summary>
        public int DiagnosticMessageCache { get; set; } = 1000;

        /// <summary>
        /// 验证配置有效性
        /// </summary>
        public void Validate()
        {
            if (string.IsNullOrEmpty(ServerAddress))
            {
                throw new ArgumentException("ServerAddress is required");
            }

            if (Port <= 0 || Port > 65535)
            {
                throw new ArgumentException("Port must be between 1 and 65535");
            }

            if (HeartbeatInterval <= 0)
            {
                throw new ArgumentException("HeartbeatInterval must be greater than 0");
            }

            if (HeartbeatTimeout < HeartbeatInterval)
            {
                throw new ArgumentException("HeartbeatTimeout must be greater than or equal to HeartbeatInterval");
            }

            if (ConnectionTimeout <= 0)
            {
                throw new ArgumentException("ConnectionTimeout must be greater than 0");
            }

            if (ReceiveBufferSize <= 0)
            {
                throw new ArgumentException("ReceiveBufferSize must be greater than 0");
            }

            if (SendBufferSize <= 0)
            {
                throw new ArgumentException("SendBufferSize must be greater than 0");
            }

            if (Encryption != EncryptionMode.None && string.IsNullOrEmpty(EncryptionKey))
            {
                throw new ArgumentException("EncryptionKey is required when encryption is enabled");
            }
        }

        /// <summary>
        /// 创建默认配置
        /// </summary>
        /// <returns></returns>
        public static ApolloClientConfig CreateDefault()
        {
            return new ApolloClientConfig();
        }

        /// <summary>
        /// 从配置文件加载
        /// </summary>
        /// <param name="configPath">配置文件路径</param>
        /// <returns></returns>
        public static ApolloClientConfig LoadFromFile(string configPath)
        {
            // TODO: 实现从JSON文件加载配置
            // 可以使用Newtonsoft.Json或Unity的JsonUtility
            return CreateDefault();
        }

        /// <summary>
        /// 保存配置到文件
        /// </summary>
        /// <param name="configPath">配置文件路径</param>
        public void SaveToFile(string configPath)
        {
            // TODO: 实现保存到JSON文件
            // 可以使用Newtonsoft.Json或Unity的JsonUtility
        }
    }
}