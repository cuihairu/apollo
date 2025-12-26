/**
 * 字节缓冲区
 * 提供二进制数据的读写操作
 */

/**
 * 字节缓冲区类
 */
export class ByteBuffer {
    private data: ArrayBuffer;
    private view: DataView;
    private uint8Array: Uint8Array;
    private position: number = 0;

    constructor(data: ArrayBuffer | Uint8Array) {
        if (data instanceof ArrayBuffer) {
            this.data = data;
        } else {
            this.data = data.buffer.slice(data.byteOffset, data.byteOffset + data.byteLength);
        }
        this.view = new DataView(this.data);
        this.uint8Array = new Uint8Array(this.data);
    }

    /**
     * 创建指定大小的空缓冲区
     */
    public static allocate(size: number): ByteBuffer {
        return new ByteBuffer(new ArrayBuffer(size));
    }

    /**
     * 从字节数组创建
     */
    public static fromBytes(bytes: number[]): ByteBuffer {
        const buffer = new ByteBuffer(new ArrayBuffer(bytes.length));
        for (let i = 0; i < bytes.length; i++) {
            buffer.uint8Array[i] = bytes[i];
        }
        return buffer;
    }

    // ==================== 读取操作 ====================

    public readInt8(): number {
        const value = this.view.getInt8(this.position);
        this.position += 1;
        return value;
    }

    public readUint8(): number {
        const value = this.view.getUint8(this.position);
        this.position += 1;
        return value;
    }

    public readInt16(): number {
        const value = this.view.getInt16(this.position, this.isLittleEndian());
        this.position += 2;
        return value;
    }

    public readUint16(): number {
        const value = this.view.getUint16(this.position, this.isLittleEndian());
        this.position += 2;
        return value;
    }

    public readInt32(): number {
        const value = this.view.getInt32(this.position, this.isLittleEndian());
        this.position += 4;
        return value;
    }

    public readUint32(): number {
        const value = this.view.getUint32(this.position, this.isLittleEndian());
        this.position += 4;
        return value;
    }

    public readFloat32(): number {
        const value = this.view.getFloat32(this.position, this.isLittleEndian());
        this.position += 4;
        return value;
    }

    public readFloat64(): number {
        const value = this.view.getFloat64(this.position, this.isLittleEndian());
        this.position += 8;
        return value;
    }

    public readBytes(length: number): Uint8Array {
        const bytes = this.uint8Array.slice(this.position, this.position + length);
        this.position += length;
        return bytes;
    }

    public readString(): string {
        const length = this.readUint16();
        const bytes = this.readBytes(length);
        return new TextDecoder().decode(bytes);
    }

    // ==================== 写入操作 ====================

    public writeInt8(value: number): void {
        this.view.setInt8(this.position, value);
        this.position += 1;
    }

    public writeUint8(value: number): void {
        this.view.setUint8(this.position, value);
        this.position += 1;
    }

    public writeInt16(value: number): void {
        this.view.setInt16(this.position, value, this.isLittleEndian());
        this.position += 2;
    }

    public writeUint16(value: number): void {
        this.view.setUint16(this.position, value, this.isLittleEndian());
        this.position += 2;
    }

    public writeInt32(value: number): void {
        this.view.setInt32(this.position, value, this.isLittleEndian());
        this.position += 4;
    }

    public writeUint32(value: number): void {
        this.view.setUint32(this.position, value, this.isLittleEndian());
        this.position += 4;
    }

    public writeFloat32(value: number): void {
        this.view.setFloat32(this.position, value, this.isLittleEndian());
        this.position += 4;
    }

    public writeFloat64(value: number): void {
        this.view.setFloat64(this.position, value, this.isLittleEndian());
        this.position += 8;
    }

    public writeBytes(bytes: Uint8Array | number[]): void {
        for (const byte of bytes) {
            this.uint8Array[this.position++] = byte;
        }
    }

    public writeString(str: string): void {
        const encoder = new TextEncoder();
        const bytes = encoder.encode(str);
        this.writeUint16(bytes.length);
        this.writeBytes(bytes);
    }

    // ==================== 工具方法 ====================

    /**
     * 获取当前位置
     */
    public getPosition(): number {
        return this.position;
    }

    /**
     * 设置位置
     */
    public setPosition(pos: number): void {
        if (pos < 0 || pos > this.data.byteLength) {
            throw new Error(`Invalid position: ${pos}`);
        }
        this.position = pos;
    }

    /**
     * 获取剩余字节数
     */
    public getRemaining(): number {
        return this.data.byteLength - this.position;
    }

    /**
     * 获取数据长度
     */
    public getLength(): number {
        return this.data.byteLength;
    }

    /**
     * 转换为数组
     */
    public toArray(): number[] {
        return Array.from(this.uint8Array);
    }

    /**
     * 转换为 ArrayBuffer
     */
    public toArrayBuffer(): ArrayBuffer {
        return this.data;
    }

    /**
     * 检查是否为小端序
     */
    private isLittleEndian(): boolean {
        return true; // 网络字节序通常是大端序，但这里使用小端序
    }
}
