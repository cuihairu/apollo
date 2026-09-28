# Apollo 多端 SDK 契约设计（sdk-contract）

> 状态：设计稿（评审中）。参照：KBEngine 的"单独 `sdks/` 目录 + 契约定义协议约定"精髓（学精髓不学形），并修正其短板（对数据的存储太弱——分析见 `docs/design/attribute-sync.md` §8）。本设计与 `attribute-sync.md`（属性模型/预测/attr_schema_hash）、`net-abstraction.md`（帧/通道）、`scripting-lua.md`（脚本白名单）、`ioc-review.md`（装配纪律）互为引用。

---

## 执行摘要

1. **契约文件是唯一事实源**：`sdks/contract/` 的 `attrs.xml` + `messages.xml`（XML + XSD，ioc-review §15.3/§15.4）定义全部属性与消息（字段/类型/可见性/同步组/预测位/通道），**代码生成器**产出服务端访问器 + 各端 SDK 序列化与客户端属性模型 + 文档（生成器内部设计见 `xml-generation.md`）。现状是反的：`modules/protocol` 手写编码、全仓库（含 SDK）无任何 .proto/.def/.toml 契约、客户端手写 `MessageCodec.ts`/`ByteBuffer.ts`——协议每端维护一份、必然漂移。
2. **目录收敛为唯一的 `sdks/`**（拼写错误目录 `skds/` 合并回原名）。事实：`git log 38656f90 refactor: 重命名 sdk 目录为 skds`——一次 rename 引入了 typo，之后新工作（unity cocos laya 的 Network/Session/Messaging 层）落在 `skds/`，而旧 `sdks/` 残留另一套内容（unity 的 Attributes 体系），**两份互斥、互相不通**。结构对齐 KBEngine：`sdks/{contract,gen,unity,ue5,cocos,laya,cpp,docs}`。
3. **学 KBEngine 的位置只有一处：契约驱动多端。** 但三处不学：(a) 不复刻 .def 的裸 XML 解析——**C-50 修正：.def 本就是 XML**（BigWorld entity_description.cpp:184-190、KBEngine entitydef.cpp:188-210，ioc-review §16.3），真正的差异是两家都**只解析、无 schema 层**（错拼标签静默缺省）；apollo 用 XML + XSD 形式校验（ioc-review §15.3），策划可读、diff 友好、校验器零开发；(b) 不做"一份通用序列化给所有端"，生成器按端定制造型（C# 事件、TS 模块、C++ 访问器是其各自惯用法）；(c) **存储不进契约**——KBEngine 的 .def 把属性形态与存储耦合（blob 化复杂类型、base-only 快照，见 attribute-sync §8 五条），契约只管线上协议形态。
4. **客户端 SDK ≠ 序列化层**：四层职责（Codec / Attribute client model / Entity·AOI 管理 / 事件与表现路由），其中第 2 层落地 attribute-sync §9 的预测矩阵（预测值/权威值双缓冲 + 校正），第 3 层落地 enter/dwell/leave 快照增量化应用。现状 `sdks/unity/ApolloSDK/Attributes/` 的 AttributeContainer/AttributeSyncManager 是第 2 层雏形，`skds/` 的 MessageCodec/MessageRouter 是第 1 层雏形——合并后按这四层重组。
5. **兼容由 hash 驱动**：`attr_schema_hash` = 生成器对契约内容的哈希（非手工版本号，防漏改），连接建立即握手校验（attribute-sync §7.2），服务器同时支持 N/N-1 两个 hash；变更纪律：增字段兼容、改语义升版本、删除须双端同周期。
6. **存储自由演化**：持久列/日志重放（attribute-sync §8 的对策）在服务端内部独立演进，不受契约冻结——加持久化字段不动契约，反之亦然。这就是"数据存储不弱"与"协议清晰"同时成立的分界。

---

## 1. 现状与问题（读码结论）

