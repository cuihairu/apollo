# XML 契约生成与装载

当前 XML 消费主要是构建期契约解析与生成，不是通用运行期配置容器。代码事实源见 modules/contract、sdks/gen 和 sdks/contract。

## 执行摘要

pugixml 解析模型，语义校验和哈希由契约模块完成，apollo_gen 生成产物；CI 执行 XSD 与 golden 检查。运行时服务消费生成元数据，不链接生成器。策划表 typed loader、声明式存储语句及相关热替换仍需独立接线。

## 1. 当前消费方

| 部件 | 当前事实 |
|---|---|
| sdks/contract | 属性、实体、消息、错误、版本与 XSD |
| modules/contract | 解析、模型校验、继承展开、哈希与测试 |
| sdks/gen | apollo_gen 命令及逐字节产物检查 |
| sdks/cpp/generated | C++、JSON、Lua、路由、语义和 proto 产物 |
| 运行时 | 装载生成表；不等于任意 XML 配置热更 |

外部引擎使用 XML 的事实不决定 Apollo 的进程拓扑或存储接口。

## 2. 用途边界

XML/XSD 用于可维护、可校验的声明数据。线上消息仍是二进制编码，Lua 仍承担脚本逻辑，大块几何资产和日志不因此转为 XML。

storage.xml、策划数值表和帧配置是独立方案，需要真实消费者、schema、装载验证和失败策略；不能在现状表中写成全部已实现。

## 3. 生成源

契约 XML 实例经 XSD 和语义规则校验后进入 ContractModel。生成代码和运行端元数据是投影，不允许在各 SDK 再维护不同的字段或权限事实。

变更描述、类型、字段和语义时应检查生成结果与协议指纹影响；纯文本编辑不能未经验证就当成不影响发布的变更。

## 4. 当前工具链

```bash
apollo_gen --contract sdks/contract --out sdks/cpp/generated
apollo_gen --contract sdks/contract --check sdks/cpp/generated
```

产物包括 apollo_contract.h、apollo_contract.json、contract.lua、semantic.json、contract_route.json 和完整/客户端 `.proto`。protoc 在 contract_pack.sh 中生成 descriptor 并签发含文件摘要的 manifest。生成器自身不生成数据库服务，也不直接为业务进程装载新协议。

## 5. 校验

1. XSD 校验输入结构、允许类型与声明约束。
2. 契约模块校验 ID/域、引用、继承环和跨字段关系。
3. 生成头文件通过编译测试，golden 检查确保提交产物与源一致。
4. 消费者启动时校验包、摘要、路由和绑定条件。

前三级已经有工具与测试；第四级的具体接线按消费者验收。包可生成、可解析与业务可兼容运行分别记录。

## 6. 存储声明的预留

可借鉴声明式语句、启动校验和数据映射的边界，但持久化不进入客户端契约。当前业务仍使用本地玩家档案、SaveQueue 和 PersistJournal；SQL/Redis 等需求启用后，再实现真实后端、参数绑定、错误/重试及版本迁移。

不能把一个 XML mapper 样例、数据库字段或依赖声明当成存储已经接入。

## 7. 更新与失败

仅代码更新保留 wire；契约和编码变化进入协议版本、兼容窗口和发布回滚流程。不存在“替换 XML 即保证所有客户端无感”的通用承诺。

任何新装载器都应先验证完整新版本，再在所属执行边界切换；失败保留可用版本并给出明确结果。实际机制以具体消费者实现为准。

## 8. 关联设计

- [SDK 契约](./sdk-contract.md)
- [Lua 更新](./scripting-lua.md)
- [属性同步](./attribute-sync.md)
- [应用装配](../architecture/starter-and-module-assembly-design.md)
