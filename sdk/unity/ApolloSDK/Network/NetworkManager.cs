using System;
using System.Net.Sockets;
using System.Threading;
using System.Threading.Tasks;
using System.Collections.Generic;
using ApolloSDK.Utilities;

namespace ApolloSDK.Network
{
    /// <summary>
    /// 网络统计信息
    /// </summary>
    public class NetworkStats
    {
        public long BytesSent { get; set; }
        public long BytesReceived { get; set; }
        public int PacketsSent { get; set; }
        public int PacketsReceived { get; set; }
        public int ReconnectCount { get; set; }
        public TimeSpan LastPingTime { get; set; }
        public DateTime LastActivityTime { get; set; }
    }

    /// <summary>
    /// 网络管理器
    /// </summary>
    public class NetworkManager : IDisposable
    {
        private ApolloClientConfig config;
        private INetworkTransport transport;
        private PacketEncoder encoder;
        private PacketDecoder decoder;
        private bool disposed = false;
        private int reconnectAttempts = 0;

        // 网络状态
        private volatile bool isConnected = false;
        private volatile bool isConnecting = false;

        // 统计信息
        private NetworkStats stats = new NetworkStats();

        // 发送队列（用于管理待发送的消息）
        private readonly Queue<Packet> sendQueue = new Queue<Packet>();
        private readonly object sendQueueLock = new object();

        // 事件
        public event Action OnConnected;
        public event Action OnDisconnected;
        public event Action<string> OnError;

        /// <summary>
        /// 是否已连接
        /// </summary>
        public bool IsConnected => isConnected;

        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="config">配置</param>
        public NetworkManager(ApolloClientConfig config)
        {
            this.config = config ?? throw new ArgumentNullException(nameof(config));

            InitializeTransport();
            InitializeCodec();
        }

        /// <summary>
        /// 异步连接服务器
        /// </summary>
        public async Task<bool> ConnectAsync(CancellationToken cancellationToken = default)
        {
            if (isConnected || isConnecting)
            {
                return isConnected;
            }

            isConnecting = true;

            try
            {
                Logger.Info($"Connecting to {config.ServerAddress}:{config.Port} using {config.Protocol}");

                // 创建传输层
                await transport.ConnectAsync(config.ServerAddress, config.Port, cancellationToken);

                isConnected = true;
                reconnectAttempts = 0;

                // 启动接收循环
                _ = Task.Run(ReceiveLoop);

                // 启动发送循环
                _ = Task.Run(SendLoop);

                Logger.Info("Connected to server");
                OnConnected?.Invoke();

                return true;
            }
            catch (Exception ex)
            {
                Logger.Error($"Connection failed: {ex.Message}");
                OnError?.Invoke($"Connection failed: {ex.Message}");
                return false;
            }
            finally
            {
                isConnecting = false;
            }
        }

        /// <summary>
        /// 异步断开连接
        /// </summary>
        public async Task DisconnectAsync()
        {
            if (!isConnected)
            {
                return;
            }

            isConnected = false;

            try
            {
                await transport.DisconnectAsync();
                Logger.Info("Disconnected from server");
                OnDisconnected?.Invoke();
            }
            catch (Exception ex)
            {
                Logger.Error($"Disconnect error: {ex.Message}");
            }
        }

