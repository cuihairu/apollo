using System;
using System.Threading;
using System.Threading.Tasks;
using ApolloSDK.Network;
using ApolloSDK.Messaging;

namespace ApolloSDK.Session
{
    /// <summary>
    /// 登录请求
    /// </summary>
    [Serializable]
    public class LoginRequest
    {
        public string Username { get; set; }
        public string Password { get; set; }
        public string DeviceId { get; set; }
        public string ClientVersion { get; set; }
        public string AuthToken { get; set; }
    }

    /// <summary>
    /// 登录响应
    /// </summary>
    [Serializable]
    public class LoginResponse
    {
        public bool Success { get; set; }
        public string UserId { get; set; }
        public string SessionToken { get; set; }
        public string RefreshToken { get; set; }
        public long ExpiresIn { get; set; }
        public string ErrorMessage { get; set; }
    }

    /// <summary>
    /// 认证管理器
    /// </summary>
    public class AuthManager
    {
        private NetworkManager networkManager;
        private MessageRouter messageRouter;
        private SessionStore sessionStore;

        // 认证状态
        private volatile bool isAuthenticated = false;
        private string userId = "";
        private string sessionToken = "";
        private string refreshToken = "";
        private DateTime tokenExpiry = DateTime.MinValue;

        // 事件
        public event Action OnAuthenticated;
        public event Action<string> OnAuthenticationFailed;

        /// <summary>
        /// 是否已认证
        /// </summary>
        public bool IsAuthenticated => isAuthenticated;

        /// <summary>
        /// 用户ID
        /// </summary>
        public string UserId => userId;

        /// <summary>
        /// 会话令牌
        /// </summary>
        public string SessionToken => sessionToken;

        /// <summary>
        /// 构造函数
        /// </summary>
        /// <param name="networkManager">网络管理器</param>
        /// <param name="messageRouter">消息路由器</param>
        public AuthManager(NetworkManager networkManager, MessageRouter messageRouter)
        {
            this.networkManager = networkManager ?? throw new ArgumentNullException(nameof(networkManager));
            this.messageRouter = messageRouter ?? throw new ArgumentNullException(nameof(messageRouter));

            sessionStore = new SessionStore();

            // 注册消息处理器
            RegisterMessageHandlers();
        }

        /// <summary>
        /// 异步登录
        /// </summary>
        /// <param name="request">登录请求</param>
        /// <param name="cancellationToken">取消令牌</param>
        /// <returns></returns>
        public async Task<LoginResponse> LoginAsync(LoginRequest request, CancellationToken cancellationToken = default)
        {
            try
            {
                // 创建认证任务
                var tcs = new TaskCompletionSource<LoginResponse>();

                // 临时注册登录响应处理器
                void HandleLoginResponse(LoginResponse response)
                {
                    tcs.SetResult(response);
                    messageRouter.UnregisterHandler<LoginResponse>(MessageIds.LOGIN_RESPONSE);
                }

                messageRouter.RegisterHandler<LoginResponse>(MessageIds.LOGIN_RESPONSE, HandleLoginResponse);

                // 发送登录请求
                await networkManager.SendAsync(MessageIds.LOGIN_REQUEST, request);

                // 等待响应
                var response = await tcs.Task;

                if (response.Success)
                {
                    // 保存认证信息
                    userId = response.UserId;
                    sessionToken = response.SessionToken;
                    refreshToken = response.RefreshToken;
                    tokenExpiry = DateTime.UtcNow.AddSeconds(response.ExpiresIn);

                    // 保存到本地存储
                    sessionStore.SaveSession(new SessionData
                    {
                        UserId = userId,
                        SessionToken = sessionToken,
                        RefreshToken = refreshToken,
                        ExpiresAt = tokenExpiry
                    });

                    isAuthenticated = true;
                    OnAuthenticated?.Invoke();
                }
                else
                {
                    OnAuthenticationFailed?.Invoke(response.ErrorMessage);
                }

                return response;
            }
            catch (Exception ex)
            {
                Logger.Error($"Login error: {ex.Message}");
                return new LoginResponse
                {
                    Success = false,
                    ErrorMessage = ex.Message
                };
            }
        }

        /// <summary>
        /// 异步登出
        /// </summary>
        public async Task LogoutAsync()
        {
            if (isAuthenticated)
            {
                try
                {
                    // 发送登出请求
                    var logoutRequest = new { UserId = userId };
                    await networkManager.SendAsync(MessageIds.LOGOUT_REQUEST, logoutRequest);
                }
                catch (Exception ex)
                {
                    Logger.Error($"Logout error: {ex.Message}");
                }
                finally
                {
                    // 清理认证状态
                    ClearAuthentication();
                }
            }
        }

