# TODO 任务计划

## 组件补充计划

### 高优先级

- [ ] 实现 db-app 数据库服务组件
  - 数据库连接池管理（MySQL）
  - 异步存档接口
  - AccountDB - 账号数据库
  - CharacterDB - 角色数据库
  - WorldDB - 世界数据库
  - 与 BaseApp/CellApp 的 RPC 通信

- [ ] 实现 cell-appmgr 空间应用管理器
  - 管理所有 CellApp 实例
  - 空间分区负载均衡
  - 动态边界调整
  - Entity 跨 CellApp 迁移调度
  - 过载自动扩容

### 中优先级

- [ ] 实现 base-appmgr 基础应用管理器
  - 管理所有 BaseApp 实例
  - 分配新客户端连接到合适的 BaseApp
  - 负载均衡
  - 跟踪 BaseApp 状态

## 功能补充计划

### 核心 MMO 功能

- [ ] AI 导航/寻路系统
  - Navigation Mesh 支持
  - A*/Recast 寻路算法
  - 服务端 NPC 自动移动
  - 地形数据构建工具

- [ ] Entity 定义系统
  - Entity 定义文件格式
  - 属性/方法声明与解析
  - 组件继承机制
  - def 文件解析器

- [ ] 自动数据持久化
  - Entity 自动序列化
  - 脏数据检测与同步
  - 数据库自动备份

### 客户端 SDK

- [ ] Unity SDK 实现
- [ ] UE4/UE5 SDK
- [ ] Cocos2d-x SDK
- [ ] JavaScript/WebSocket SDK

### 脚本系统

- [ ] 脚本抽象层设计（支持 Lua/Python 可选）
- [ ] Lua 嵌入与绑定
- [ ] Python 嵌入与绑定
- [ ] 脚本热更新机制

### 监控与运维

- [ ] Console 监控接口
- [ ] Web 监控界面
- [ ] 性能指标采集
- [ ] 服务器状态可视化

## 文档完善计划

- [ ] 完成 QA.md 中的答案编写
- [ ] 各组件详细设计文档
- [ ] API 参考文档
- [ ] 部署运维文档
