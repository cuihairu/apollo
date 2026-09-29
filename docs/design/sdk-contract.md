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
- **P2**：Cocos/Laya TS 生成；第 3 层（AOI 实体管理）生成壳；预测混合器 v1（movement 双缓冲）；**proto/bin 双后端**（§10：契约 → .proto golden → protoc 同批出 `.pb.cc`（服务端热路径，A 路线）+ `descriptor.bin`（默认通道：客户端热更资源管线/冷路径/工具，C 路线）——A+C 双轨定案见 §10.6，两后端同批实现）。
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

## 10. 契约 → protobuf 编码：两层模型与 proto 后端（2026-09-29 追加）

> 定位：README/36 号文档把「def 管语义、protobuf 管字节」一句话带过（决策 #3），本节写透——两层模型、def→proto 映射规则、生成器 proto 后端、与差分同步的配合、zstd 边界，以及「编译期生成 / descriptor 反射池 / 运行时解释」三路线的正面权衡（§10.6）。分期落 P2（§8 已补行）；第一批契约（`sdks/contract` v1 @ a5334014）尚无 proto 产物，本节是其后端设计定稿。引用基线：attribute-sync §3/§5/§7、net-abstraction §3/§5.5、ioc-review §16.7.2、docs/05 §6.1/§6.3。

### 10.0 为什么 def 契约不排斥 protobuf——反而依赖它

先分清三个常被混为一谈的概念：

| 概念 | 本质 | 回答的问题 | 例子 |
|---|---|---|---|
| def 契约（attrs/messages/entities.xml + XSD） | **语义层**声明 | 该不该发、发给谁、何时发、谁能写、值是什么含义 | `hp`：int64，PROP 可见，默认 1000 |
| protobuf | **wire 层**编码格式 | 值怎么变成字节 | varint 消前导零、zigzag 消符号跳变、缺省零字节 |
| zstd | 帧级**传输压缩**（可选第二层） | 字节流还能不能更短 | 跨条目重复模式（attr_id 重复、结构相似） |

论证链（五条）：

1. **语义不可从字节推导**。varint 流里没有「TEAM 可见」「client_predicted」「继承自 Monster」——这些信息必须有一份机器可读声明，即契约。反过来，protobuf 也不产生语义：`AttrDelta` 的 attr_id 指向谁、接收端能不能信，全由契约定义。
2. **字节不该手写两遍**。§1 的现状正是反例：服务端手写 codec + 各端手写 ByteBuffer，协议漂移即错位。契约→生成→各端一致是学 KBE 的精髓本体（§2.1）；protobuf 只是把「生成什么」落到一个有成熟工具链的编码目标上，替代的是手写编码，不是替代契约。
3. **一份事实源、两端产物**。契约 → apollo_gen → `.proto`（golden）→ protoc → C#/TS/C++ 序列化层——契约变更单点扩散，多端在编译期对齐，与 §3 的 CI diff 闸同一条纪律。
4. **hash 单点对齐**。`schema_hash` 管语义兼容（握手，§6）；protoc 产物按构建对齐（CI pin 版本）。两层各自单调，不互相发明版本机制。
5. **生态免费拿**。varint/zigzag/packed 的正确实现 + 各端成熟运行时由 protoc 供给（protobuf 库已是 vcpkg manifest 依赖）——自研 codec 得不到「错一处全网搜得到答案」的运维红利。

常见误读修正：

| 误读 | 修正 |
|---|---|
| 「契约和 protobuf 二选一」 | 契约是源，proto 是产物之一（与 C++ 头/JSON/文档并列，§3 五类产物） |
| 「protobuf 就是压缩」 | varint/zigzag 是**紧凑编码**（消字段内冗余：前导零、符号跳变），不感知跨字段/跨条目重复；重复模式归 zstd |
| 「有 zstd 还要 protobuf 干嘛」 | zstd 压不了「解码」——客户端要的是强类型字段流，不是字节串。protobuf 给结构，zstd 给传输字节量，正交叠加 |
| 「sync 掩码/predict 要编进 wire」 | 都不进（§10.2.4/§10.2.6）——wire 只承载「发了什么值」，不承载「为什么发」 |

两层模型：