        /// <summary>
        /// 异步刷新令牌
        /// </summary>
        public async Task<bool> RefreshTokenAsync()
        {
            if (string.IsNullOrEmpty(refreshToken))
            {
                return false;
            }

            try
            {
                // 创建刷新任务
                var tcs = new TaskCompletionSource<bool>();

                // 发送刷新请求
                var refreshRequest = new { RefreshToken = refreshToken };
                await networkManager.SendAsync(MessageIds.REFRESH_TOKEN_REQUEST, refreshRequest);

                // 等待响应（简化实现）
                // TODO: 实现完整的刷新令牌流程

                return true;
            }
            catch (Exception ex)
            {
                Logger.Error($"Token refresh error: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 尝试恢复会话
        /// </summary>
        public async Task<bool> RestoreSessionAsync()
        {
            try
            {
                var session = sessionStore.LoadSession();
                if (session != null && session.ExpiresAt > DateTime.UtcNow)
                {
                    // 会话有效，直接使用
                    userId = session.UserId;
                    sessionToken = session.SessionToken;
                    refreshToken = session.RefreshToken;
                    tokenExpiry = session.ExpiresAt;

                    // 验证会话是否仍然有效
                    if (await ValidateSessionAsync())
                    {
                        isAuthenticated = true;
                        OnAuthenticated?.Invoke();
                        return true;
                    }
                    else
                    {
                        // 会话无效，尝试刷新令牌
                        if (await RefreshTokenAsync())
                        {
                            isAuthenticated = true;
                            OnAuthenticated?.Invoke();
                            return true;
                        }
                    }
                }

                return false;
            }
            catch (Exception ex)
            {
                Logger.Error($"Session restore error: {ex.Message}");
                return false;
            }
        }

        /// <summary>
        /// 检查令牌是否即将过期
        /// </summary>
        public bool IsTokenExpiringSoon(int secondsThreshold = 300)
        {
            return tokenExpiry - DateTime.UtcNow < TimeSpan.FromSeconds(secondsThreshold);
        }

        /// <summary>
        /// 获取认证头
        /// </summary>
        public string GetAuthHeader()
        {
            if (!isAuthenticated)
            {
                return null;
            }

            return $"Bearer {sessionToken}";
        }

        #region Private Methods

        /// <summary>
        /// 注册消息处理器
        /// </summary>
        private void RegisterMessageHandlers()
        {
            // 注册令牌刷新响应
            messageRouter.RegisterHandler<TokenRefreshResponse>(MessageIds.REFRESH_TOKEN_RESPONSE, HandleTokenRefreshResponse);

            // 注册强制登出消息
            messageRouter.RegisterHandler<ForceLogoutMessage>(MessageIds.FORCE_LOGOUT, HandleForceLogout);
        }

        /// <summary>
        /// 处理令牌刷新响应
        /// </summary>
        private void HandleTokenRefreshResponse(TokenRefreshResponse response)
        {
            if (response.Success)
            {
                sessionToken = response.NewSessionToken;
                refreshToken = response.NewRefreshToken;
                tokenExpiry = DateTime.UtcNow.AddSeconds(response.ExpiresIn);

                // 更新本地存储
                sessionStore.SaveSession(new SessionData
                {
                    UserId = userId,
                    SessionToken = sessionToken,
                    RefreshToken = refreshToken,
                    ExpiresAt = tokenExpiry
                });

                Logger.Info("Token refreshed successfully");
            }
            else
            {
                Logger.Error($"Token refresh failed: {response.ErrorMessage}");
                ClearAuthentication();
            }
        }

        /// <summary>
        /// 处理强制登出
        /// </summary>
        private void HandleForceLogout(ForceLogoutMessage message)
        {
            Logger.Warning($"Force logout: {message.Reason}");
            ClearAuthentication();
        }

        /// <summary>
        /// 验证会话
        /// </summary>
        private async Task<bool> ValidateSessionAsync()
        {
            try
            {
                // 发送会话验证请求
                var validateRequest = new { UserId = userId, SessionToken = sessionToken };
                await networkManager.SendAsync(MessageIds.VALIDATE_SESSION_REQUEST, validateRequest);

                // 等待验证响应（简化实现）
                // TODO: 实现完整的会话验证流程

                return true;
            }
            catch
            {
                return false;
            }
        }

        /// <summary>
        /// 清理认证状态
        /// </summary>
        private void ClearAuthentication()
        {
            isAuthenticated = false;
            userId = "";
            sessionToken = "";
            refreshToken = "";
            tokenExpiry = DateTime.MinValue;

            // 清理本地存储
            sessionStore.ClearSession();
        }

        #endregion

        #region IDisposable

        public void Dispose()
        {
            _ = LogoutAsync();
        }

        #endregion
    }
}