| 事实 | 位置 | 问题 |
|---|---|---|
| 两个 SDK 目录并存、内容互斥 | `sdks/`（仅 unity/ApolloSDK/Attributes/*.cs：AttributeDefinition/Registry/Container/SyncManager/ComputedAttribute）vs `skds/`（README + unity/cocos/laya：ApolloClient/Config、Network、Session、Messaging、Utilities） | 同一次 rename 事故（git 38656f90 `重命名 sdk 目录为 skds` 引入 typo）后两头各自演进；`skds/unity` 没有 Attributes 层、`sdks/unity` 没有 Client/Network 层——任何一个端都拼不齐一套 SDK |
| 协议无单一事实源 | `modules/protocol/src/messages.cpp`、`codec.cpp`（手写）；`skds/*/Messaging/MessageCodec.ts`、`Utilities/ByteBuffer.ts`（每端手写） | 服务端与各客户端各维护一份编码，字段增删改要同步 N 处，漏一处即线上错位；无 .proto/.def/任何契约文件 |
| 属性定义双份 | 服务端 `include/apollo/game/attributes/attribute_id.h`（~300 个遗留 ID）+ 客户端 `AttributeRegistry.cs` | 同一份属性表两处手写，segment 重叠缺陷（attribute-sync §0）直接在客户端再生一遍 |
| 目录名"skds"拼写错误 | `git log` 可证 | 生成器/文档/URL 全要跟着错名称走 |

合并后的目标结构（§3）把这些问题一次性根除：契约出、生成器收，任何端不再手写协议。

## 2. 契约形态（学精髓，不学形）

### 2.1 学的是什么

KBEngine：`.def` 文件声明实体的属性与方法 → 生成器同时产出服务端 C++ 与多端客户端代码——**一端声明、多端实现、编译期/启动期即对齐**。这解决了 MMO 多端协议漂移的全部痛点，是它的精髓，apollo 全盘采纳的只有这一件事。

### 2.2 改了什么

| KBEngine .def | apollo 契约 | 理由 |
|---|---|---|
| 裸 XML 解析、无 schema 层（C-50：.def 本就是 XML，引证见执行摘要 3(a)） | XML + XSD（`attrs.xml` / `messages.xml`，紧凑一行一属性风格，见 §2.3） | 学的是两家同构血统；apollo 增量 = XSD 形式校验层（两家都没有） |
| 属性/方法/存储耦合在一个文件 | 契约只管**线上协议形态**；存储模型在服务端内部（§7） | KBEngine 头号短板（attribute-sync §8） |
| 一份通用客户端代码生成 | 按端定制生成（C#/TS/C++ 各自惯用法） | 端差异不是序列化差异，是事件与对象模型的差异 |
| 无版本/hash | 文件带 `version`，生成器产 `schema_hash` | 兼容策略的数据基础（§6） |

### 2.3 契约文件与紧凑风格（XML + XSD，2026-09-28 同步修订）

```xml
<!-- attrs.xml —— 实体属性（apollo.xsd 校验；对齐 attribute-sync §2 的同步属性收敛） -->
<attrs version="7">
  <attr id="1" name="hp" type="int64" init="1000" scope="BASE" sync="attributes"
        persist="false" predict="client_predicted,server_authoritative"/>
  <!-- type:  int64/固定位宽/int64 万分比/string/string[]/切片容器
       scope: BASE/CELL/RO_MIRROR × SELF/TEAM/GUILD/AOI/WORLD（xs:enumeration，attribute-sync §2.3）
       sync:  通道名 movement/attributes/events/control（net-abstraction §4.1）
       persist/column: 两段式存储的方向性提示（真定义在服务端 storage.xml，§7） -->
</attrs>

<!-- messages.xml —— 消息/意图（方向 + 通道 + 负载） -->
<messages version="7">
  <msg id="10" name="move" dir="C2S" channel="movement">   <!-- intent only，服务端定夺 -->
    <field name="x" type="f32"/><field name="y" type="f32"/>
    <field name="z" type="f32"/><field name="yaw" type="f32"/>
  </msg>
  <msg id="11" name="attr_batch" dir="S2C" channel="attributes">
    <field name="seq" type="u32"/><field name="base_seq" type="u32"/>
    <field name="deltas" type="AttrDelta[]"/>
  </msg>
</messages>

<!-- entities.xml —— 实体清单与继承（ioc-review §16.7.2：单继承、生成期展开；xs:keyref 锁 parent 引用，环检测在生成器） -->
<entities version="7">
  <entity id="Monster"/>
  <entity id="Avatar" parent="Monster"/>
