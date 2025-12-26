/**
 * Apollo SDK for LayaBox
 * @version 1.0.0
 * @description LayaBox/Air 游戏服务器的网络通信 SDK
 */

import { WebSocket } from 'laya/net/WebSocket';
import { Byte } from 'laya/utils/Byte';
import { ApolloClientConfig } from './ApolloClientConfig';
import { NetworkManager } from './Network/NetworkManager';
import { AuthManager } from './Session/AuthManager';
import { MessageRouter } from './Messaging/MessageRouter';
import { HeartbeatManager } from './Utilities/HeartbeatManager';
import { Logger } from './Utilities/Logger';
import { LoginRequest, LoginResponse } from './Session/AuthTypes';
import { IMessage } from './Messaging/IMessage';
import { NetworkStats } from './Network/NetworkTypes';

/**
 * Apollo 客户端主类
 */
export class ApolloClient {
    private config: ApolloClientConfig;
    private networkManager: NetworkManager;
    private authManager: AuthManager;
    private messageRouter: MessageRouter;
    private heartbeatManager: HeartbeatManager;
    private disposed: boolean = false;

    // 状态
    private _isConnected: boolean = false;
    private _isAuthenticated: boolean = false;

    // 事件回调
    private onConnectedCallback: Laya.Handler;
    private onDisconnectedCallback: Laya.Handler;
    private onErrorCallback: Laya.Handler;
    private onHeartbeatTimeoutCallback: Laya.Handler;

    /**
     * 构造函数
     * @param config 客户端配置
     */
    constructor(config: ApolloClientConfig) {
        if (!config) {
            throw new Error('Config cannot be null');
        }
        this.config = config;
        this.initialize();
    }

    /**
     * 是否已连接
     */
    public get isConnected(): boolean {
        return this._isConnected;
    }

    /**
     * 是否已认证
     */
    public get isAuthenticated(): boolean {
        return this._isAuthenticated;
    }

    /**
     * 初始化SDK
     */
    private initialize(): void {
        // 初始化日志
        Logger.initialize(this.config.logLevel);

        Logger.info('Initializing Apollo SDK for LayaBox...');

        // 创建网络管理器
        this.networkManager = new NetworkManager(this.config);
        this.networkManager.onConnected = this.handleNetworkConnected.bind(this);
        this.networkManager.onDisconnected = this.handleNetworkDisconnected.bind(this);
        this.networkManager.onError = this.handleNetworkError.bind(this);

        // 创建消息路由
        this.messageRouter = new MessageRouter();
        this.messageRouter.initialize();

        // 创建认证管理器
        this.authManager = new AuthManager(this.networkManager, this.messageRouter);
        this.authManager.onAuthenticated = this.handleAuthenticated.bind(this);

        // 创建心跳管理器
        this.heartbeatManager = new HeartbeatManager(
            this.networkManager,
            this.config.heartbeatInterval
        );
        this.heartbeatManager.onTimeout = this.handleHeartbeatTimeout.bind(this);

        Logger.info('Apollo SDK for LayaBox initialized successfully');
    }

    /**
     * 连接到服务器
     * @returns Promise<boolean> 连接是否成功
     */
    public connect(): Promise<boolean> {
        return new Promise<boolean>((resolve) => {
            if (this._isConnected) {
                resolve(true);
                return;
            }

            Logger.info(`Connecting to server ${this.config.serverAddress}:${this.config.port}`);

            this.networkManager.connect().then((success) => {
                if (success) {
                    Logger.info('Connected to server successfully');
                } else {
                    Logger.error('Failed to connect to server');
                }
                resolve(success);
            });
        });
    }

    /**
     * 断开连接
     */
    public disconnect(): void {
        if (this._isConnected) {
            Logger.info('Disconnecting from server');

            this.authManager.logout();
            this.heartbeatManager.stop();
            this.networkManager.disconnect();

            this._isConnected = false;
            this._isAuthenticated = false;
        }
    }

