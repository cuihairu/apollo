# SDK 契约与消息编码

XML/XSD 契约是生成元数据的事实源。契约、字节编码、传输与业务结果分别验收；不能把生成产物存在当成运行链路已经接通。

## 执行摘要

解析器、生成器、哈希、域过滤、实体继承展开、slice 声明及 golden 检查已经存在。部分应用仍使用 MessageCodec 与 M1 帧转换。N/N-1 协商、完整客户端接线与生产发布流程仍在 todo.md。

## 协议更新与仅代码更新

协议更新改变契约或实际 wire，必须重新生成、分类兼容性并验证发布/回滚。仅代码更新保持契约和 wire：Lua 使用 prepare / commit / canary / rollback，C++ 使用排空后的进程替换。二者都不能仅凭 Gateway 存在宣称无感更新。

## 1. 当前接线边界

`sdks/` 包含契约、生成器、C++ 产物与 SDK；`skds/` 仍有平台 SDK 工作区。目录及端侧消费尚未全部收敛。迁移须按真实消费者验证，不能写成已经完成所有平台接入。

## 2. 契约模型

### 2.1 输入

`sdks/contract/` 包含 attrs.xml、entities.xml、messages.xml、errors.xml、version 和 apollo.xsd。解析与语义校验位于 `modules/contract`，工具位于 `sdks/gen`。

### 2.2 类型

现有 WireType 包括 bool、int32、int64、float、double、string、bytes、string 列表和 int64 列表。alias 只命名已有基元，不创建另一种线上类型。嵌套对象不能从设计示例推断为已受支持。

### 2.3 属性与继承

属性声明 ID、类型、可见掩码、通道、预测权限和可选 slice 信息。slice 上限与元素类型成对声明并校验。实体单继承在生成期展开，父先入、子覆盖、禁止同名改型；运行端消费扁平属性集。

存储标记不进入线上契约。持久字段、数据库表和 journal 由服务端持久化职责管理，不通过客户端同步掩码直接控制保存。

## 3. 生成产物

当前 golden 产物为：

| 文件 | 用途 |
|---|---|
| apollo_contract.h | C++ ID 与元数据 |
| apollo_contract.json | 契约 JSON 投影 |
| contract.lua | Lua 契约和绑定元数据 |
| semantic.json | 客户端语义投影 |
| contract_route.json | 消息路由和字段布局 |
| apollo_contract.proto | 完整 protobuf schema |
| apollo_contract_client.proto | 仅客户端域的 protobuf schema |

生成器不进入业务运行时依赖图。应用消费生成产物；各客户端不得自行实现另一套契约解析规则。

## 4. SDK 职责

区分编解码、属性基线/增量状态、实体/AOI 生命周期以及表现事件。相同契约在 C++、C#、TS 等端可生成符合各语言习惯的 API，但字段和消息语义必须一致。

## 5. 服务端接入

按路由表校验消息方向和域，绑定已注册 handler。Lua 的属性写权限和消息入口来自受控元数据；网络回调不直接修改 Scene。当前 World 的实际应用帧转换及脚本接线需要按真实消费者验收。

## 6. 版本与兼容

生成物包含 schema、client 和 internal 指纹。客户端包只暴露客户端域，内部变化不应把内部 schema 泄露给客户端。哈希可判断不一致，不能独自判断兼容。

当前 InterServerLink 的握手受理集 = {本代 route_version} ∪ compat_route_versions（N/N-1 发布窗口；`APOLLO_ROUTE_COMPAT` 以旧代 schema_hash 列表开窗，双向声明、收敛撤窗、窗外仍拒），对端代次经 `peer_route_version()` 可观测。业务层 N/N-1 双版本解码尚未提供：窗口只保证握手与路由 id 可达，按代次选解码器必须由消费者按真实编码与语义另行验证。

字段增加不自动兼容：当前定长字段序列编码、protobuf 编码以及业务默认值可能有不同结果。变更分类必须依据真实使用的编码和语义，不能只依据 XML diff。

## 7. 存储边界

线上类型、可见性和消息版本不决定数据库表。服务端可在明确迁移规则下独立演进档案、journal 或将来按需接入的数据库。storage.xml 是声明式语句方案的设计方向，不是当前业务已经使用的存储入口。

## 8. 当前工作

实施状态仅维护在根目录 todo.md。端侧收敛、实际编码迁移和发布兼容验收分别完成，不维护另一份按旧阶段编号追加的路线。

## 9. 关联设计

- [XML 生成](./xml-generation.md)
- [属性同步](./attribute-sync.md)
- [Lua](./scripting-lua.md)
- [网络边界](./net-abstraction.md)

## 10. 语义、编码与压缩

### 10.1 语义与字节

契约声明字段含义、方向、域与权限；编码把值变成字节；压缩处理字节流。不能从一种编码格式推导业务授权。

### 10.2 字段映射

类型、列表、字段顺序与默认值以生成器和对应解码器为准。实际应用若仍使用手写 MessageCodec，必须验证转换一致性，不能把同名结构当成同一 wire。

### 10.3 工具链

apollo_gen 生成文本/元数据；protoc 在打包工具中生成 descriptor。生成 schema、descriptor 可加载及运行时实际消费是三个不同结果。

### 10.4 属性差分

属性基线、序号和域投影遵循属性同步规则。编码库不能替代观察者状态或重建缺失基线。

### 10.5 zstd

zstd 属于可选帧压缩，须有阈值、解压上限、版本与性能验证。当前不宣称它已经进入真实玩法路径。

### 10.6 native 与 reflect

消息有 native / reflect 绑定元数据；缺省按通道解析。Lua 可以消费生成路由和语义，但不能由 reflect 标记推导完整 protobuf 反射链路已经接通。当前 Lua 绑定使用 C API。

### 10.7 发布边界

改 Lua handler 而不改契约属于仅代码更新；改变字段、消息、布局或语义属于协议审查范围。代码更新也需验证状态兼容、可靠投递和失败回滚。

## 11. 内外分域与交付包

### 11.1 域

client 消息 ID 为 1–899，internal 为 900–65535。消息方向和域分别校验，不能只凭 ID 段接受客户端发来的服务器内部操作。

### 11.2 标识稳定性

已发布的 ID、字段含义和编码不能任意复用。迁移应用命名不应隐式修改现役 ABI；确需修改时进入协议版本流程。

### 11.3 双域指纹

client_hash 描述客户端域，internal_hash 描述内部域，schema_hash 描述完整契约。客户端包不携带内部消息定义；服务器按对应包和指纹装载。

### 11.4 文件组织

文件拆分服务于维护，不改变逻辑域。共享模型由同一解析器生成，不在每个客户端另建解释器。

### 11.5 装载

先校验包、版本和逐文件摘要，再绑定路由/handler。产物不齐或版本混用必须显式失败；不能只替换其中一个文件宣布协议更新完成。

### 11.6 当前打包工具

contract_pack.sh 通过 protoc 组装客户端 descriptor.bin + semantic.json，以及服务器 descriptor_full.bin + contract.lua + contract_route.json。manifest 记录生成器、protoc、域指纹和逐文件摘要。打包成功不等于兼容发布成功。

## 12. 开发与 CI

```bash
apollo_gen --contract sdks/contract --out sdks/cpp/generated
apollo_gen --contract sdks/contract --check sdks/cpp/generated
```

XSD 校验、语义测试与 golden 检查约束输入和产物一致性。协议兼容测试、新旧端互操作及滚动发布/回滚需另行验收。文档使用 VitePress locales，各语言维护独立页面。