</entities>
```

风格纪律（§15.3）：紧凑一行一属性元素，勿子元素嵌套；`xs:key` 锁 id 唯一（C-35 分段冲突无法入库）、`xs:keyref` 锁引用（派生 DAG/parent/列提升）、`xs:enumeration` 锁域/通道/所有权取值。

## 3. 目录结构与生成器

```
sdks/
├── contract/        # 契约源（唯一事实源）：apollo.xsd、attrs.xml、messages.xml、entities.xml、errors.xml、version（ioc-review §15.4 布局）
├── gen/             # 生成器（C++ 单二进制，入 CI；失败即阻断协议发版）
├── cpp/             # 生成产物 + 手工壳 → 以模块链入 modules/protocol
├── unity/           # U3D 插件（生成 + 手工运行时壳）
├── ue5/             # UE5 插件（规划 P3）
├── cocos/           # Cocos Creator TS（生成 + 手工壳）
├── laya/            # LayaBox TS（与 cocos 共享生成内核）
└── docs/            # 协议文档/变更日志/错误码表（生成器产物）
```

生成器输入=契约文件，输出=5 类产物：① C++ 编码/访问器与校验（服务端 + cpp 端 SDK）② C#（Unity）③ TS（Cocos/Laya，同一模板两处实例化）④ 协议文档 ⑤ `schema_hash` 常量头。**CI 强制**：契约变更必须提交过生成产物（pip 式 diff 检查），防"改了契约忘了生成"。

## 4. 各端 SDK 的四层职责

| 层 | 生成/手工 | 职责 |
|---|---|---|
| 1. Codec | 生成（帧定界模板 + 消息表） | 帧头/CRC/编解码（net-abstraction §3 L1）；服务端与各端同表同序 |
| 2. Attribute client model | 生成 + 手工壳 | 可见属性容器 → dirty 订阅 → **预测/权威双缓冲混合器**（attribute-sync §9 落点：movement 预测 + 校正；HP 插值仅表现层；其余以权威值即时替换） |
| 3. Entity/AOI 管理 | 生成 + 手工壳 | enter-view 全量快照 → dwell 增量应用 → leave 清理（attribute-sync §6）；实体 id → 端上对象映射 |
| 4. 事件与表现路由 | 手工 | 补间/动画状态机/UI 绑定接口；端上专用，契约不约束 |

现状映射：`sdks/unity/ApolloSDK/Attributes/{AttributeContainer,AttributeSyncManager,AttributeEvents,ComputedAttribute}.cs` ≈ 第 2 层雏形（保留其结构修正后并入）；`skds/*/Messaging/{MessageCodec,MessageRouter}.ts`、`Utilities/ByteBuffer.ts` ≈ 第 1 层雏形（重写为生成产物）；其余（AuthManager/HeartbeatManager）保留为手工壳。**四层重组后各端只需维护手工壳层，协议层永远跟随契约。**

## 5. 服务端接入

- `modules/protocol` 的生成产物替换手写 `codec.cpp/messages.cpp`（net-abstraction §1 判定）；`nng_wrapper` 仅进程间路径保留与否见 net-abstraction §6 决策。
- 属性访问器：契约 attr id ↔ 服务端运行时 AttributeDef 从**同一份 L1 静态配置生成**（attribute-sync §2.1 的 L1 以契约文件为源），服务端属性表与客户端属性表在构造上同源，`attribute_id.h` 的遗留手工 ID 表随之退役。
- 握手：连接建立后交换 `schema_hash`；服务器支持 N/N-1 两个 hash（§6）；不匹配拒绝并回含服务端版本的拒绝帧，客户端提示用户升级。
- 上行意图白名单：`scripting-lua.md` §5.2 的脚本可写白名单**由契约生成**（`predict` 位 + 服务端配置交集）——防止脚本层与契约再次分家。
- 服务端自身（模块/工具）反序列化也走同一生成访问器——服务端内部不同模块之间天然协议一致。

## 6. 版本与兼容策略

```
契约提交 → CI 生成 → schema_hash = SHA-256(契约源+生成器版本) → 各端 SDK 发布
线上服务器：server_supported = {hash_N, hash_{N-1}}
```

- **hash 而非手工版本号**：漏改版本号 → 哈希自动变，握手直接暴露；version 字段仅给人读。
- 变更纪律（写进生成器 README）：增字段 = 向后兼容（旧端忽略缺省，服务端补缺省值）；改类型/语义 = 升契约版本（新旧 hash 并存一周期，灰度完成后再移除旧 hash）；删除字段 = 必须双端同周期，且需要一版"删除前的公告版本"。
- 与 attribute-sync §3 的切换联动：快照带 `attr_schema_version`（scripting-lua §3.3）——同一场景内新旧版本实体并存期间，客户端按实体上的版本号选解码表。

## 7. 与存储的解耦（为什么契约不冻结存储）

KBEngine 的结构性弱点（attribute-sync §8，要点）：.def 属性形态与持久化耦合 → 复杂类型被 blob 化、持久化只会 base 快照、无日志、无查询面。对策在这里定型为一条原则：

> 契约回答"线上怎么传"，存储回答"服务端怎么落"——两者只在 `persist` 给一个方向性提示，永不绑定。派生属性/持久列/日志重放全部是服务端内部演化对象。

具体：新增"需要存档的属性"只改服务端持久层（column promotion，attribute-sync §8.2），契约零改动——因为客户端不需要知道服务端存哪张表；同理契约加字段（如 UI 表现提示）不动存储。存储定义的具体形态 = 服务端私有 `storage.xml`（表/列提升/journal 语句，"语句即数据"的 MyBatis 形态，xml-generation.md §6；不在契约目录、不进 schema_hash——ioc-review §15.4/§15.5 两段式）。**协议与存储的演化周期互相解耦，是"学精髓"的最后一环。**

## 8. 分期

- **P1**：目录收敛（`skds/` 内容并入 `sdks/`，删空壳保留正确拼写；git mv 记录）；契约 v1（`apollo.xsd` + attrs/messages/entities/errors.xml，§15.4 布局）；生成器 v1（C++ + 文档 + hash，内部设计见 `xml-generation.md`）；Unity 端按四层重组（Codec 生成 + Attributes 层并入生成产物）。
- **P2**：Cocos/Laya TS 生成；第 3 层（AOI 实体管理）生成壳；预测混合器 v1（movement 双缓冲）。
- **P3**：UE5 插件；跨端事件协议评审；契约变更流水线接 CI 发版。

## 9. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| attribute-sync.md | 契约 §2.3 即其 §2 属性模型/§9 预测矩阵的机器可读形态；`attr_schema_hash`/`attr_schema_version` 握手与切换；快照/增量消息即 `attr_batch` |
| net-abstraction.md | 帧格式与四通道写进契约（各端插件据此实现帧定界与通道映射）；`modules/protocol` 手写代码退役 |
| scripting-lua.md | 脚本白名单由契约生成（§5）；合同版本随实体下发（scripting-lua §3.3） |
| ssengine-reference.md | 弱存储教训的出处（attribute-sync §8）；sdpkg 帧头方向对齐 net-abstraction §3 |
| ioc-review.md | 无关联容器语义；作为"配置只有一份"纪律在跨端维度上的延伸；§15.3/15.4/15.5 契约决策源、C-50 .def 修正（§16.3） |
| xml-generation.md | gen/ 生成器的内部设计（pugixml 解析、校验四层漏斗、产物五类、MyBatis 映射）；本设计的 gen/ 即其实现载体 |

---

*基线：apollo main @ 35a9c528（`sdks/`、`skds/` 读码，git log 38656f90 目录改名记录）；KBEngine 参照其公开文档的 .def/生成器/SDK 结构（非源码评审）。2026-09-28 同步修订（①⑤）：契约形态 TOML → XML+XSD（ioc-review §15.3/§15.4）、.def 表述按 C-50 修正（改以 ioc-review §16 源码证据为准：BigWorld entity_description.cpp:184-190、KBEngine entitydef.cpp:188-210，两工作副本为浅克隆/官方包）；行号基线仍为 35a9c528。*