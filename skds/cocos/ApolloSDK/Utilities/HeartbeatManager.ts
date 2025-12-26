/**
 * 心跳管理器
 * 负责维持与服务器的连接，定期发送心跳包
 */

import { NetworkManager } from '../Network/NetworkManager';
import { Logger } from './Logger';

// 心跳消息ID
const MSG_HEARTBEAT = 9999;

/**
 * 心跳包
 */
interface HeartbeatMessage {
    timestamp: number;
}

/**
 * 心跳管理器类
 */
export class HeartbeatManager {
    private networkManager: NetworkManager;
    private interval: number;
    private timer: number | null = null;
    private timeoutCount: number = 0;
    private lastHeartbeatTime: number = 0;
    private lastResponseTime: number = 0;
    private maxTimeoutCount: number = 3;

    // 事件回调
    public onTimeout?: () => void;

    constructor(networkManager: NetworkManager, interval: number) {
        this.networkManager = networkManager;
        this.interval = interval;
    }

    /**
     * 启动心跳
     */
    public start(): void {
        if (this.timer !== null) {
            Logger.warning('Heartbeat already started');
            return;
        }

        Logger.info(`Starting heartbeat (interval: ${this.interval}ms)`);
        this.timeoutCount = 0;
        this.lastHeartbeatTime = Date.now();

        // 立即发送一次心跳
        this.sendHeartbeat();

        // 启动定时器
        this.timer = setInterval(() => {
            this.sendHeartbeat();
        }, this.interval) as unknown as number;
    }

    /**
     * 停止心跳
     */
    public stop(): void {
        if (this.timer !== null) {
            clearInterval(this.timer);
            this.timer = null;
            Logger.info('Heartbeat stopped');
        }
    }

    /**
     * 发送心跳包
     */
    private sendHeartbeat(): void {
        if (!this.networkManager.state || this.networkManager.state !== 'connected') {
            Logger.warning('Cannot send heartbeat: not connected');
            return;
        }

        const heartbeat: HeartbeatMessage = {
            timestamp: Date.now()
        };

        this.lastHeartbeatTime = Date.now();

        // 发送心跳
        this.networkManager.send(MSG_HEARTBEAT, heartbeat);

        // 检查是否超时
        const elapsed = this.lastHeartbeatTime - this.lastResponseTime;
        if (this.lastResponseTime > 0 && elapsed > this.interval * 2) {
            this.timeoutCount++;
            Logger.warning(`Heartbeat timeout count: ${this.timeoutCount}/${this.maxTimeoutCount}`);

            if (this.timeoutCount >= this.maxTimeoutCount) {
                Logger.error('Heartbeat timeout threshold reached');
                this.stop();
                this.onTimeout?.();
            }
        } else {
            this.timeoutCount = 0;
        }
    }

    /**
     * 处理心跳响应
     */
    public handleHeartbeatResponse(timestamp: number): void {
        this.lastResponseTime = Date.now();
        this.timeoutCount = 0;

        // 计算延迟
        const latency = this.lastResponseTime - timestamp;
        Logger.debug(`Heartbeat round-trip: ${latency}ms`);
    }

    /**
     * 获取心跳统计
     */
    public getStats(): { interval: number; timeoutCount: number; lastHeartbeatTime: number; lastResponseTime: number } {
        return {
            interval: this.interval,
            timeoutCount: this.timeoutCount,
            lastHeartbeatTime: this.lastHeartbeatTime,
            lastResponseTime: this.lastResponseTime
        };
    }

    /**
     * 销毁
     */
    public dispose(): void {
        this.stop();
    }
}