    /**
     * 登录
     * @param request 登录请求
     * @returns Promise<LoginResponse> 登录响应
     */
    public login(request: LoginRequest): Promise<LoginResponse> {
        return new Promise<LoginResponse>((resolve) => {
            if (!this._isConnected) {
                const errorResponse: LoginResponse = {
                    success: false,
                    errorMessage: 'Not connected to server'
                };
                resolve(errorResponse);
                return;
            }

            Logger.info(`Logging in user: ${request.username}`);

            this.authManager.login(request).then((response) => {
                if (response.success) {
                    this._isAuthenticated = true;
                    this.heartbeatManager.start();
                    Logger.info('Login successful');
                } else {
                    Logger.error(`Login failed: ${response.errorMessage}`);
                }
                resolve(response);
            });
        });
    }

    /**
     * 发送消息
     * @param messageId 消息ID
     * @param message 消息内容
     * @returns Promise<boolean> 发送是否成功
     */
    public send<T extends IMessage>(messageId: number, message: T): Promise<boolean> {
        return new Promise<boolean>((resolve) => {
            if (!this._isAuthenticated) {
                Logger.warning('Cannot send message: not authenticated');
                resolve(false);
                return;
            }

            this.networkManager.send(messageId, message).then((success) => {
                if (!success) {
                    Logger.error(`Failed to send message ${messageId}`);
                }
                resolve(success);
            });
        });
    }

    /**
     * 注册消息处理器
     * @param messageId 消息ID
     * @param handler 处理函数
     */
    public registerHandler<T extends IMessage>(
        messageId: number,
        caller: any,
        handler: (message: T) => void
    ): void {
        this.messageRouter.registerHandler(messageId, caller, handler);
    }

    /**
     * 取消注册消息处理器
     * @param messageId 消息ID
     */
    public unregisterHandler(messageId: number): void {
        this.messageRouter.unregisterHandler(messageId);
    }

    /**
     * 获取网络统计信息
     * @returns NetworkStats 网络统计信息
     */
    public getNetworkStats(): NetworkStats {
        return this.networkManager.getStats();
    }

    /**
     * 设置事件监听器 (Laya.Handler 方式)
     */
    public on(event: string, caller: any, listener: Function): void {
        const handler = Laya.Handler.create(caller, listener);

        switch (event) {
            case 'connected':
                this.onConnectedCallback = handler;
                break;
            case 'disconnected':
                this.onDisconnectedCallback = handler;
                break;
            case 'error':
                this.onErrorCallback = handler;
                break;
            case 'heartbeatTimeout':
                this.onHeartbeatTimeoutCallback = handler;
                break;
        }
    }

    /**
     * 移除事件监听器
     */
    public off(event: string): void {
        switch (event) {
            case 'connected':
                this.onConnectedCallback = null;
                break;
            case 'disconnected':
                this.onDisconnectedCallback = null;
                break;
            case 'error':
                this.onErrorCallback = null;
                break;
            case 'heartbeatTimeout':
                this.onHeartbeatTimeoutCallback = null;
                break;
        }
    }

    /**
     * 销毁客户端
     */
    public dispose(): void {
        if (!this.disposed) {
            this.disconnect();

            this.networkManager?.dispose();
            this.messageRouter?.dispose();
            this.authManager?.dispose();
            this.heartbeatManager?.dispose();

            this.disposed = true;
        }
    }

    // ==================== 事件处理函数 ====================

    private handleNetworkConnected(): void {
        this._isConnected = true;
        if (this.onConnectedCallback) {
            this.onConnectedCallback.run();
        }
    }

    private handleNetworkDisconnected(): void {
        this._isConnected = false;
        this._isAuthenticated = false;
        this.heartbeatManager.stop();
        if (this.onDisconnectedCallback) {
            this.onDisconnectedCallback.run();
        }
    }

    private handleNetworkError(error: string): void {
        Logger.error(`Network error: ${error}`);
        if (this.onErrorCallback) {
            this.onErrorCallback.runWith(error);
        }
    }

    private handleAuthenticated(): void {
        this._isAuthenticated = true;
        this.heartbeatManager.start();
    }

    private handleHeartbeatTimeout(): void {
        Logger.error('Heartbeat timeout');
        if (this.onHeartbeatTimeoutCallback) {
            this.onHeartbeatTimeoutCallback.run();
        }
        this.disconnect();
    }
}