        /// <summary>
        /// 异步发送消息
        /// </summary>
        public async Task<bool> SendAsync<T>(ushort messageId, T message) where T : class, IMessage
        {
            if (!isConnected)
            {
                Logger.Warning("Cannot send message: not connected");
                return false;
            }

            try
            {
                // 序列化消息
                var data = ProtobufSerializer.Serialize(message);

                // 编码数据包
                var packet = new Packet
                {
                    MessageId = messageId,
                    Data = data,
                    Timestamp = DateTime.UtcNow
                };

                // 加密和压缩
                packet = await ProcessOutgoingPacket(packet);

                // 加入发送队列
                lock (sendQueueLock)
                {
                    sendQueue.Enqueue(packet);
                }

                Logger.Debug($"Message queued for send: {messageId}");
                return true;
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to send message {messageId}: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 获取网络统计信息
        /// </summary>
        public NetworkStats GetStats()
        {
            return stats;
        }

        /// <summary>
        /// 执行Ping测试
        /// </summary>
        public async Task<TimeSpan> PingAsync()
        {
            if (!isConnected)
            {
                return TimeSpan.MaxValue;
            }

            var startTime = DateTime.UtcNow;

            // 发送ping消息
            var pingPacket = new Packet
            {
                MessageId = MessageIds.PING,
                Timestamp = startTime
            };

            // TODO: 实现特殊的ping消息处理
            // 这里简化实现，直接返回延迟
            return DateTime.UtcNow - startTime;
        }

        #region Private Methods

        /// <summary>
        /// 初始化传输层
        /// </summary>
        private void InitializeTransport()
        {
            switch (config.Protocol)
            {
                case TransportProtocol.Tcp:
                    transport = new TcpTransport();
                    break;
                case TransportProtocol.WebSocket:
                    transport = new WebSocketTransport();
                    break;
                case TransportProtocol.Kcp:
                    transport = new KcpTransport();
                    break;
                default:
                    throw new NotSupportedException($"Protocol {config.Protocol} is not supported");
            }

            transport.Configure(new TransportConfig
            {
                ReceiveBufferSize = config.ReceiveBufferSize,
                SendBufferSize = config.SendBufferSize,
                ConnectionTimeout = config.ConnectionTimeout
            });
        }

        /// <summary>
        /// 初始化编解码器
        /// </summary>
        private void InitializeCodec()
        {
            encoder = new PacketEncoder(config);
            decoder = new PacketDecoder(config);
        }

        /// <summary>
        /// 处理出站数据包
        /// </summary>
        private async Task<Packet> ProcessOutgoingPacket(Packet packet)
        {
            // 压缩
            if (config.EnableCompression && packet.Data.Length >= config.CompressionThreshold)
            {
                packet.Data = await Compressor.CompressAsync(packet.Data);
                packet.Flags |= PacketFlags.Compressed;
            }

            // 加密
            if (config.Encryption != EncryptionMode.None)
            {
                packet.Data = await Encryptor.EncryptAsync(packet.Data, config.EncryptionKey);
                packet.Flags |= PacketFlags.Encrypted;
            }

            // 添加校验和
            packet.Checksum = Checksum.Compute(packet.Data);

            return packet;
        }

        /// <summary>
        /// 处理入站数据包
        /// </summary>
        private async Task<Packet> ProcessIncomingPacket(Packet packet)
        {
            // 验证校验和
            if (!Checksum.Verify(packet.Data, packet.Checksum))
            {
                throw new InvalidOperationException("Invalid packet checksum");
            }

            // 解密
            if ((packet.Flags & PacketFlags.Encrypted) != 0)
            {
                packet.Data = await Encryptor.DecryptAsync(packet.Data, config.EncryptionKey);
            }

            // 解压缩
            if ((packet.Flags & PacketFlags.Compressed) != 0)
            {
                packet.Data = await Compressor.DecompressAsync(packet.Data);
            }

            return packet;
        }

        /// <summary>
        /// 接收循环
        /// </summary>
        private async Task ReceiveLoop()
        {
            while (isConnected)
            {
                try
                {
                    // 接收数据
                    var data = await transport.ReceiveAsync();
                    if (data == null || data.Length == 0)
                    {
                        break;
                    }

                    // 更新统计
                    stats.BytesReceived += data.Length;
                    stats.PacketsReceived++;
                    stats.LastActivityTime = DateTime.UtcNow;

                    // 解码数据包
                    var packet = decoder.Decode(data);

                    // 处理数据包
                    packet = await ProcessIncomingPacket(packet);

                    // 分发消息
                    MessageDispatcher.Instance.Dispatch(packet);
                }
                catch (Exception ex)
                {
                    Logger.Error($"Receive loop error: {ex.Message}");
                    if (isConnected)
                    {
                        OnError?.Invoke($"Receive error: {ex.Message}");
                        break;
                    }
                }
            }

            // 连接断开
            if (isConnected)
            {
                _ = DisconnectAsync();
            }
        }

        /// <summary>
        /// 发送循环
        /// </summary>
        private async Task SendLoop()
        {
            while (isConnected)
            {
                try
                {
                    Packet packet = null;

                    // 获取待发送的数据包
                    lock (sendQueueLock)
                    {
                        if (sendQueue.Count > 0)
                        {
                            packet = sendQueue.Dequeue();
                        }
                    }

                    if (packet != null)
                    {
                        // 编码数据包
                        var data = encoder.Encode(packet);

                        // 发送数据
                        await transport.SendAsync(data);

                        // 更新统计
                        stats.BytesSent += data.Length;
                        stats.PacketsSent++;
                        stats.LastActivityTime = DateTime.UtcNow;
                    }
                    else
                    {
                        // 没有待发送的数据，等待
                        await Task.Delay(1);
                    }
                }
                catch (Exception ex)
                {
                    Logger.Error($"Send loop error: {ex.Message}");
                    if (isConnected)
                    {
                        OnError?.Invoke($"Send error: {ex.Message}");
                        break;
                    }
                }
            }
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            if (!disposed)
            {
                _ = DisconnectAsync();
                transport?.Dispose();
                disposed = true;
            }
        }

        #endregion
    }
}