```
┌─────────────────────────────────────────────────────────────────┐
│  语义层（def 契约 = 唯一事实源）                                   │
│  attrs.xml    属性：id/name/type/sync 掩码/predict/default        │
│  messages.xml 消息：id/dir/channel/fields                        │
│  entities.xml 实体：单继承（生成期拍平，ioc-review §16.7.2）       │
│  errors.xml   错误码分区                                          │
│  —— 决定：谁能看（掩码）/谁能写（predict）/何时发（IMMEDIATE/节流）  │
└──────────────┬──────────────────────────────────────────────────┘
               │ apollo_gen（四层漏斗：XSD→解析→static_assert→启动，xml-generation §5）
               ▼
┌─────────────────────────────────────────────────────────────────┐
│  wire 层（产物，golden 入库）                                      │
│  apollo_contract.h/.json（已交付） + apollo_contract.proto（P2）   │
│  —— 决定：值怎么编码（varint/zigzag/fixed/len/packed）             │
└──────────────┬──────────────────────────────────────────────────┘
               │ protobuf 序列化（protoc 各端运行时）
               ▼
┌─────────────────────────────────────────────────────────────────┐
│  帧层（net-abstraction §3/§5.5）                                   │
│  FrameFilter 链：zstd(>256B) → [加密 P3] → L1 16B 帧头+CRC32C      │
│  —— 决定：字节流怎么上传输（压缩/校验/定界），与 payload 内容无关     │
└─────────────────────────────────────────────────────────────────┘
```

### 10.1 protobuf 的「紧凑」边界——紧凑编码 ≠ 通用压缩

编码机制要点（映射规则的地基）：

- **tag 交织**：每个字段 = `tag | value`，`tag = (field_number << 3) | wire_type`——字段间天然按出现顺序交织，没有对齐填充。
- **varint**：每 7bit 一组、高位续位——`uint32 v=1` 编码 1B；前导零被吃掉。
- **zigzag**：`sint32/sint64` 先 `(n << 1) ^ (n >> 31/63)` 映射到非负再 varint——`-1` 与 `1` 都是 1B，消灭符号位跳变导致的 5B/10B 惨案。
- **缺省零字节（proto3）**：等于默认值的字段**不编码**——「字段级 optional」语义免费获得（不出现 = 缺省/未变更，这正是差分需要的形态，§10.4）。
- **packed repeated**：数值型 repeated 连续存一次 tag——同容器多条目摊薄头部。

**澄清（README 同口径）**：以上全部是**字段内**紧凑化。protobuf 不知道跨字段、跨条目、跨帧的重复模式——100 条 `AttrDelta` 里 attr_id=1 出现 100 次，protobuf 逐条编码不去重；这个层面的冗余归 zstd（§10.5）。所以「protobuf+zstd」不是重复投资，是两层正交：前者管结构熵，后者管重复熵。

### 10.2 def → proto schema 映射规则

#### 10.2.1 双路径：差分（通用对）与强类型（逐属性 field number）

**判定：差分热路径走 `(attr_id, value)` 通用对（`AttrDelta`），不按属性生成 field；强类型投影（访问器/SDK 容器/调试工具）才逐属性编 field，且 field number ≡ attr id。**

两条路径对照（以 `hp` 变更为例）：

```
差分路径（热路径，attribute-sync §7.1 已定型，本节钉死「由契约生成」）：
  message AttrDelta { uint32 attr_id=1; sint64 value=2; uint32 change_n=3; }
  → 编码：tag(1B) attr_id=1(1B) tag(1B) value(1-3B zigzag)…
  → schema 固定 3 字段，不随属性数膨胀；attr_id 取值域=契约全集（生成枚举校验）

强类型路径（各端容器/服务端访问器/P3 调试 proto）：
  field number = attr id（1:1）
  → hp 的 field number 恒为 1；加属性=新 id=新 field，天然无冲突
  → 消除「契约 id ↔ proto field」间接映射表——错位类缺陷从结构上不存在
```

为何差分不采用「全属性单 message + 每属性一 field、只 set 变化字段」（per-field optional 方案）：

| 对比项 | per-field optional message | AttrDelta 通用对（采纳） |
|---|---|---|
| schema 尺寸 | 500 属性 = 500 field 的巨型 message | 固定 3 field |
| 接收端 | 生成 N 分支 switch / 反射式遍历 | 表驱动通用循环：id→容器 set |
| 单属性变更帧 | tag+value ≈ 3-10B | ≈ 4-12B（多 1-2B）——热路径每帧多条时被 packed/合并摊平 |
| change_n 诊断位 | 无处安放 | 天然第 3 字段 |
| 快照/delta 同构 | 两套形态 | 同一消息（snapshot 置位，attribute-sync §7.1） |

编码税可忽略、换来 schema 稳定与通用解码——**差分路径采纳通用对**。

