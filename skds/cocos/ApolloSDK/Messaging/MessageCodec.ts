/**
 * 消息编解码器
 * 负责消息的序列化和反序列化
 */

import { MessageHeader } from '../Network/NetworkTypes';
import { IMessage } from './IMessage';
import { ByteBuffer } from '../Utilities/ByteBuffer';

/**
 * 消息编解码器类
 */
export class MessageCodec {
    private static readonly HEADER_SIZE = 16;
    private static readonly MAGIC_NUMBER = 0x414F4C4F; // "OLO" in hex

    /**
     * 编码消息
     * @param messageId 消息ID
     * @param message 消息对象
     * @returns ArrayBuffer 编码后的二进制数据
     */
    public encode(messageId: number, message: IMessage): ArrayBuffer {
        // 序列化消息体
        const bodyData = message.encode();
        const bodyLength = bodyData.byteLength;

        // 创建消息头
        const header = this.createHeader(messageId, bodyLength);

        // 合并头和消息体
        const result = new ArrayBuffer(this.HEADER_SIZE + bodyLength);
        const view = new Uint8Array(result);

        // 复制消息头
        view.set(new Uint8Array(header), 0);
        // 复制消息体
        view.set(new Uint8Array(bodyData), this.HEADER_SIZE);

        return result;
    }

    /**
     * 解码消息
     * @param data 接收到的二进制数据
     * @returns { header: MessageHeader; body: ArrayBuffer } 解码后的消息头和消息体
     */
    public decode(data: ArrayBuffer): { header: MessageHeader; body: ArrayBuffer } {
        if (data.byteLength < this.HEADER_SIZE) {
            throw new Error('Invalid message: data too short');
        }

        const header = this.decodeHeader(data);
        const body = data.slice(this.HEADER_SIZE);

        return { header, body };
    }

    /**
     * 解码消息头
     * @param data 二进制数据
     * @returns MessageHeader 消息头
     */
    public decodeHeader(data: ArrayBuffer): MessageHeader {
        const buffer = new ByteBuffer(data);
        const magic = buffer.readUint32();

        if (magic !== this.MAGIC_NUMBER) {
            throw new Error(`Invalid magic number: 0x${magic.toString(16)}`);
        }

        return {
            length: buffer.readUint32(),
            msgId: buffer.readUint16(),
            seq: buffer.readUint32(),
            flags: buffer.readUint16()
        };
    }

    /**
     * 创建消息头
     * @param messageId 消息ID
     * @param bodyLength 消息体长度
     * @returns ArrayBuffer 消息头数据
     */
    private createHeader(messageId: number, bodyLength: number): ArrayBuffer {
        const header = new ArrayBuffer(this.HEADER_SIZE);
        const buffer = new ByteBuffer(header);

        buffer.writeUint32(this.MAGIC_NUMBER);
        buffer.writeUint32(bodyLength);
        buffer.writeUint16(messageId);
        buffer.writeUint32(this.getNextSequence());
        buffer.writeUint16(0); // flags

        return header;
    }

    /**
     * 获取下一个序列号
     */
    private sequence: number = 0;
    private getNextSequence(): number {
        return ++this.sequence;
    }
}
