# FishNet 架构分析

## 1. 定位

FishNet 是 Unity 生态里另一个很常见的多人网络框架。  
和 Mirror 类似，它不是完整 MMO 框架，但在 MMO 原型和 Unity 实时项目里常被拿来评估。

## 2. 核心架构

FishNet 强调的关键能力包括：

- observer system
- scene visibility
- server/client ownership
- 组件化网络系统

其中和 MMO 最相关的是：

- `NetworkObserver`
- 自定义 observer conditions

## 3. MMO 核心能力分析

### 3.1 世界模型

FishNet 不提供完整世界引擎。  
它更像：

- 同步框架
- 可观察性和可见性控制框架

### 3.2 同步

Observer System 是它最值得关注的点。  
官方文档明确允许：

- 通过条件定义哪些客户端成为对象的 observer
- 自定义 observers 规则

这使它在 AOI / visibility 这类问题上比一些轻量网络库更进一步。

### 3.3 场景与可见性

Scene Visibility 也是一个重要能力。  
这对：

- 多场景
- 分图加载
- 副本

很有帮助。

## 4. 特色

- Observer System 更灵活
- Scene Visibility 适合复杂 Unity 项目
- 很适合想自己做上层 MMO 骨架的 Unity 团队

## 5. 优点

- 网络可见性控制能力强
- 比单纯房间同步更适合复杂实时世界
- 适合构建 MMO-like 原型

## 6. 缺点

- 仍然不是完整 MMO backend
- 持久化、社交、账号、编排仍需外层解决

## 7. 适用场景

适合：

- Unity MMO-like 原型
- 自建服务端逻辑但需要成熟同步层
- 有多场景和可见性需求的实时项目

## 8. 对 Apollo 的启发

最值得吸收的是：

- observer 规则最好可配置和可扩展
- scene visibility 对分区和实例管理很有帮助

## 9. 参考资料

- NetworkObserver：https://fish-networking.gitbook.io/docs/fishnet-building-blocks/components/network-observer
- Custom Conditions：https://fish-networking.gitbook.io/docs/guides/features/observers/custom-conditions
- Scene Visibility：https://fish-networking.gitbook.io/docs/guides/features/scene-management/scene-visibility