#### 10.2.2 类型映射表

| def 类型（alias 先展开为基元） | proto 类型 | wire | 说明 |
|---|---|---|---|
| bool | bool | varint | 1B |
| int32 | sint32 | varint(zigzag) | 可为负的数值语义 |
| int64 | sint64 | varint(zigzag) | 与 AttrDelta.value 值桶统一 |
| float | float | fixed32 | 坐标/朝向；protobuf 不做浮点 varint，恒 4B |
| double | double | fixed64 | 恒 8B |
| string | string | LEN | |
| bytes | bytes | LEN | |
| string[] | repeated string | LEN×N | |
| int64[] | repeated sint64 | LEN(packed) | 连续 varint 流 |
| alias（percent/timestamp…） | **零特判**，按 underlying 基元映射 | — | 万分比恒非负仍走 sint64——zigzag 对非负值多花 1bit 是**编码税**，换取「alias 不产生第二种值桶形态」；特判反而破坏 AttrDelta 单一 sint64 值桶 |
| 嵌套 object（P2+ 按需） | nested message | LEN | msg field type 引用另一 msg name → message 引用；第一批契约无此形态 |

#### 10.2.3 attr id 分段与 message 拆分的取舍

现状分段（docs/05 §6.1，attrs.xml 注释同源）：1-99 基础 / 100-199 战斗 / 300-399 状态 / 600-699 外观 / 700-799 社交 / 800-899 标记。三个候选方案：

| 方案 | 形态 | 判定 |
|---|---|---|
| A 全属性单 message（逐属性 field，§10.2.1 强类型路径） | 生成一个 AttrSet | **P3 可选产物**（编辑器/调试工具 golden），非运行时通路 |
| B 同段一 message（AttrsBasic_1_99 / AttrsCombat_100_199…） | 按段分组投影 | **否决**——把「段」编进 wire 词汇：docs/05 分段表调整（语义组织演化）会强制 schema 变更；段是给策划/评审的组织，不是编码组织 |
| C 分段完全不进 wire（差分/快照都走通用对） | 段只是契约注释 + 生成枚举的分区 | **主路径采纳**——wire 与分段表解耦，调段零 wire 变更 |

#### 10.2.4 sync 掩码：不进 wire、不拆 message、不用 optional 表达可见性

掩码 7 位（contract_model.hpp:19-27）：APPR 0x01 / PROP 0x02 / SELF 0x08 / TEAM 0x10 / GUILD 0x20 / WORLD 0x40 / IMMEDIATE 0x80（DB 0x04 契约禁用，归 storage.xml——决策 #5）。**判定：掩码不产生任何 proto schema 元素。**它的三个消费点全在语义侧：

1. **收集期域裁剪**（attribute-sync §3.2/§4.2）：per-viewer 可见集 = 掩码 ∩ viewer 关系——决定哪些 attr_id **进**本帧 delta；
2. **生成访问器断言**（SDK 层）：可见性/写权限编译期检查；
3. **文档/校验**（§3 产物④）。

不拆档位 message / 不用 per-field optional 表达可见性的理由：(a) 同一广播 tick 内 SELF+TEAM+AOI 属性齐变是常态，拆档位 = 发多个 message；(b) 接收端出现两套应用路径；(c) 掩码演化（加档位）会改 schema——违反「wire 只承载发了什么值」；(d) 域裁剪后的 (id,value) 集合**已经**是档位的结果，「不出现 = 不可见或未变更」是 delta 流的天然语义。

```
同一实体，三个 viewer，同一 tick（hp=PROP，level=PROP TEAM WORLD，gold=SELF）：
  viewer=self   delta = {hp, level, gold}      ← SELF∪TEAM∪WORLD∪AOI 全可见
  viewer=队友    delta = {hp, level}            ← TEAM 关系成立，gold 被裁掉
  viewer=路人    delta = {hp, level}            ← WORLD 档可见；若 hp 改为 SELF 档则只剩 level
  —— 三份 AttrBatch 内容不同、编码器同一；客户端无掩码概念，收到即权威
```

#### 10.2.5 继承：拍平进投影，不进 proto（对齐 §16.7.2）

entities.xml 单继承链 `Monster ← Avatar ← Player` 在生成期展开（父先入、子覆盖、禁同名改型、带 provenance——ioc-review §16.7.2），**三个消费端只见扁平结果**（attribute-sync §7.3）。proto 层因此**没有继承概念**：属性流里实体只以 `entity_id + template_id` 出现，客户端按模板知道属性集。两个候选均否决：

