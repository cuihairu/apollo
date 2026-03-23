# Mirror 架构分析

## 1. 定位

Mirror 是 Unity 生态里非常常见的开源网络框架。  
它不是 MMO 框架，但在多人、AOI 和 interest management 讨论里很常见。

## 2. 核心架构

Mirror 的核心是：

- NetworkIdentity
- NetworkBehaviour
- Server-authoritative replication
- Interest Management

对 MMO 最 relevant 的是它明确提供多种 Interest Management 方案。

## 3. MMO 核心能力分析

### 3.1 世界模型

Mirror 本身不提供 MMO 世界模型。  
它更像网络层和同步层。

### 3.2 同步

Mirror 最值得关注的是 Interest Management。  
官方文档直接用 MMO 作为例子，指出如果向所有玩家广播整个世界状态，会完全不可扩展。

所以它提供的是：

- 只向相关客户端同步相关对象
- 以兴趣范围控制同步规模

### 3.3 持久化与业务

Mirror 基本不关心：

- 持久化
- 账号
- 社交
- MMORPG 世界逻辑

这些都需要外层补。

## 4. 特色

- 轻量
- Unity 社区常见
- interest management 作为正式能力存在

## 5. 优点

- 接入 Unity 项目容易
- 网络对象同步简单直接
- 适合自己构建上层 MMO 逻辑

## 6. 缺点

- 只是网络框架，不是 MMO backend
- 持久世界、编排、在线基础设施能力都缺
- 大世界仍需大量自建

## 7. 适用场景

适合：

- Unity 自研后端方案的网络层
- 中小型实时多人
- 需要定制同步模型的团队

## 8. 对 Apollo 的启发

最值得吸收的是：

- Interest Management 必须是一等设计问题
- 不要把 AOI 视作上线后再补的优化项

## 9. 参考资料

- 文档首页：https://mirror-networking.gitbook.io/docs
- Interest Management：https://mirror-networking.gitbook.io/docs/manual/interest-management
