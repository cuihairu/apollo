/**
 * 认证相关类型定义
 */

/**
 * 登录请求
 */
export interface LoginRequest {
    /** 用户名 */
    username: string;
    /** 密码 */
    password: string;
    /** 服务器ID */
    serverId?: number;
    /** 设备信息 */
    deviceInfo?: DeviceInfo;
}

/**
 * 设备信息
 */
export interface DeviceInfo {
    /** 设备ID */
    deviceId: string;
    /** 设备型号 */
    deviceModel: string;
    /** 操作系统 */
    os: string;
    /** OS版本 */
    osVersion: string;
    /** 应用版本 */
    appVersion: string;
}

/**
 * 登录响应
 */
export interface LoginResponse {
    /** 是否成功 */
    success: boolean;
    /** 用户ID */
    userId?: number;
    /** 会话令牌 */
    token?: string;
    /** 错误消息 */
    errorMessage?: string;
    /** 服务器时间 */
    serverTime?: number;
}

/**
 * 登出请求
 */
export interface LogoutRequest {
    /** 用户ID */
    userId: number;
    /** 会话令牌 */
    token: string;
}

/**
 * 登出响应
 */
export interface LogoutResponse {
    /** 是否成功 */
    success: boolean;
    /** 错误消息 */
    errorMessage?: string;
}
