/**
 * 消息路由器
 * 负责消息的分发和处理器管理
 */

import { IMessage } from './IMessage';
import { Logger } from '../Utilities/Logger';

/**
 * 消息处理器类型
 */
type MessageHandler<T extends IMessage> = (message: T) => void;

/**
 * 消息路由器类
 */
export class MessageRouter {
    private handlers: Map<number, MessageHandler<any>> = new Map();
    private initialized: boolean = false;

    /**
     * 初始化消息路由器
     */
    public initialize(): void {
        if (this.initialized) {
            return;
        }

        this.initialized = true;
        Logger.info('MessageRouter initialized');
    }

    /**
     * 注册消息处理器
     * @param messageId 消息ID
     * @param handler 处理函数
     */
    public registerHandler<T extends IMessage>(messageId: number, handler: MessageHandler<T>): void {
        if (this.handlers.has(messageId)) {
            Logger.warning(`Handler for message ${messageId} already exists, overwriting`);
        }

        this.handlers.set(messageId, handler);
        Logger.debug(`Registered handler for message ${messageId}`);
    }

    /**
     * 取消注册消息处理器
     * @param messageId 消息ID
     */
    public unregisterHandler(messageId: number): void {
        if (this.handlers.delete(messageId)) {
            Logger.debug(`Unregistered handler for message ${messageId}`);
        } else {
            Logger.warning(`No handler found for message ${messageId}`);
        }
    }

    /**
     * 路由消息到对应的处理器
     * @param messageId 消息ID
     * @param data 消息数据
     */
    public route<T extends IMessage>(messageId: number, data: ArrayBuffer, messageFactory: () => T): void {
        const handler = this.handlers.get(messageId);

        if (!handler) {
            Logger.warning(`No handler registered for message ${messageId}`);
            return;
        }

        try {
            // 创建消息对象并反序列化
            const message = messageFactory();
            message.decode(data);

            // 调用处理器
            handler(message);

        } catch (error) {
            Logger.error(`Error routing message ${messageId}:`, error);
        }
    }

    /**
     * 检查是否有消息处理器
     * @param messageId 消息ID
     */
    public hasHandler(messageId: number): boolean {
        return this.handlers.has(messageId);
    }

    /**
     * 获取已注册的处理器数量
     */
    public getHandlerCount(): number {
        return this.handlers.size;
    }

    /**
     * 清除所有处理器
     */
    public clear(): void {
        this.handlers.clear();
        Logger.info('Cleared all message handlers');
    }

    /**
     * 销毁
     */
    public dispose(): void {
        this.clear();
        this.initialized = false;
    }
}
