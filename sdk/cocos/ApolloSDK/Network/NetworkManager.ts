/**
 * 网络管理器
 * 负责 WebSocket 连接管理和消息收发
 */

import { ApolloClientConfig } from '../ApolloClientConfig';
import { ConnectionState, NetworkStats, NetworkError, NetworkErrorInfo } from './NetworkTypes';
import { MessageCodec } from '../Messaging/MessageCodec';
import { IMessage } from '../Messaging/IMessage';
import { Logger } from '../Utilities/Logger';
import { ByteBuffer } from '../Utilities/ByteBuffer';

/**
 * 网络管理器类
 */
export class NetworkManager {
    private config: ApolloClientConfig;
    private websocket: WebSocket | null = null;
    private _state: ConnectionState = ConnectionState.DISCONNECTED;
    private messageCodec: MessageCodec;

    // 统计信息
    private stats: NetworkStats = {
        isConnected: false,
        connectTime: 0,
        sentBytes: 0,
        receivedBytes: 0,
        sentMessages: 0,
        receivedMessages: 0,
        latency: 0
    };

    // 消息队列
    private messageQueue: Array<{ messageId: number; data: ArrayBuffer }> = [];
    private isProcessingQueue: boolean = false;

    // 重连相关
    private reconnectTimer: number | null = null;
    private reconnectAttempts: number = 0;
    private manualDisconnect: boolean = false;

    // 心跳相关
    private lastHeartbeatTime: number = 0;
    private heartbeatTimer: number | null = null;

    // 事件回调
    public onConnected?: () => void;
    public onDisconnected?: () => void;
    public onError?: (error: string) => void;
    public onMessage?: (messageId: number, data: ArrayBuffer) => void;

    constructor(config: ApolloClientConfig) {
        this.config = config;
        this.messageCodec = new MessageCodec();
    }

    /**
     * 获取当前连接状态
     */
    public get state(): ConnectionState {
        return this._state;
    }

    /**
     * 连接到服务器
     */
    public async connect(): Promise<boolean> {
        return new Promise<boolean>((resolve, reject) => {
            if (this._state === ConnectionState.CONNECTED || this._state === ConnectionState.CONNECTING) {
                resolve(true);
                return;
            }

            this._state = ConnectionState.CONNECTING;
            this.manualDisconnect = false;

            const url = this.config.getWebSocketURL();
            Logger.info(`Connecting to ${url}`);

            try {
                this.websocket = new WebSocket(url);
                this.websocket.binaryType = 'arraybuffer';

                // 连接超时处理
                const timeoutId = setTimeout(() => {
                    if (this._state === ConnectionState.CONNECTING) {
                        this.cleanup();
                        this._state = ConnectionState.DISCONNECTED;
                        const error = new NetworkErrorInfo(NetworkError.CONNECTION_TIMEOUT, 'Connection timeout');
                        this.handleError(error);
                        resolve(false);
                    }
                }, this.config.connectTimeout);

                this.websocket.onopen = () => {
                    clearTimeout(timeoutId);
                    this._state = ConnectionState.CONNECTED;
                    this.stats.isConnected = true;
                    this.stats.connectTime = Date.now();
                    this.reconnectAttempts = 0;

                    Logger.info('WebSocket connected');

                    // 处理消息队列
                    this.processMessageQueue();

                    this.onConnected?.();
                    resolve(true);
                };

                this.websocket.onclose = (event) => {
                    clearTimeout(timeoutId);
                    Logger.info(`WebSocket closed: code=${event.code}, reason=${event.reason}`);

                    this._state = ConnectionState.DISCONNECTED;
                    this.stats.isConnected = false;
                    this.cleanup();

                    // 自动重连
                    if (!this.manualDisconnect && this.config.enableAutoReconnect) {
                        this.scheduleReconnect();
                    }

                    this.onDisconnected?.();
                };

                this.websocket.onerror = (error) => {
                    clearTimeout(timeoutId);
                    Logger.error('WebSocket error:', error);

                    const networkError = new NetworkErrorInfo(
                        NetworkError.CONNECTION_ERROR,
                        'WebSocket connection error'
                    );
                    this.handleError(networkError);
                    resolve(false);
                };

                this.websocket.onmessage = (event) => {
                    this.handleMessage(event.data);
                };

            } catch (error) {
                this._state = ConnectionState.DISCONNECTED;
                const networkError = new NetworkErrorInfo(
                    NetworkError.CONNECTION_FAILED,
                    `Failed to create WebSocket: ${error}`
                );
                this.handleError(networkError);
                resolve(false);
            }
        });
    }

