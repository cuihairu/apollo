/**
 * Apollo SDK for Cocos Creator
 * @version 1.0.0
 * @description Cocos Creator 游戏服务器的网络通信 SDK
 */

import { WebSocket } from 'cc';
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
    private onConnectedCallback?: () => void;
    private onDisconnectedCallback?: () => void;
    private onErrorCallback?: (error: string) => void;
    private onHeartbeatTimeoutCallback?: () => void;

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

        Logger.info('Initializing Apollo SDK...');

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

        Logger.info('Apollo SDK initialized successfully');
    }

    /**
     * 连接到服务器
     * @returns Promise<boolean> 连接是否成功
     */
    public async connect(): Promise<boolean> {
        try {
            Logger.info(`Connecting to server ${this.config.serverAddress}:${this.config.port}`);

            const connected = await this.networkManager.connect();

            if (connected) {
                Logger.info('Connected to server successfully');
                return true;
            } else {
                Logger.error('Failed to connect to server');
                return false;
            }
        } catch (error) {
            Logger.error(`Connection error: ${error}`);
            return false;
        }
    }

    /**
     * 断开连接
     */
    public async disconnect(): Promise<void> {
        if (this._isConnected) {
            Logger.info('Disconnecting from server');

            await this.authManager.logout();
            this.heartbeatManager.stop();
            await this.networkManager.disconnect();

            this._isConnected = false;
            this._isAuthenticated = false;
        }
    }

    /**
     * 登录
     * @param request 登录请求
     * @returns Promise<LoginResponse> 登录响应
     */
    public async login(request: LoginRequest): Promise<LoginResponse> {
        if (!this._isConnected) {
            throw new Error('Not connected to server');
        }

        Logger.info(`Logging in user: ${request.username}`);

        const response = await this.authManager.login(request);

        if (response.success) {
            this._isAuthenticated = true;
            this.heartbeatManager.start();
            Logger.info('Login successful');
        } else {
            Logger.error(`Login failed: ${response.errorMessage}`);
        }

        return response;
    }

    /**
     * 发送消息
     * @param messageId 消息ID
     * @param message 消息内容
     * @returns Promise<boolean> 发送是否成功
     */
    public async send<T extends IMessage>(messageId: number, message: T): Promise<boolean> {
        if (!this._isAuthenticated) {
            Logger.warning('Cannot send message: not authenticated');
            return false;
        }

        try {
            return await this.networkManager.send(messageId, message);
        } catch (error) {
            Logger.error(`Failed to send message ${messageId}: ${error}`);
            return false;
        }
    }

    /**
     * 注册消息处理器
     * @param messageId 消息ID
     * @param handler 处理函数
     */
    public registerHandler<T extends IMessage>(
        messageId: number,
        handler: (message: T) => void
    ): void {
        this.messageRouter.registerHandler(messageId, handler);
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
     * 设置事件监听器
     */
    public on(event: 'connected', callback: () => void): void;
    public on(event: 'disconnected', callback: () => void): void;
    public on(event: 'error', callback: (error: string) => void): void;
    public on(event: 'heartbeatTimeout', callback: () => void): void;
    public on(event: string, callback: any): void {
        switch (event) {
            case 'connected':
                this.onConnectedCallback = callback;
                break;
            case 'disconnected':
                this.onDisconnectedCallback = callback;
                break;
            case 'error':
                this.onErrorCallback = callback;
                break;
            case 'heartbeatTimeout':
                this.onHeartbeatTimeoutCallback = callback;
                break;
        }
    }

    /**
     * 移除事件监听器
     */
    public off(event: string): void {
        switch (event) {
            case 'connected':
                this.onConnectedCallback = undefined;
                break;
            case 'disconnected':
                this.onDisconnectedCallback = undefined;
                break;
            case 'error':
                this.onErrorCallback = undefined;
                break;
            case 'heartbeatTimeout':
                this.onHeartbeatTimeoutCallback = undefined;
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
        this.onConnectedCallback?.();
    }

    private handleNetworkDisconnected(): void {
        this._isConnected = false;
        this._isAuthenticated = false;
        this.heartbeatManager.stop();
        this.onDisconnectedCallback?.();
    }

    private handleNetworkError(error: string): void {
        Logger.error(`Network error: ${error}`);
        this.onErrorCallback?.(error);
    }

    private handleAuthenticated(): void {
        this._isAuthenticated = true;
        this.heartbeatManager.start();
    }

    private handleHeartbeatTimeout(): void {
        Logger.error('Heartbeat timeout');
        this.onHeartbeatTimeoutCallback?.();
        this.disconnect();
    }
}