| 候选 | 形态 | 判定 |
|---|---|---|
| 拍平单 message per entity（PlayerMessage 含全部继承+自有字段） | 每实体一个强类型 message | **仅 P3 工具链可选**（同 §10.2.3 A）；运行时属性通路不用 |
| 嵌套复用（`Player { Monster base = 1; Avatar ext = 2; … }`） | proto 嵌套表达继承 | **否决**——解码语义依赖嵌套存在性/字段遮蔽规则，跨端生成复杂，且与「生成期拍平、投影只见扁平」直接冲突（§16.7.2：展开逻辑不进任何运行期模块） |

#### 10.2.6 predict 权限位：校验层语义，零 wire 字节

`predict`（none / client_predicted / server_authoritative）决定：①Lua 可写白名单（§5 → scripting-lua §5.2）②客户端预测混合器行为（attribute-sync §9：哪些属性可本地先行、哪些纯权威）③服务端校验强度。wire 上**没有任何「这是可预测属性」的标记**——预测是端上本地行为，由生成常量表（PredictableAttrs）驱动；「能否预测」是静态语义，不随帧变化，不占字节。

### 10.3 生成器流水线：apollo_gen 的 proto 后端

```
 sdks/contract/*.xml + apollo.xsd
        │
        ▼
 ┌──────────────────────────── apollo_gen（独立二进制，决策 #4：不进运行期链接图）
 │  解析 → validateAndResolve → 规范序（contract_writer 内排序）
 ├──────────────────────────────────────────────────────────────
 │  产物①  C++ 头（已交付：sdks/cpp/generated/apollo_contract.h）
 │  产物②  JSON（已交付）
 │  产物③  协议文档/错误码表（§3 产物④）
 │  产物④  schema_hash 常量（各端同值）
 │  产物⑤  apollo_contract.proto（P2 新增：固定消息族 + MsgId enum + 文件头带 hash）
 └──────┬───────────────────────────────────────────────────────
        ▼  checked-in golden（与 .h/.json 同纪律：CI byte-diff 闸，apollo_gen_golden_check 同型）
 apollo_contract.proto
        │
        ▼  protoc（版本由 vcpkg manifest 锁定；CI 单点跑）
 ┌────────────┬────────────┬────────────┐
 │ C++ (cpp)  │ C# (unity) │ TS (cocos/ │   ← 各端序列化层；与四层职责（§4）的第 1 层 Codec 合并
 │            │            │    laya)   │
 └────────────┴────────────┴────────────┘
```

- **proto 是 golden 产物不是构建期即时生成**：契约变更必须连 `.proto` 一起提交（CI diff 阻断「改契约忘生成」）；开发者环境不需要装 protoc——只有发版 CI 需要。
- **语法 proto3**：缺省零字节即字段级 optional（§10.1）；文件头注释带 schema_hash + 生成器版本 + DO NOT EDIT；field number 预留段 19000-19999（protobuf 内部保留）在注释中声明禁用——契约分段上限 899，无交集。
- **schema_hash 覆盖 proto 层**：hash = SHA-256(canonicalBundle + generatorVersion)。proto 产物由同一 canonical bundle 决定 ⇒ **语义变化必然改变 proto golden ⇒ hash 必然变**，不存在「proto 变了 hash 没变」的缝隙；proto 后端引入本身也升 generatorVersion ⇒ hash 变 ⇒ 握手按 §6 双版本窗口（N/N-1）灰度。protoc 产物对齐不靠 hash、靠 CI pin（vcpkg manifest 锁 protobuf 版本，各端序列化层同一次 protoc 跑出）。
- **各端 hash 常量四处同值**：C++ `kSchemaHash` / C# `SchemaHash` / TS `SCHEMA_HASH` / proto 文件头注释——单点生成，禁止手抄。

### 10.4 差分同步与 protobuf 的配合

全链（服务端每广播 tick，attribute-sync §3.2/§10 六阶段中的 4-5 阶段）：

