/**
 * 认证管理器
 * 负责用户登录、登出等认证操作
 */

import { NetworkManager } from '../Network/NetworkManager';
import { MessageRouter } from '../Messaging/MessageRouter';
import { LoginRequest, LoginResponse, LogoutRequest, LogoutResponse } from './AuthTypes';
import { Logger } from '../Utilities/Logger';

// 消息ID定义（根据实际协议定义）
const MSG_LOGIN_REQ = 1001;
const MSG_LOGIN_RESP = 1002;
const MSG_LOGOUT_REQ = 1003;
const MSG_LOGOUT_RESP = 1004;

/**
 * 认证管理器类
 */
export class AuthManager {
    private networkManager: NetworkManager;
    private messageRouter: MessageRouter;
    private pendingRequests: Map<number, { resolve: (value: any) => void; reject: (reason: any) => void; timeout: number }> = new Map();
    private requestSequence: number = 0;

    // 事件回调
    public onAuthenticated?: () => void;
    public onLoggedOut?: () => void;

    constructor(networkManager: NetworkManager, messageRouter: MessageRouter) {
        this.networkManager = networkManager;
        this.messageRouter = messageRouter;
        this.registerHandlers();
    }

    /**
     * 登录
     * @param request 登录请求
     * @returns Promise<LoginResponse> 登录响应
     */
    public async login(request: LoginRequest): Promise<LoginResponse> {
        Logger.info(`Logging in: ${request.username}`);

        return new Promise<LoginResponse>((resolve, reject) => {
            const seq = this.getNextSequence();

            // 设置超时
            const timeout = setTimeout(() => {
                this.pendingRequests.delete(seq);
                reject(new Error('Login timeout'));
            }, 10000) as unknown as number;

            // 保存请求回调
            this.pendingRequests.set(seq, { resolve, reject, timeout });

            // 发送登录请求
            this.networkManager.send(MSG_LOGIN_REQ, request);
        });
    }

    /**
     * 登出
     * @returns Promise<LogoutResponse> 登出响应
     */
    public async logout(): Promise<LogoutResponse> {
        Logger.info('Logging out');

        // TODO: 实现登出逻辑
        return { success: true };
    }

    /**
     * 注册消息处理器
     */
    private registerHandlers(): void {
        // 注册登录响应处理器
        this.messageRouter.registerHandler(MSG_LOGIN_RESP, (response: LoginResponse) => {
            this.handleLoginResponse(response);
        });

        // 注册登出响应处理器
        this.messageRouter.registerHandler(MSG_LOGOUT_RESP, (response: LogoutResponse) => {
            this.handleLogoutResponse(response);
        });
    }

    /**
     * 处理登录响应
     */
    private handleLoginResponse(response: LoginResponse): void {
        // 简化处理：直接通知认证成功
        if (response.success) {
            Logger.info('Authentication successful');
            this.onAuthenticated?.();
        } else {
            Logger.error(`Authentication failed: ${response.errorMessage}`);
        }
    }

    /**
     * 处理登出响应
     */
    private handleLogoutResponse(response: LogoutResponse): void {
        if (response.success) {
            Logger.info('Logout successful');
            this.onLoggedOut?.();
        } else {
            Logger.error(`Logout failed: ${response.errorMessage}`);
        }
    }

    /**
     * 获取下一个序列号
     */
    private getNextSequence(): number {
        return ++this.requestSequence;
    }

    /**
     * 销毁
     */
    public dispose(): void {
        // 清理所有待处理的请求
        for (const [seq, request] of this.pendingRequests) {
            clearTimeout(request.timeout);
            request.reject(new Error('AuthManager disposed'));
        }
        this.pendingRequests.clear();
    }
}
