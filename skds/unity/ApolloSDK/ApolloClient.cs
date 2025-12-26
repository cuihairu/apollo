using System;
using System.Threading;
using System.Threading.Tasks;
using ApolloSDK.Network;
using ApolloSDK.Session;
using ApolloSDK.Messaging;
using ApolloSDK.Utilities;

namespace ApolloSDK
{
    /// <summary>
    /// Apollo SDK 主客户端类
    /// </summary>
    public class ApolloClient : IDisposable
    {
        private ApolloClientConfig config;
        private NetworkManager networkManager;
        private AuthManager authManager;
        private MessageRouter messageRouter;
        private HeartbeatManager heartbeatManager;
        private bool disposed = false;

        // 事件
        public event Action OnConnected;
        public event Action OnDisconnected;
        public event Action<string> OnError;
        public event Action OnHeartbeatTimeout;

        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="config">客户端配置</param>
        public ApolloClient(ApolloClientConfig config)
        {
            this.config = config ?? throw new ArgumentNullException(nameof(config));

            Initialize();
        }

        /// <summary>
        /// 是否已连接
        /// </summary>
        public bool IsConnected { get; private set; }

        /// <summary>
        /// 是否已认证
        /// </summary>
        public bool IsAuthenticated { get; private set; }

        /// <summary>
        /// 初始化SDK
        /// </summary>
        private void Initialize()
        {
            // 初始化日志
            Logger.Initialize(config.LogLevel);

            // 创建网络管理器
            networkManager = new NetworkManager(config);
            networkManager.OnConnected += HandleNetworkConnected;
            networkManager.OnDisconnected += HandleNetworkDisconnected;
            networkManager.OnError += HandleNetworkError;

            // 创建消息路由
            messageRouter = new MessageRouter();
            messageRouter.Initialize();

            // 创建认证管理器
            authManager = new AuthManager(networkManager, messageRouter);
            authManager.OnAuthenticated += HandleAuthenticated;

            // 创建心跳管理器
            heartbeatManager = new HeartbeatManager(networkManager, config.HeartbeatInterval);
            heartbeatManager.OnTimeout += HandleHeartbeatTimeout;
        }

        /// <summary>
        /// 异步连接服务器
        /// </summary>
        /// <returns></returns>
        public async Task<bool> ConnectAsync(CancellationToken cancellationToken = default)
        {
            try
            {
                Logger.Info($"Connecting to server {config.ServerAddress}:{config.Port}");

                var connected = await networkManager.ConnectAsync(cancellationToken);
                if (connected)
                {
                    Logger.Info("Connected to server successfully");
                    IsConnected = true;
                    return true;
                }
                else
                {
                    Logger.Error("Failed to connect to server");
                    return false;
                }
            }
            catch (Exception ex)
            {
                Logger.Error($"Connection error: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 异步断开连接
        /// </summary>
        /// <returns></returns>
        public async Task DisconnectAsync()
        {
            if (IsConnected)
            {
                Logger.Info("Disconnecting from server");

                await authManager.LogoutAsync();
                heartbeatManager.Stop();

                await networkManager.DisconnectAsync();

                IsConnected = false;
                IsAuthenticated = false;
            }
        }

        /// <summary>
        /// 异步登录
        /// </summary>
        /// <param name="request">登录请求</param>
        /// <param name="cancellationToken">取消令牌</param>
        /// <returns></returns>
        public async Task<LoginResponse> LoginAsync(LoginRequest request, CancellationToken cancellationToken = default)
        {
            if (!IsConnected)
            {
                throw new InvalidOperationException("Not connected to server");
            }

            Logger.Info($"Logging in user: {request.Username}");

            var response = await authManager.LoginAsync(request, cancellationToken);

            if (response.Success)
            {
                IsAuthenticated = true;
                heartbeatManager.Start();
                Logger.Info("Login successful");
            }
            else
            {
                Logger.Error($"Login failed: {response.ErrorMessage}");
            }

            return response;
        }

        /// <summary>
        /// 发送消息
        /// </summary>
        /// <typeparam name="T">消息类型</typeparam>
        /// <param name="messageId">消息ID</param>
        /// <param name="message">消息内容</param>
        /// <returns></returns>
        public async Task<bool> SendAsync<T>(ushort messageId, T message) where T : class, IMessage
        {
            if (!IsAuthenticated)
            {
                Logger.Warning("Cannot send message: not authenticated");
                return false;
            }

            try
            {
                return await networkManager.SendAsync(messageId, message);
            }
            catch (Exception ex)
            {
                Logger.Error($"Failed to send message {messageId}: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 注册消息处理器
        /// </summary>
        /// <typeparam name="T">消息类型</typeparam>
        /// <param name="messageId">消息ID</param>
        /// <param name="handler">处理函数</param>
        public void RegisterHandler<T>(ushort messageId, Action<T> handler) where T : class, IMessage
        {
            messageRouter.RegisterHandler(messageId, handler);
        }

        /// <summary>
        /// 取消注册消息处理器
        /// </summary>
        /// <param name="messageId">消息ID</param>
        public void UnregisterHandler(ushort messageId)
        {
            messageRouter.UnregisterHandler(messageId);
        }

        /// <summary>
        /// 获取网络统计信息
        /// </summary>
        /// <returns></returns>
        public NetworkStats GetNetworkStats()
        {
            return networkManager.GetStats();
        }

        #region Event Handlers

        private void HandleNetworkConnected()
        {
            IsConnected = true;
            OnConnected?.Invoke();
        }

        private void HandleNetworkDisconnected()
        {
            IsConnected = false;
            IsAuthenticated = false;
            heartbeatManager.Stop();
            OnDisconnected?.Invoke();
        }

        private void HandleNetworkError(string error)
        {
            Logger.Error($"Network error: {error}");
            OnError?.Invoke(error);
        }

        private void HandleAuthenticated()
        {
            IsAuthenticated = true;
            heartbeatManager.Start();
        }

        private void HandleHeartbeatTimeout()
        {
            Logger.Error("Heartbeat timeout");
            OnHeartbeatTimeout?.Invoke();
            _ = DisconnectAsync();
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            if (!disposed)
            {
                _ = DisconnectAsync();

                networkManager?.Dispose();
                messageRouter?.Dispose();
                authManager?.Dispose();
                heartbeatManager?.Dispose();

                disposed = true;
            }
        }

        #endregion
    }
}