```
 owning 场景线程（单写者）
 ┌───────────────────────────────────────────────────────────────┐
 │ history ring[(seq, attr_id, value)]  ←─ simulate/recalc 写入    │
 └──────┬────────────────────────────────────────────────────────┘
        ▼ collect（无锁读 history）
 per-viewer：delta = history[(acked_seq, world_seq]] 按掩码裁剪
        │   ├─ IMMEDIATE(0x80) 位 → 跳批量当 tick 发（attribute-sync §5.3 INSTANT 同型）
        │   ├─ min_interval/优先级 → 档位节流（掩码决定「进不进」，
        │   │                        protobuf 只编码「进来的长什么样」）
        │   └─ 预算/水位 → attribute-sync §5 × net-abstraction §4.2
        ▼ 组包
 AttrBatch{entity_id, base_seq, repeated AttrDelta deltas, snapshot?}
 AttrSyncFrame{repeated batches, frame_ms}   ← 每客户端每 tick 至多一帧
        ▼ protobuf 编码（§10.2 映射）
 payload ──► zstd filter(>256B) ──► L1 帧头(flags) ──► 传输
        ▼ 客户端逆序
 解帧 → 解压 → 解码 → 表驱动应用：attr_id → 容器 set（SDK 第 2 层，
        │   预测混合器按 §10.2.6 的 PredictableAttrs 表行为）
        ▼
 AttrAck{entity_id, seq} 捎带任意上行 ──► 推进 ViewerState.acked_seq
 （history 覆盖不了对方进度 → 快照重置自愈，attribute-sync §3.3）
```

- **差分粒度 = 字段级 optional**：每条 AttrDelta「出现即变更」、不出现即未变更——proto3 缺省零字节使这层语义零成本（§10.1）；帧元数据（frame_ms 等）同理。
- **per-viewer 独立 batch**（attribute-sync §4.2 的代价）：同一编码器复用，只是 (id,value) 集合不同——protobuf 对此无感知，正确性由收集侧闭环。

### 10.5 zstd 的边界：FrameFilter 链位置、阈值、叠加收益

位置（net-abstraction §5.5 的 FrameFilter 插件位，filter 只做字节↔字节变换）：

```
 [业务对象 AttrSyncFrame]
      │ protobuf 序列化（结构熵：varint/zigzag/packed/缺省零字节）
      ▼
 [payload bytes]
      │ zstd filter —— 仅当 payload > 256B（重复熵：跨条目模式）
      ▼
 [压缩 payload]
      │ 加密 filter（P3 可选，位次在压缩之后：压密文无收益）
      ▼
 [L1 帧：16B 头(magic|len|seq|ch/flags|crc32c) + payload]
      └─ flags 位标记「本帧已压缩」——filter 栈由契约声明（帧头 ver 字段 +
         messages.xml 通道声明 → 生成器产出装配代码，net-abstraction §5.5）
```

- **压缩标记收敛到帧头 flags**（唯一权威）：filter 栈自描述，接收端按帧头逆序解包，与「栈顺序契约锁死」一致。attribute-sync §7.1 的 `AttrSyncFrame.zstd` bool 标注为过渡兼容，P2 收敛删除——两层重复标记是漂移源。
- **>256B 阈值依据**：zstd 最小帧开销 ≈ 9-13B（magic 4B + 帧头 1-2B + block header 3B + 可选 checksum 4B）+ 单次调用 CPU；movement 帧（xyz+yaw 四 float + 头）常态 <100B，压缩必负收益；attr 多实体批量、快照常态超阈。阈值 per-channel 可配，P1 落地时基准化（与 attribute-sync §11 度量一并）。
- **叠加收益判断标准**（什么时候 protobuf 之后还值得压）：

| payload 形态 | protobuf 消不掉的冗余 | zstd 期望收益 |
|---|---|---|
| 单实体 1-3 条 delta（10-30B） | 无跨条目模式 | **负收益，不压** |
| 多实体批量（>256B） | attr_id 跨条目重复、实体间 batch 结构相似、数值高字节零 | **高，压** |
| 全量快照 | 字段连续 + 默认值密集 | 中高，压 |
| movement 帧 | 恒小 | 永不压 |

  判定式：protobuf 消**字段内**冗余（前导零/符号跳变），zstd 消**跨条目**冗余（重复模式）——两者正交，只有后者存在时压缩才有增益。跨帧最强的重复模式是 per-viewer delta 帧结构相似——**zstd dictionary 训练留 P3 评估**，不进首版。

### 10.6 编译期生成 vs 运行时解释：正面回答路线权衡

这是 BigWorld 与 KBEngine 的**真实分叉**，也是对「生成路线」最常见的质疑（「改契约要重编」）。两家实测形态：

