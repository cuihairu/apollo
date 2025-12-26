/**
 * 消息接口定义
 */

/**
 * 消息接口
 * 所有通过 Apollo SDK 发送的消息都需要实现此接口
 */
export interface IMessage {
    /**
     * 序列化消息
     * @returns ArrayBuffer 序列化后的二进制数据
     */
    encode(): ArrayBuffer;

    /**
     * 反序列化消息
     * @param data 二进制数据
     */
    decode(data: ArrayBuffer): void;

    /**
     * 获取消息名称
     * @returns string 消息名称
     */
    getMessageName(): string;
}

/**
 * Protobuf 消息包装器
 * 用于包装 Google Protobuf 消息
 */
export class ProtobufMessage implements IMessage {
    private protoMessage: any;

    constructor(protoMessage: any) {
        this.protoMessage = protoMessage;
    }

    public encode(): ArrayBuffer {
        const uint8Array = this.protoMessage.serializeBinary();
        return uint8Array.buffer.slice(uint8Array.byteOffset, uint8Array.byteOffset + uint8Array.byteLength);
    }

    public decode(data: ArrayBuffer): void {
        const uint8Array = new Uint8Array(data);
        this.protoMessage = this.protoMessage.deserializeBinary(uint8Array);
    }

    public getMessageName(): string {
        return this.protoMessage.constructor.name || 'ProtobufMessage';
    }

    public getProtoMessage(): any {
        return this.protoMessage;
    }
}

/**
 * JSON 消息包装器
 * 用于包装 JSON 格式的消息
 */
export class JsonMessage<T extends object> implements IMessage {
    private data: T;

    constructor(data: T) {
        this.data = data;
    }

    public encode(): ArrayBuffer {
        const json = JSON.stringify(this.data);
        const encoder = new TextEncoder();
        return encoder.encode(json).buffer;
    }

    public decode(data: ArrayBuffer): void {
        const decoder = new TextDecoder();
        const json = decoder.decode(data);
        this.data = JSON.parse(json);
    }

    public getMessageName(): string {
        return 'JsonMessage';
    }

    public getData(): T {
        return this.data;
    }
}
