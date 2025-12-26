/**
 * 网络模块类型定义
 */

/**
 * 连接状态
 */
export enum ConnectionState {
    DISCONNECTED = 'disconnected',
    CONNECTING = 'connecting',
    CONNECTED = 'connected',
    RECONNECTING = 'reconnecting'
}

/**
 * 网络统计信息
 */
export interface NetworkStats {
    /** 是否已连接 */
    isConnected: boolean;
    /** 连接时间戳 */
    connectTime: number;
    /** 发送字节数 */
    sentBytes: number;
    /** 接收字节数 */
    receivedBytes: number;
    /** 发送消息数 */
    sentMessages: number;
    /** 接收消息数 */
    receivedMessages: number;
    /** 当前延迟（毫秒） */
    latency: number;
}

/**
 * 网络错误类型
 */
export enum NetworkError {
    /** 未知错误 */
    UNKNOWN = 'unknown',
    /** 连接失败 */
    CONNECTION_FAILED = 'connection_failed',
    /** 连接超时 */
    CONNECTION_TIMEOUT = 'connection_timeout',
    /** 连接关闭 */
    CONNECTION_CLOSED = 'connection_closed',
    /** 连接错误 */
    CONNECTION_ERROR = 'connection_error',
    /** 发送失败 */
    SEND_FAILED = 'send_failed',
    /** 接收错误 */
    RECEIVE_ERROR = 'receive_error',
    /** 消息解析错误 */
    MESSAGE_PARSE_ERROR = 'message_parse_error'
}

/**
 * 网络错误信息
 */
export class NetworkErrorInfo extends Error {
    public code: NetworkError;
    public timestamp: number;

    constructor(code: NetworkError, message: string) {
        super(message);
        this.name = 'NetworkErrorInfo';
        this.code = code;
        this.timestamp = Date.now();
    }
}

/**
 * WebSocket 消息事件
 */
export interface WebSocketMessageEvent {
    /** 消息数据 */
    data: ArrayBuffer;
    /** 消息类型 */
    type: string;
}

/**
 * 消息头
 */
export interface MessageHeader {
    /** 消息长度（不含头） */
    length: number;
    /** 消息ID */
    msgId: number;
    /** 序列号 */
    seq: number;
    /** 标志位 */
    flags: number;
}

/**
 * 消息标志位
 */
export enum MessageFlags {
    /** 无标志 */
    NONE = 0,
    /** 需要确认 */
    ACK_REQUIRED = 1 << 0,
    /** 压缩 */
    COMPRESSED = 1 << 1,
    /** 加密 */
    ENCRYPTED = 1 << 2
}