- **BigWorld**：.def → 生成 C++ 代码（entity_description 解析后进编译产物）——热路径零开销、强类型；代价是契约变更走重编（doc 36 §2 :200：演进 = 改文件重启）。
- **KBEngine**：.def 运行时解析 + Python 对象模型承载（entitydef.cpp:188-210，C-50）——理论上热加载；代价是热路径查表 + Variant 通用类型，且其存储面的孪生缺陷（复杂类型 blob 化、契约/存储耦合五条短板，attribute-sync §8、决策 #5）与运行时解释同族：**都是「用运行期通用性换编译期确定性」**。注意：KBE 的「热加载」在两家实践中都未兑现为不停机演进（doc 36 :200：改文件重启、无版本握手）——运行时解释换来的热加载红利实际没拿到。

**第三条路线 C：descriptor 反射池（曾实践过的技术，正面评估）。**构建期 apollo_gen 吐 `FileDescriptorSet` 二进制（`descriptor.bin`——protoc `--descriptor_set_out` 的标准产物，**数据非代码**），运行时启动加载进 `DescriptorPool` + `DynamicMessageFactory`，按 descriptor 动态构造与编解码消息。先划清与 B 的本质区别：C **不是**「运行时解释的温和版」——.bin 是构建期由 protoc 校验并定型的规范数据，「生成什么」在构建期已定死，运行期没有第二次解释源文件的机会；加载期只做反序列化重建，descriptor 非法/字段引用悬垂即启动报错，不会静默缺省。它仍属「构建期定型」家族，只是**端侧消费形态从代码变成数据**。边界同样明确：descriptor 只描述 wire 层——掩码/predict/分段等语义层不进 .bin，语义消费端（Lua 白名单、预测混合器）继续走既有 JSON golden（§3 产物②）与生成常量表，两层产物恰好对齐 §10.0 的两层模型。

三路线对比（A 纯编译期生成 / C descriptor.bin 反射池 / B 运行时解析 def）：

| 维度 | A 纯编译期生成（BW 路线） | C descriptor.bin 反射池 | B 运行时解析 def（KBE 路线） |
|---|---|---|---|
| 契约加载形态 | apollo_gen 离线产出 C++/C#/TS/.proto，进构建 | apollo_gen 构建期吐**数据文件**（.bin），端侧启动加载进 DescriptorPool | 启动时解析裸 XML 建运行时属性表 |
| 契约变更成本 | 重编 server 二进制（增量分钟级）+ 各端 SDK 重发 | **端侧换一个 .bin 文件即完成跟进**——宿主程序不重编 | 换文件重启（实践中与 BW 相同的停机窗口，另加运行期错位风险） |
| 热路径开销 | 生成代码直接字段访问，零查表零分派（基准 1×） | 反射编解码比原生**慢 2-5×**（逐字段走 Reflection 接口）；prototype clone 可摊薄描述符查找，仍慢于 A | 逐属性查表 + Variant 类型分派（每 tick 百万级属性访问的乘数） |
| 校验时机 | 四层漏斗前置：XSD→解析→static_assert→启动（xml-generation §5） | **加载期即报错**——descriptor 非法 DescriptorPool 构建失败，启动即红 | 运行期才见（错拼静默缺省是 BW/KBE 共同缺陷，决策 #3） |
| 多端支持 | 每端一套代码生成后端（C#/TS/C++ 模板各自维护） | **同一份 .bin 任何带 protobuf 运行时/绑定栈的端可读**——JS/C#/Lua 绑定通吃，零 per-端生成 | 各端各写一份 XML 解释器，漂移面 N 倍 |
| 生成器复杂度 | 吐代码：模板/语法/惯用法 per 端维护 | **吐数据比吐代码简单一个量级**——生成 .proto 后 `protoc --descriptor_set_out` 即得，protoc 替生成器干活 | 无生成器（成本转嫁给每个端的解释器） |
| 类型形态 | 强类型（struct/访问器，错用即编译错） | 弱类型（DynamicMessage 按名/号取字段，错用运行期才见） | 通用 Variant |
| 失败面 | CI/编译即红，带病上不了线（决策 #4） | 加载期红；语义层校验不进 descriptor，仍靠生成侧闸兜住 | 启动晚失败 / 运行期数据错位 |

**分档用法**（C 不做全量替换，按路径冷热分档；定案口径见本节末）：

- **热路径**（服务端 tick）：走 A——生成代码零开销；若某热路径不得已走 C，用 prototype clone（启动期 `factory.GetPrototype()` 缓存原型、消息构造走 `prototype->New()`）摊掉描述符查找，但仍要按 2-5× 编解码税做预算。
- **冷路径与客户端**（GM 后台、调试器、内部工具、离线分析；客户端 SDK 全量）：纯反射够用——客户端解码的是**自己的**一条视图流（非服务端百万级实体遍历），2-5× 在端侧预算内；换来**零生成代码、零重编**的跟进速度。

