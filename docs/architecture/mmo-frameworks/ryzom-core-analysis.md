# Ryzom Core 架构分析

## 1. 定位

Ryzom Core 代表的是另一种很有价值的 MMO 架构传统：

- 多服务拆分
- 长生命周期商业 MMO 风格
- 服务网格化
- MMORPG 专项算法和网络层优化

它比 TrinityCore 更“服务化”，比 KBEngine 更“工业服务云”。

## 2. 核心架构

官方服务架构文档列出了大量服务：

- `AES`
- `AS`
- `AIS`
- `BMS`
- `EGS`
- `GPMS`
- `IOS`
- `NS`
- `WS`
- `TS`
- `MS`
- `SU`
- `FES`

这类命名看起来很老派，但本质上说明它采取的是：

- 明确的专职服务拆分
- 命名与发现服务
- Tick 同步服务
- AI 服务
- 前端接入服务
- 全局位置服务

## 3. MMO 核心能力分析

### 3.1 服务拆分

Ryzom Core 的核心不是单个 worldserver，而是“一个 shard 由一组服务协同完成”。

这是非常典型的商业 MMO 运营级思路。

### 3.2 网络与变量数据库

官方特性文档提到：

- 位打包 UDP
- 固定时间速率连接
- packet loss 管理
- tree structured network variable database
- XML 定义网络变量数据库

这意味着它不是简单发 protobuf 包，而是存在更强的 MMO 专用同步建模。

### 3.3 地理区域和 AI

`AIS` 负责地理区域 AI，`GPMS` 负责全局位置，这说明它对空间和实体位置管理有明确服务化建模。

## 4. 特色

- 服务网格拆分极强
- 很像传统商业 MMORPG 运营架构
- 网络层对 MMO 流量特点有很强针对性
- 有“变量数据库”这一类比普通房间服更偏 MMO 的抽象

## 5. 优点

- 对复杂 MMORPG 的系统拆分非常有参考价值
- 服务职责很细
- 适合研究 MMO 生产级服务布局

## 6. 缺点

- 架构年代感明显
- 理解和上手成本高
- 资料分散，生态不如主流现代框架活跃

## 7. 适用场景

适合：

- 研究大型 MMORPG 服务拆分
- 分析传统商业 MMO 的专职服务布局
- 参考“非单世界服”的老派成熟方案

## 8. 对 Apollo 的启发

最值得吸收的是：

- naming / tick / front-end / AI / position 这类服务边界
- 将 MMO 核心职责拆成长期运行的明确服务
- 变量数据库和同步建模思路

## 9. 参考资料

- 服务架构：https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884873/Ryzom%2BService%2BArchitecture
- 项目结构：https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884861/Ryzom%2BCore%2BProject%2BStructure
- 特性概览：https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884824/Ryzom%2BCore%2BFeatures
