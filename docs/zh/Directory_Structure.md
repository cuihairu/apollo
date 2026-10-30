
# Apollo 项目目录结构

## 概述

这份文档描述当前仓库的实际目录布局。

## 顶层目录

| 目录 | 说明 |
|------|------|
| `apps/` | 可执行服务与进程壳 |
| `modules/` | 模块化实现 |
| `docs/` | 文档站点 |
| `examples/` | 示例代码 |
| `tests/` | 测试代码 |
| `include/` | 公共头文件与兼容层头文件 |
| `src/` | 公共实现与兼容层实现 |
| `sdks/` | 契约源与客户端 SDK（contract/gen/cpp/unity） |
| `skds/` | Cocos / Laya / 历史 SDK 工作区 |
| `cmake/` | CMake 辅助文件 |
| `scripts/` | 脚本 |
| `build*/` | 构建输出 |

## apps/

| 目录 | 说明 |
|------|------|
| `login/` | 登录入口（认证、服务注册/在线目录、恢复编队面） |
| `gateway/` | 边缘接入层 |
| `base/` | base 独立入口，远程访问 database，承载长期资料与结算受理 |
| `database/` | 独立存储入口，M1 加载/保存与文件 journal |
| `common/` | 可复用的玩家资料、存储及目录实现；宿主按 Base/database 边界装配 |
| `world/` | world 入口（缺省管理角色；`world --role=cell` 已显式拒绝，执行端唯一入口为独立 `cellServer`）+ cellServer 执行端入口（执行内核 CellApp 已分离；cell 侧主动拨号 `--register-host/--register-port/--cell-id`、失联冻结/代次水位隔离 `--link-grace-ms`、多 Space 声明面 `--add-space`；world 管理端同样解析并生效 `--add-space`（经内嵌 CellApp 的 `config.spaces` 于启动窗逐条登记，cell_app.cpp 启动窗创建），即 `--add-space` 为 world 管理端与 cellServer 执行端共用声明面；world 侧管理面：cell 注册受理 960/961（含契约兼容投影裁决 code 5）、心跳失效窗 `--cell-expiry-ms`、World 分配面 962 `--assign-spaces`（含过期释放/重认领/撤销标记与占用重试的回收切片；条目可带能力位要求 `<id>[:<needs>]`）、跨重启持久墓碑 `--world-journal`、断连存疑标记；拓扑版本/重平衡编排待 P2 后续条目） |
| `machined/` | 本机进程监督（拉起/退避重启/死亡上报），不决定跨服路由 |
| `admin/`、`fleetctl/`、`logger/` | 运维与量测工具 |
| `apolloc/` | 项目初始化 CLI（`apolloc init <path>` 生成最小项目骨架；模板随 P0.2） |

## modules/

| 目录 | 说明 |
|------|------|
| `base/` | 基础工具与底座能力 |
| `core/` | 核心框架能力 |
| `contract/` | 契约源与生成器接入（XML 契约） |
| `data/` | 数据访问与缓存抽象 |
| `game/` | 游戏逻辑域 |
| `net/` | 网络与协议 |
| `protocol/` | 协议层 |
| `runtime/` | 运行时与宿主 |
| `bigworld/` | BigWorld 兼容层 |

## docs/

| 目录 | 说明 |
|------|------|
| `.vitepress/` | 站点配置与主题 |
| `guide/` | 入门指南 |
| `architecture/` | 架构文档（ADR 与语义参考件） |
| `analysis/` | 框架对照分析 |
| `design/` | 设计件（按域归档的设计与批记） |
| `apps/` | 应用设计文档 |
| `modules/` | 模块设计文档 |
| `api/` | API 参考 |
| `qa/` | 问答索引 |
| `analysis/` | 当前架构现状与差距分析 |
| `sdks/` | SDK 文档 |
| `public/` | 静态资源 |

## 备注

仓库里同时保留了 `sdks/` 和 `skds/` 两个目录名，其中 `sdks/` 是契约源与客户端
SDK 主目录（contract/gen/cpp/unity），`skds/` 是多平台 SDK 工作区与历史目录
（cocos/laya）。文档按实际用途描述，不再按旧版单树结构编排。