**schema_hash 直接对 .bin 计算的多端一致性优势**：A 路线下 hash = SHA-256(canonicalBundle + generatorVersion)，canonical bundle 是 apollo_gen 的内部概念——各端只能「拿到常量、信它」，无法独立重算验证；C 路线下 hash 可**直接锚定 descriptor.bin 字节**（protoc 序列化确定性保证同输入同字节）——「端拿到的 wire 描述」与「握手用的 hash」同源，任何端加载 .bin 后可自行重算校验（自验证），多端一致性从「信任生成器」升级为「字节可验」。落地形态：.bin 与 .proto 同为 checked-in golden（同纪律），hash 常量与 .bin 同批产出（§10.3 产物④/⑤ 并列）。

**C 路线的热更适配（各运行时动态加载 API，查证）**——descriptor.bin 走各运行时标准动态加载入口，无任何私有格式：

| 运行时 | 动态加载入口 |
|---|---|
| C++ | `FileDescriptorSet::ParseFromString` + `SimpleDescriptorDatabase` 喂 `DescriptorPool` |
| C#/Unity | `Google.Protobuf.Reflection`（descriptor 反射面，`FileDescriptor` 从字节串构建） |
| Lua | lua-protobuf / pbc 的 `pb.load`（国内客户端热更标配） |
| JS/TS | protobuf.js `Root`（加载 descriptor 后全量反射可用） |
| Python | `descriptor_pool.AddSerializedFile`（运维脚本/离线工具直接吃 bin） |

工程形态：**descriptor.bin 按普通资源文件进热更管线**——Unity AssetBundle / 小游戏分包 / CDN 版本目录，换 bin 不重编引擎、不走商店重发（资源更新通道）；字段号兼容（消解阀①）保证新旧 bin 与新旧服务端在 N/N-1 窗口内共存。两个工程注意点：

- **① C++ `DescriptorPool` 描述符不可卸载**——热重载的正确形态是**换代**：新版本 bin 建新 pool，旧 pool 等存量消息生命周期结束后整体废弃；不做原地增删 descriptor（未定义行为）。
- **② 反射比原生慢 2-5×**——分档不变：热路径编译期生成（A），冷路径/客户端/工具走 bin（C）。

**既有决策链站的是「构建期定型」，不是「A 独占」**（非本节新立）：决策 #4 生成器独立二进制、不进运行期链接图、失败阻断发版；ioc-review §0 删 Spring 式运行时容器——运行时解释契约正是「运行时容器」在数据面的镜像；契约第一批已落 static_assert 编译期闸（modules/contract/tests/gen_compile_test.cpp 第③层）；决策 #11 的对照注记（UE 的 DOREPLIFETIME 是 C++ 宏、改协议必重编——apollo 把掩码声明从代码移进契约数据，改掩码 = 改 XML + 重新生成，不改手写代码；「运行时可配」指声明位置数据化，非运行期解释）。A 与 C 同属「构建期定型」家族——.bin 由 apollo_gen 构建期吐出、protoc 校验、运行期只读（加载后不解释源文件、不改结构），与 ioc-review §0「启动期定型、运行期只读」同一纪律，不落入「运行时容器」的批判面；A/B/C 三路线的分轨结论见本节末定案。

对「改契约要重编」的三点消解：

**① proto 字段号的前向/后向兼容——演进 ≠ 全端同步重编。**field number ≡ attr id（§10.2.1）加新属性 = 新 field number，**旧端对未知字段按 protobuf 规则跳过（unknown-field 保留），不破坏解码**；新端读旧端缺字段走默认值。配合 hash 握手的 N/N-1 双版本窗口（§6），「加字段」类演进只要求服务端先升级，客户端随版本节奏跟上——不存在「改一个属性全端停机重编」的耦合。真正需要全端同周期的只有删字段/改语义（§6 变更纪律），那本来就该全端过一遍。

**② 重编译的真实成本被高估。**重编的是 **server 二进制与各端 SDK 产物，不是引擎**——apollo 模块化布局下契约产物只链 `modules/protocol` 与 `sdks/*`，增量编译分钟级。且 MMO 的**结构层**契约变更（加属性段、调分段）天然伴随 storage.xml 迁移、灰度、公告、维护窗口——这是变更流程的固有部分，「重编」搭车完成，不是额外成本。真正高频的变更（数值、文案、活动参数）走 ConfigRegistry 与 Lua（决策 #12），根本不碰契约。

