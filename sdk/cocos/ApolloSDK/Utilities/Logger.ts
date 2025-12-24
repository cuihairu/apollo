/**
 * 日志工具类
 */

import { LogLevel } from '../ApolloClientConfig';

/**
 * 日志工具类
 */
export class Logger {
    private static logLevel: LogLevel = LogLevel.INFO;

    /**
     * 初始化日志系统
     */
    public static initialize(level: LogLevel): void {
        this.logLevel = level;
    }

    /**
     * 设置日志级别
     */
    public static setLevel(level: LogLevel): void {
        this.logLevel = level;
    }

    /**
     * 获取日志级别
     */
    public static getLevel(): LogLevel {
        return this.logLevel;
    }

    /**
     * 输出调试日志
     */
    public static debug(...args: any[]): void {
        if (this.logLevel <= LogLevel.DEBUG) {
            console.log(`[DEBUG] [${this.getTimestamp()}]`, ...args);
        }
    }

    /**
     * 输出信息日志
     */
    public static info(...args: any[]): void {
        if (this.logLevel <= LogLevel.INFO) {
            console.log(`[INFO] [${this.getTimestamp()}]`, ...args);
        }
    }

    /**
     * 输出警告日志
     */
    public static warning(...args: any[]): void {
        if (this.logLevel <= LogLevel.WARNING) {
            console.warn(`[WARN] [${this.getTimestamp()}]`, ...args);
        }
    }

    /**
     * 输出错误日志
     */
    public static error(...args: any[]): void {
        if (this.logLevel <= LogLevel.ERROR) {
            console.error(`[ERROR] [${this.getTimestamp()}]`, ...args);
        }
    }

    /**
     * 获取当前时间戳
     */
    private static getTimestamp(): string {
        const now = new Date();
        const hours = String(now.getHours()).padStart(2, '0');
        const minutes = String(now.getMinutes()).padStart(2, '0');
        const seconds = String(now.getSeconds()).padStart(2, '0');
        const milliseconds = String(now.getMilliseconds()).padStart(3, '0');
        return `${hours}:${minutes}:${seconds}.${milliseconds}`;
    }
}