    /**
     * 断开连接
     */
    public async disconnect(): Promise<void> {
        this.manualDisconnect = true;

        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }

        if (this.websocket) {
            this.websocket.close(1000, 'Client disconnect');
        }

        this.cleanup();
    }

    /**
     * 发送消息
     */
    public async send<T extends IMessage>(messageId: number, message: T): Promise<boolean> {
        try {
            const data = this.messageCodec.encode(messageId, message);

            if (this._state === ConnectionState.CONNECTED && this.websocket) {
                this.websocket.send(data);
                this.updateSendStats(data.byteLength);
                return true;
            } else {
                // 加入消息队列
                this.messageQueue.push({ messageId, data });
                Logger.debug(`Message ${messageId} queued (${this.messageQueue.length} in queue)`);
                return true;
            }
        } catch (error) {
            Logger.error(`Failed to send message ${messageId}:`, error);
            return false;
        }
    }

    /**
     * 获取统计信息
     */
    public getStats(): NetworkStats {
        return { ...this.stats };
    }

    /**
     * 处理接收到的消息
     */
    private handleMessage(data: ArrayBuffer): void {
        try {
            this.stats.receivedBytes += data.byteLength;
            this.stats.receivedMessages++;

            // 解析消息头
            const header = this.messageCodec.decodeHeader(data);
            const messageData = data.slice(16); // 跳过消息头

            Logger.debug(`Received message: ID=${header.msgId}, length=${header.length}`);

            this.onMessage?.(header.msgId, messageData);

        } catch (error) {
            Logger.error('Failed to handle message:', error);
            const networkError = new NetworkErrorInfo(
                NetworkError.MESSAGE_PARSE_ERROR,
                `Message parse error: ${error}`
            );
            this.handleError(networkError);
        }
    }

    /**
     * 处理消息队列
     */
    private processMessageQueue(): void {
        if (this.isProcessingQueue || this.messageQueue.length === 0) {
            return;
        }

        this.isProcessingQueue = true;

        while (this.messageQueue.length > 0 && this._state === ConnectionState.CONNECTED) {
            const item = this.messageQueue.shift();
            if (item && this.websocket) {
                try {
                    this.websocket.send(item.data);
                    this.updateSendStats(item.data.byteLength);
                    Logger.debug(`Sent queued message: ID=${item.messageId}`);
                } catch (error) {
                    Logger.error('Failed to send queued message:', error);
                    // 重新加入队列
                    this.messageQueue.unshift(item);
                    break;
                }
            }
        }

        this.isProcessingQueue = false;
    }

    /**
     * 安排重连
     */
    private scheduleReconnect(): void {
        if (this.reconnectAttempts >= this.config.reconnectAttempts) {
            Logger.error('Max reconnect attempts reached');
            return;
        }

        const delay = this.config.reconnectInterval * Math.pow(2, this.reconnectAttempts);
        Logger.info(`Scheduling reconnect in ${delay}ms (attempt ${this.reconnectAttempts + 1})`);

        this.reconnectTimer = setTimeout(() => {
            this.reconnectAttempts++;
            this._state = ConnectionState.RECONNECTING;
            this.connect();
        }, delay) as unknown as number;
    }

    /**
     * 处理错误
     */
    private handleError(error: NetworkErrorInfo): void {
        Logger.error(`Network error: ${error.message}`);
        this.onError?.(error.message);
    }

    /**
     * 更新发送统计
     */
    private updateSendStats(bytes: number): void {
        this.stats.sentBytes += bytes;
        this.stats.sentMessages++;
    }

    /**
     * 清理资源
     */
    private cleanup(): void {
        if (this.websocket) {
            this.websocket.onopen = null;
            this.websocket.onclose = null;
            this.websocket.onerror = null;
            this.websocket.onmessage = null;
            this.websocket = null;
        }

        this._state = ConnectionState.DISCONNECTED;
        this.stats.isConnected = false;

        if (this.heartbeatTimer) {
            clearInterval(this.heartbeatTimer);
            this.heartbeatTimer = null;
        }
    }

    /**
     * 销毁
     */
    public dispose(): void {
        this.disconnect();
        this.messageQueue.length = 0;
    }
}