**③ 分层设计给高频变更留了门——结构层生成、业务层动态。**

```
 结构层（低频稳定）：attrs.xml 1-899 语义分段 —— 生成 C++/C#/TS，零开销，强类型
 业务层（高频变更）：两条不重编通道
   (a) 通用容器字段：契约预留 map<uint32, bytes> 型 attr（如 ext_data）——
       内容键值由 Lua/配置解释，wire 层只是一个 bytes，proto 零感知
   (b) configId 引用：业务内容表驱动（技能/道具/任务全在 DB 表，doc 36 §2
       Mangos 先例 :142）——契约只传 id，内容演化零 wire 变更
 结论：改业务不重编由 (a)+(b)+Lua 白名单（§5）承接；
       「改协议结构要重编」从缺陷变成质量闸（编译期把错挡在上线前）。
```

**定案（2026-09-29）：编解码路线 = A+C 双轨、C 为主。**

- **A 编译期生成——收窄保留**：只用于**服务端 tick 热路径**（帧预算内零开销的核心消息：attr delta/movement/控制面；低频变更，重编成本由维护窗口吸收，消解阀②）。
- **C descriptor.bin——默认通道**：客户端 SDK（Unity/Cocos/Laya 全量走 bin，经热更资源管线随版本目录下发，**不随服务器发版**）、服务端冷路径（GM/运维/离线工具）、调试工具。
- **生成器形态**：apollo_gen 同一 def 源**同批吐 `.pb.cc/.pb.h` 与 `descriptor.bin`**（§10.3 产物⑤ 扩为 .proto + .bin 双输出）——schema_hash 两态一致：常量仍按 §6 算法对 canonicalBundle 计算（两产物同源同批），且因 protoc 版本 pin（§10.3）与 .bin 字节锚定等价，端侧可对 .bin 重算自验。
- **定案理由**：客户端更新链路（商店审核/玩家升级）比服务器重编译**更难控**——bin 当资源下发，把演进链路里最慢的一环（端侧发版）与协议解耦；服务端重编可控（消解阀②），端侧发版不可控。
- **批次影响**：生成器 proto 后端与 bin 后端**同批实现**（§8 P2 行已同步扩写）；B 路线维持否决。

### 10.7 收束

def 契约与 protobuf 的关系一句话：**契约是源，proto 是它最重要的投影之一；语义层（掩码/predict/继承/分段）不进 wire，wire 层（varint/zigzag/缺省零字节）不生语义；zstd 在帧层消 protobuf 消不掉的跨条目冗余**。多端一致由「单 golden + schema_hash + protoc pin」三点闭环；路线定案为 **A+C 双轨、C 为主**（§10.6：服务端热路径生成代码零开销，客户端/冷路径/工具走 descriptor.bin 资源管线——端侧发版与协议演进解耦）；业务动态性由通用容器字段 + configId + Lua 三扇门承接——协议结构层的稳定不再是对演进速度的牺牲，而是对「带病上线」的免疫。

---

*基线：apollo main @ 35a9c528（`sdks/`、`skds/` 读码，git log 38656f90 目录改名记录）；KBEngine 参照其公开文档的 .def/生成器/SDK 结构（非源码评审）。2026-09-28 同步修订（①⑤）：契约形态 TOML → XML+XSD（ioc-review §15.3/§15.4）、.def 表述按 C-50 修正（改以 ioc-review §16 源码证据为准：BigWorld entity_description.cpp:184-190、KBEngine entitydef.cpp:188-210，两工作副本为浅克隆/官方包）；行号基线仍为 35a9c528。2026-09-29 追加 §10（契约→protobuf 编码：两层模型/映射规则/proto 后端/差分配合/zstd 边界/三路线权衡——编译期生成 vs descriptor.bin 反射池 vs 运行时解释，含各运行时动态加载 API 查证与 C++ DescriptorPool 换代注意点；**定案 A+C 双轨、C 为主**：A 收窄服务端热路径，C 为默认通道走客户端热更资源管线，生成器同批吐 .pb.cc 与 descriptor.bin，schema_hash 两态一致）——引用 attribute-sync §3/§5/§7、net-abstraction §3/§5.5、ioc-review §16.7.2、contract_model.hpp:19-27 掩码位（@ a5334014 实读）、sdks/contract v1 全部契约文件（@ a5334014）；§8 P2 分期同步扩为 proto/bin 双后端行；决策同步 docs/36 决策追溯表 #19。*