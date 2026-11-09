#pragma once

// SSEngine 命名空间和类型定义
// 注意: 实际使用时需要包含 SSEngine 头文件并配置正确的 include 路径
namespace SSCP {

// 基本类型定义 (与 SSEngine 兼容)
using INT8 = signed char;
using UINT8 = unsigned char;
using INT16 = signed short;
using UINT16 = unsigned short;
using INT32 = signed int;
using UINT32 = unsigned int;
using INT64 = long long;
using UINT64 = unsigned long long;

// 版本结构
struct SSSVersion {
    UINT16 wMajorVersion;
    UINT16 wMinorVersion;
    UINT16 wCompatibleVersion;
    UINT16 wBuildNumber;
};

// 基础接口
class ISSBase {
public:
    virtual ~ISSBase() {}
    virtual void AddRef() = 0;
    virtual UINT32 QueryRef() = 0;
    virtual void Release() = 0;
    virtual SSSVersion GetVersion() = 0;
    virtual const char* GetModuleName() = 0;
};

// 前向声明
class ISSConnection;
class ISSPacketParser;
class ISSSessionFactory;

// 网络错误码
enum ESDNetErrCode {
    NET_BIND_FAIL = -7,
    NET_CONNECT_FAIL = -6,
    NET_SYSTEM_ERROR = -5,
    NET_RECV_ERROR = -4,
    NET_SEND_ERROR = -3,
    NET_SEND_OVERFLOW = -2,
    NET_PACKET_ERROR = -1,
    NET_SUCCESS = 0
};

// I/O 类型
constexpr UINT32 NETIO_COMPLETIONPORT = 1;
constexpr UINT32 NETIO_COMPLETIONPORT_GATE = 11;
constexpr UINT32 NETIO_ASYNCSELECT = 2;
constexpr UINT32 NETIO_EPOLL = 10;
constexpr UINT32 NETIO_EPOLL_GATE = 101;

/**
 * @brief SSEngine 连接接口
 */
class ISSConnection {
public:
    virtual ~ISSConnection() {}
    virtual bool IsConnected() = 0;
    virtual void Send(const char* pBuf, UINT32 dwLen) = 0;
    virtual void DelaySend(const char* pBuf, UINT32 dwLen) = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
    virtual void Disconnect() = 0;
    virtual const UINT32 GetRemoteIP() = 0;
    virtual const char* GetRemoteIPStr() = 0;
    virtual UINT16 GetRemotePort() = 0;
    virtual const UINT32 GetLocalIP() = 0;
    virtual const char* GetLocalIPStr() = 0;
    virtual UINT16 GetLocalPort() = 0;
    virtual UINT32 GetSendBufFree() = 0;
};

/**
 * @brief SSEngine 会话接口
 */
class ISSSession {
public:
    virtual ~ISSSession() {}
    virtual void SetConnection(ISSConnection* poConnection) = 0;
    virtual void OnEstablish() = 0;
    virtual void OnTerminate() = 0;
    virtual bool OnError(INT32 nModuleErr, INT32 nSysErr) = 0;
    virtual void OnRecv(const char* pBuf, UINT32 dwLen) = 0;
    virtual void Release() = 0;
    virtual void ActiveClose() {}
    virtual bool IsActiveClose() { return false; }
    virtual void SetActiveClose(bool bFlag) {}
};

/**
 * @brief SSEngine 数据包解析器接口
 */
class ISSPacketParser {
public:
    virtual ~ISSPacketParser() {}
    virtual INT32 ParsePacket(const char* pBuf, UINT32 dwLen) = 0;
    virtual INT32 ParseFirstPacket(const char* pBuf, UINT32 dwLen) {
        return ParsePacket(pBuf, dwLen);
    }
};

/**
 * @brief SSEngine 会话工厂接口
 */
class ISSSessionFactory {
public:
    virtual ~ISSSessionFactory() {}
    virtual ISSSession* CreateSession(ISSConnection* poConnection) = 0;
};

/**
 * @brief SSEngine 监听器接口
 */
class ISSListener {
public:
    virtual ~ISSListener() {}
    virtual void SetPacketParser(ISSPacketParser* poPacketParser) = 0;
    virtual void SetSessionFactory(ISSSessionFactory* poSessionFactory) = 0;
    virtual void SetBufferSize(UINT32 dwRecvBufSize, UINT32 dwSendBufSize) = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
    virtual bool Start(const char* pszIP, UINT16 wPort, bool bReUseAddr = true) = 0;
    virtual bool Stop() = 0;
    virtual void Release() = 0;
};

/**
 * @brief SSEngine 连接器接口
 */
class ISSConnector {
public:
    virtual ~ISSConnector() {}
    virtual void SetPacketParser(ISSPacketParser* poPakcetParser) = 0;
    virtual void SetSession(ISSSession* poSession) = 0;
    virtual ISSSession* GetSession() = 0;
    virtual void SetBufferSize(UINT32 dwRecvBufSize, UINT32 dwSendBufSize) = 0;
    virtual int Connect(const char* pszIP, UINT16 wPort) = 0;
    virtual int ReConnect() = 0;
    virtual void Release() = 0;
    virtual void SetOpt(UINT32 dwType, void* pOpt) = 0;
};

/**
 * @brief SSEngine 网络模块接口
 */
class ISSNet : public ISSBase {
public:
    virtual ~ISSNet() {}
    virtual ISSConnector* CreateConnector(UINT32 dwNetIOType) = 0;
    virtual ISSListener* CreateListener(UINT32 dwNetIOType) = 0;
    virtual bool Run(INT32 nCount = -1) = 0;
};

// SDNet 模块获取函数 (需要在实际链接时提供)
// ISSNet* SSNetGetModule(const SSSVersion* pstVersion);

} // namespace SSCP

