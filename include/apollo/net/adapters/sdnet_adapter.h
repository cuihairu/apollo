#pragma once

#include "apollo/net/net.h"
#include <atomic>
#include <cstdio>

// 定义使用 SDNet 后端
#define APOLLO_USE_SDNET
#ifdef APOLLO_USE_SDNET

// 拆分单元（LAY-007）：SSCP 门面类型 / 工具 / 连接与帧解析 / 会话映射 /
// 监听器与连接器 / 网络管理器——原单头实现按职责分文件，此处聚合保持
// include 面不变
#include "apollo/net/adapters/sdnet_sscp.h"
#include "apollo/net/adapters/sdnet_util.h"
#include "apollo/net/adapters/sdnet_connection.h"
#include "apollo/net/adapters/sdnet_session.h"
#include "apollo/net/adapters/sdnet_endpoint.h"
#include "apollo/net/adapters/sdnet_manager.h"

#endif // APOLLO_USE_SDNET
