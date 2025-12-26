/**
 * Apollo 客户端配置
 */

/**
 * 日志级别
 */
export enum LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    NONE = 4
}

/**
 * Apollo 客户端配置类
 */
export class ApolloClientConfig {
    /**
     * 服务器地址
     */
    public serverAddress: string = 'localhost';

    /**
     * 服务器端口
     */
    public port: number = 8080;

    /**
     * 是否使用 WebSocket Secure (wss://)
     */
    public useSSL: boolean = false;

    /**
     * 连接超时时间（毫秒）
     */
    public connectTimeout: number = 10000;

    /**
     * 心跳间隔（毫秒）
     */
    public heartbeatInterval: number = 30000;

    /**
     * 心跳超时次数阈值
     */
    public heartbeatTimeoutThreshold: number = 3;

    /**
     * 重连尝试次数
     */
    public reconnectAttempts: number = 3;

    /**
     * 重连间隔（毫秒）
     */
    public reconnectInterval: number = 5000;

    /**
     * 日志级别
     */
    public logLevel: LogLevel = LogLevel.INFO;

    /**
     * 是否启用自动重连
     */
    public enableAutoReconnect: boolean = true;

    /**
     * 消息压缩
     */
    public enableCompression: boolean = false;

    /**
     * 构造函数
     */
    constructor(options?: Partial<ApolloClientConfig>) {
        if (options) {
            for (const key in options) {
                if (options.hasOwnProperty(key)) {
                    this[key] = options[key];
                }
            }
        }
    }

    /**
     * 获取 WebSocket URL
     */
    public getWebSocketURL(): string {
        const protocol = this.useSSL ? 'wss://' : 'ws://';
        return `${protocol}${this.serverAddress}:${this.port}`;
    }

    /**
     * 创建默认配置
     */
    public static createDefault(): ApolloClientConfig {
        return new ApolloClientConfig();
    }

    /**
     * 创建生产环境配置
     */
    public static createProduction(serverAddress: string, port: number): ApolloClientConfig {
        return new ApolloClientConfig({
            serverAddress: serverAddress,
            port: port,
            useSSL: true,
            logLevel: LogLevel.WARNING,
            enableAutoReconnect: true
        });
    }

    /**
     * 创建开发环境配置
     */
    public static createDevelopment(serverAddress: string, port: number): ApolloClientConfig {
        return new ApolloClientConfig({
            serverAddress: serverAddress,
            port: port,
            useSSL: false,
            logLevel: LogLevel.DEBUG,
            enableAutoReconnect: true
        });
    }
}
