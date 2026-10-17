---
title: 应用模块装配
---

# 应用模块装配

应用入口显式选择模块、构造依赖并接入宿主。不为装配新增业务进程，也不引入未实现的运行期 Starter 容器。

## 装配与部署

CMake target 决定构建和链接依赖；应用配置决定本次启动启用什么；宿主负责已经注册的服务生命周期。三者不能互相代替。

| 业务程序 | 应用内服务 | 边界 |
|---|---|---|
| Login | 认证、OnlineDirectory、准入链路 | 不持有玩家游戏档案 |
| Gateway | 客户端连接、准入客户端、目录查询、World 出口 | 不推进玩法模拟 |
| World | WorldHost、World/Scene/Instance、PlayerStore、Lua | 持有当前模拟与玩家资料写权 |

`apps/common` 提供 PlayerStore 和 OnlineDirectory 的共享实现，编译为 `apollo_app_services`，不生成额外业务程序。运维工具和 machined 按部署需要独立运行。

## 模块依赖

应用通过既有 CMake target 链接模块。组件依赖应在构建和启动期显式可见，不在消息处理时按字符串动态搜索实现，也不通过运行期扫描来决定进程拓扑。

需要构造注入时使用 `ApplicationContextBuilder`；需要生命周期管理时使用 ApplicationHost / ServiceHost 或 WorldHost。是否使用依赖注入不改变服务的唯一所有者。当前应用仍有显式构造与停止逻辑，不能写成全部已迁入容器。

## 启动与停止

1. 校验应用配置及契约产物。
2. 初始化日志、崩溃采集和应用所需设施。
3. 构造所属服务及其依赖，启动监听与执行线程。
4. 按服务就绪状态开放准入。
5. 停止时关闭新准入、排空工作并释放业务状态，再逆序关闭依赖。

以上是装配规则；各入口的就绪闸、排空期限和异常补偿必须分别实现和验证。CMake 链接成功不代表网络端点就绪，也不代表启动失败能完整恢复。

## Lua 与热更新

当前脚本后端使用 Lua C API 和 ScriptRuntime。补丁只在所属逻辑边界提交，不运行期增删 C++ 服务。代码更新和协议更新分开验收。

## 跨服预留

普通与跨服场景复用同一 World 程序及 Scene/Instance 类型。以后可按部署配置选择承载跨服活动的 World 池，不能据此引入新的进程角色或同义对象。参见[跨服场景设计](../design/cross-server-scenes.md)。

## 关联文档

- [宿主与依赖注入](./host-builder-and-di-design.md)
- [服务器应用](../apps/index.md)
- [当前架构决策](./adr.md)
