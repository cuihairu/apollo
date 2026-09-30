# Apollo 多端 SDK 契约设计（sdk-contract）

> 状态：设计稿（评审中）。参照：KBEngine 的"单独 `sdks/` 目录 + 契约定义协议约定"精髓（学精髓不学形），并修正其短板（对数据的存储太弱——分析见 `docs/design/attribute-sync.md` §8）。本设计与 `attribute-sync.md`（属性模型/预测/attr_schema_hash）、`net-abstraction.md`（帧/通道）、`scripting-lua.md`（脚本白名单）、`architecture-review.md`（装配纪律）互为引用。

---

## 执行摘要

1. **契约文件是唯一事实源**：`sdks/contract/` 的 `attrs.xml` + `messages.xml`（XML + XSD，architecture-review §15.3/§15.4）定义全部属性与消息（字段/类型/可见性/同步组/预测位/通道），**代码生成器**产出服务端访问器 + 各端 SDK 序列化与客户端属性模型 + 文档（生成器内部设计见 `xml-generation.md`）。现状是反的：`modules/protocol` 手写编码、全仓库（含 SDK）无任何 .proto/.def/.toml 契约、客户端手写 `MessageCodec.ts`/`ByteBuffer.ts`——协议每端维护一份、必然漂移。
2. **目录收敛为唯一的 `sdks/`**（拼写错误目录 `skds/` 合并回原名）。事实：`git log 38656f90 refactor: 重命名 sdk 目录为 skds`——一次 rename 引入了 typo，之后新工作（unity cocos laya 的 Network/Session/Messaging 层）落在 `skds/`，而旧 `sdks/` 残留另一套内容（unity 的 Attributes 体系），**两份互斥、互相不通**。结构对齐 KBEngine：`sdks/{contract,gen,unity,ue5,cocos,laya,cpp,docs}`。
3. **学 KBEngine 的位置只有一处：契约驱动多端。** 但三处不学：(a) 不复刻 .def 的裸 XML 解析——**C-50 修正：.def 本就是 XML**（BigWorld entity_description.cpp:184-190、KBEngine entitydef.cpp:188-210，architecture-review §16.3），真正的差异是两家都**只解析、无 schema 层**（错拼标签静默缺省）；apollo 用 XML + XSD 形式校验（architecture-review §15.3），策划可读、diff 友好、校验器零开发；(b) 不做"一份通用序列化给所有端"，生成器按端定制造型（C# 事件、TS 模块、C++ 访问器是其各自惯用法）；(c) **存储不进契约**——KBEngine 的 .def 把属性形态与存储耦合（blob 化复杂类型、base-only 快照，见 attribute-sync §8 五条），契约只管线上协议形态。
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

<!-- entities.xml —— 实体清单与继承（architecture-review §16.7.2：单继承、生成期展开；xs:keyref 锁 parent 引用，环检测在生成器） -->
<entities version="7">
  <entity id="Monster"/>
  <entity id="Avatar" parent="Monster"/>
</entities>
```

风格纪律（§15.3）：紧凑一行一属性元素，勿子元素嵌套；`xs:key` 锁 id 唯一（C-35 分段冲突无法入库）、`xs:keyref` 锁引用（派生 DAG/parent/列提升）、`xs:enumeration` 锁域/通道/所有权取值。

## 3. 目录结构与生成器

```
sdks/
├── contract/        # 契约源（唯一事实源）：apollo.xsd、attrs.xml、messages.xml、entities.xml、errors.xml、version（architecture-review §15.4 布局）
├── gen/             # 生成器（C++ 单二进制，入 CI；失败即阻断协议发版）
├── cpp/             # 生成产物 + 手工壳 → 以模块链入 modules/protocol
├── unity/           # U3D 插件（生成 + 手工运行时壳）
├── ue5/             # UE5 插件（规划 P3）
├── cocos/           # Cocos Creator TS（生成 + 手工壳）
├── laya/            # LayaBox TS（与 cocos 共享生成内核）
└── docs/            # 协议文档/变更日志/错误码表（生成器产物）
```

生成器输入=契约文件，输出=5 类产物：① C++ 编码/访问器与校验（服务端 + cpp 端 SDK）② C#（Unity）③ TS（Cocos/Laya，同一模板两处实例化）④ 协议文档 ⑤ `schema_hash` 常量头。**CI 强制**：契约变更必须提交过生成产物（pip 式 diff 检查），防"改了契约忘了生成"。

链接边界（architecture-review §16.8.3-②）：gen 是**运行期依赖图之外**的构建期工具——不被任何运行期目标链接，产物被 modules/protocol、tables 消费模块与各 sdks/* 链接，运行期代码禁止 include gen 内部头。先例：KBEngine 配置转换器在源码树外（kbe/tools/xlsx2py）、BigWorld 工具族独立二进制（server/tools）。

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

具体：新增"需要存档的属性"只改服务端持久层（column promotion，attribute-sync §8.2），契约零改动——因为客户端不需要知道服务端存哪张表；同理契约加字段（如 UI 表现提示）不动存储。存储定义的具体形态 = 服务端私有 `storage.xml`（表/列提升/journal 语句，"语句即数据"的 MyBatis 形态，xml-generation.md §6；不在契约目录、不进 schema_hash——architecture-review §15.4/§15.5 两段式）。**协议与存储的演化周期互相解耦，是"学精髓"的最后一环。**

## 8. 分期

- **P1**：目录收敛（`skds/` 内容并入 `sdks/`，删空壳保留正确拼写；git mv 记录）；契约 v1（`apollo.xsd` + attrs/messages/entities/errors.xml，§15.4 布局）；生成器 v1（C++ + 文档 + hash，内部设计见 `xml-generation.md`）；Unity 端按四层重组（Codec 生成 + Attributes 层并入生成产物）。
- **P2**：Cocos/Laya TS 生成；第 3 层（AOI 实体管理）生成壳；预测混合器 v1（movement 双缓冲）；**反射通道与端侧代码包装**（§10.6 v3 定案：.proto golden → protoc 同批出 `descriptor.bin`（业务消息反射 + sol2 桥进 Lua）+ `contract.lua`（服务端语义）+ `semantic.json`（客户端语义）+ protoc/pbjs 端代码（有代码热更管线的端）——**反射为默认、代码为两端增强**，同批实现）；**契约内外分域**（§11：domain 属性 + msg id 按域分段 + 双 hash + 按域过滤 + 跨域禁令，与 bin 投影同一机制）。
- **P3**：UE5 插件；跨端事件协议评审；契约变更流水线接 CI 发版（§12：`compat_window_check` job + VSCode 插件层 + 预编译单二进制分发）。

## 9. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| attribute-sync.md | 契约 §2.3 即其 §2 属性模型/§9 预测矩阵的机器可读形态；`attr_schema_hash`/`attr_schema_version` 握手与切换；快照/增量消息即 `attr_batch` |
| net-abstraction.md | 帧格式与四通道写进契约（各端插件据此实现帧定界与通道映射）；`modules/protocol` 手写代码退役 |
| scripting-lua.md | 脚本白名单由契约生成（§5）；合同版本随实体下发（scripting-lua §3.3） |
| ssengine-reference.md | 弱存储教训的出处（attribute-sync §8）；sdpkg 帧头方向对齐 net-abstraction §3 |
| architecture-review.md | 无关联容器语义；作为"配置只有一份"纪律在跨端维度上的延伸；§15.3/15.4/15.5 契约决策源、C-50 .def 修正（§16.3） |
| xml-generation.md | gen/ 生成器的内部设计（pugixml 解析、校验四层漏斗、产物五类、MyBatis 映射）；本设计的 gen/ 即其实现载体 |

---

## 10. 契约 → protobuf 编码：两层模型与 proto 后端（2026-09-29 追加）

> 定位：README/36 号文档把「def 管语义、protobuf 管字节」一句话带过（决策 #3），本节写透——两层模型、def→proto 映射规则、生成器 proto 后端、与差分同步的配合、zstd 边界，以及「编译期生成 / descriptor 反射池 / 运行时解释」三路线的正面权衡（§10.6）。分期落 P2（§8 已补行）；第一批契约（`sdks/contract` v1 @ a5334014）尚无 proto 产物，本节是其后端设计定稿。引用基线：attribute-sync §3/§5/§7、net-abstraction §3/§5.5、architecture-review §16.7.2、docs/05 §6.1/§6.3（docs/05 已于 2026-09-30 删除，git 可溯）。

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
│  entities.xml 实体：单继承（生成期拍平，architecture-review §16.7.2）       │
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

现状分段（docs/05 §6.1——已删 git 可溯；attrs.xml 注释同源）：1-99 基础 / 100-199 战斗 / 300-399 状态 / 600-699 外观 / 700-799 社交 / 800-899 标记。三个候选方案：

| 方案 | 形态 | 判定 |
|---|---|---|
| A 全属性单 message（逐属性 field，§10.2.1 强类型路径） | 生成一个 AttrSet | **P3 可选产物**（编辑器/调试工具 golden），非运行时通路 |
| B 同段一 message（AttrsBasic_1_99 / AttrsCombat_100_199…） | 按段分组投影 | **否决**——把「段」编进 wire 词汇：docs/05（已删 git 可溯）分段表调整（语义组织演化）会强制 schema 变更；段是给策划/评审的组织，不是编码组织 |
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

entities.xml 单继承链 `Monster ← Avatar ← Player` 在生成期展开（父先入、子覆盖、禁同名改型、带 provenance——architecture-review §16.7.2），**三个消费端只见扁平结果**（attribute-sync §7.3）。proto 层因此**没有继承概念**：属性流里实体只以 `entity_id + template_id` 出现，客户端按模板知道属性集。两个候选均否决：

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

**v3 后端扩展与工具链分工（2026-09-29 定案修正，定案见 §10.6 末）**：产物清单从「.pb.cc + .bin 双后端」修正为——`.proto`（golden，bin 的源）+ `descriptor.bin` + `contract.lua`（服务端 Lua 契约表）+ `semantic.json`（客户端语义小件）+ `contract_route`（服务端 C++ 帧路由清单：msg id → name/dir/domain/Lua handler 绑定——按 id 分发、上行白名单、域过滤；§10.6 附节两解码案共用）+ 端侧薄常量（AttrIds/错误码）+ protoc/pbjs 端代码（CI 单点吐、golden 入库）；业务消息的 `.pb.cc` 不再默认产出（框架固定消息族的内建代码除外——其 schema 不随契约变，见 §10.6 消息两分法）。生成器内部形态澄清：**无自建 AST**——pugixml DOM 即解析树（选 XML 而非自定义 DSL 的直接红利），自建核心是 IR（ContractModel + 四层校验），各产物是「读 IR → 渲染文本」的薄 writer（无优化 pass、无语义变换）；C# 消息类由 protoc `--csharp_out` 吐（现成生成器，非自研模板）。工具分发纪律：**前端零工具链**——本节「golden 入库、protoc 只在 CI」的既有纪律推广到全部产物，端插件只拉产物做导入（生成代码进工程、bin/semantic 进资源目录、登记 manifest），绝不做第二个契约解析实现（防 §1 批判的多端漂移借尸还魂）；改契约的动作天然发生在服务端仓库（apollo_gen 是 CMake 目标之一）；contract.lua 是 IR 的 Lua 表字面量 dump（数据非代码，无编译环节——可选 luac 预编译随 Lua 5.4 发行版自带）；本地快速迭代的预编译单二进制为可选进阶，非必需。

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

**分档用法**（首版口径；v3 修正见定案段——热路径改由框架固定消息族内建强类型承接，反射不进热路径）：

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

**既有决策链站的是「构建期定型」，不是「A 独占」**（非本节新立）：决策 #4 生成器独立二进制、不进运行期链接图、失败阻断发版；architecture-review §0 否决运行时容器形态——运行时解释契约正是「运行时容器」在数据面的镜像；契约第一批已落 static_assert 编译期闸（modules/contract/tests/gen_compile_test.cpp 第③层）；决策 #11 的对照注记（UE 的 DOREPLIFETIME 是 C++ 宏、改协议必重编——apollo 把掩码声明从代码移进契约数据，改掩码 = 改 XML + 重新生成，不改手写代码；「运行时可配」指声明位置数据化，非运行期解释）。A 与 C 同属「构建期定型」家族——.bin 由 apollo_gen 构建期吐出、protoc 校验、运行期只读（加载后不解释源文件、不改结构），与 architecture-review §0「启动期定型、运行期只读」同一纪律，不落入「运行时容器」的批判面；A/B/C 三路线的分轨结论见本节末定案。

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

**定案（2026-09-29，同日修正为 v3）：编解码路线 = 反射为默认、代码为两端增强。**

首版定案为「A+C 双轨、C 为主」（A 收窄服务端热路径、C 为客户端默认）。修正触发 = 两个新输入：① **端侧代码热更通道**（Unity HybridCLR 的 C# DLL 热更、Cocos/Laya 的 TS 脚本原生热更）使 A 的头号代价（商店发版）消失——编译期严谨与热更可兼得，C 全量包端不再必要；② **服务端零重编诉求**使业务消息的 .pb.cc 投影退役。v3 口径：

- **关键补丁——消息分两类**：**框架固定消息族**（movement/attributes/control 通道：AttrDelta/AttrBatch/move/heartbeat…）schema **不随契约变**（契约变的是 attr_id 取值域/枚举校验，不是这几个 message 的形状）→ 强类型代码一次生成**内建进框架**，契约变更永不触发其重编；**业务消息**（events 等通道）schema 随契约变 → bin 反射。反射税只落在业务消息（单服 5000 CCU、人均秒级数条 ≈ 万条/秒，DynamicMessage µs 级解码，CPU 占比可忽略）；**热路径零反射**。边界进契约：messages.xml 加 `binding="native|reflect"`（默认按通道：movement/attributes/control=native，events=reflect）。
- **服务端：契约变更零重编**。装载 descriptor.bin（全量）+ contract.lua（Lua 契约表：attr 表/消息路由/白名单——semantic 同内容双形态，require 即用）；业务消息 DynamicMessage ──sol2 桥（遍历 descriptor 字段搬运，固定框架代码一次写好，不随契约变）──► Lua 表 ──► Lua handler；属性容器表驱动（id → AttributeValue，从 contract.lua 构建）。契约变更 = 换 bin + 换 contract.lua + Lua 热更 handler，**C++ 不动**。DescriptorPool 换代式热更（本节注意点①：新 bin 新 pool，旧 pool 等存量消息生命周期结束整体废弃）。
- **客户端：按代码热更能力分档**。有热更管线的端走**生成代码**（protoc `--csharp_out` → HybridCLR 热更程序集；TS 走 pbjs/ts-proto → 脚本热更）——编译期类型检查 + 热更兼得；**bin 为兜底通道**（UE5 P3/工具/GM/调试/新端快接/不接代码热更的团队）——零生成代码零编译，加载期以 semantic.json 校验引用完整性（业务引用的 attr id/字段名全部存在）挡大头。
- **加载期一致性闸（严谨性来源从编译期改装载期）**：服务端启动/热更装载三件互检——bin 字节重算 ↔ internal_hash 锚定（protoc pin）；bin 消息集 ↔ contract.lua 路由表**逐条对齐**（bin 有而 Lua 无 handler = 启动红——KBE 是运行期才 miss，§11.1）；白名单 attr id ↔ contract.lua 存在性。manifest 机制同一套（§11.6）。与 KBE 运行时协商的本质区别：**构建期定型产物 + 装载期闸** vs 无事实源运行期自证。
- **生成器形态**：apollo_gen 同批吐 `.proto`（golden，bin 的源）/ `descriptor.bin` / `contract.lua` / `semantic.json` / 端侧薄常量 / protoc/pbjs 端代码（§10.3 v3 段）；业务消息 `.pb.cc` 不默认产出（框架族内建代码除外）。schema_hash 机制不变（§6 算法对 canonicalBundle + .bin 字节锚定自验，两态一致）。
- **定案理由**：演进链路里两个最慢环节（服务端 C++ 重编、端侧商店发版）**同时**与协议解耦；严谨性不依赖编译期——由「装载即全量校验」承接；热路径严谨与性能由框架族两分法保住。
- **批次影响**：bin/lua 后端 + sol2 反射桥 + 校验闸 + 端代码包装**同批实现**（§8 P2 已改口径）；B 路线维持否决。与决策链相容：bin 构建期定型运行期只读（不落 architecture-review §0「运行时容器」批判面）；两层模型（§10.0）/分域四条（§11.3）/manifest（§11.6）全保留；决策 #12 加深——Lua 从业务脚本升格为契约语义的运行时载体（白名单仍由契约生成，载体明示为 contract.lua）。
- **诚实代价（记入批次评审）**：sol2 反射桥的维护面（packed repeated/嵌套递归）；Lua handler 层无编译期类型（契约 golden 回放测试兜回归，与决策 #12 既有形态一致不恶化）；binding 两分边界需契约评审把好关；C++ 侧新增固定模块（descriptor 加载 + 反射编解码 + 校验闸）。

#### 业务消息解码位置：C++ 桥 vs Lua 侧解（两案，装配选择非契约分叉；2026-09-29 补）

> **2026-09-30 更新：sol2 弃用（上游维护停滞）**——本文「sol2 桥」一律读作「**C-API 搬运桥**」：桥本就是自写固定通用转换代码（下段澄清其非 sol2 能力），去 sol2 只是把「搬运进 Lua 表」的函数族从 sol2 模板换成原生 Lua C API（`lua_newtable`/`lua_pushinteger`/`lua_settable` 族），数量级与维护面反降。历史段落中的「sol2 桥」字样保留原文（v3 定案记录），执行以本注为准；scripting-lua §2 同批修订（**Lua 5.5 线随 vcpkg + 原生 C API 绑定定案**——版本策略 2026-09-30 再修订：锁 5.5 主线、补丁位跟 vcpkg lua port，当前 5.5.x）。

v3 把业务消息定为 bin 反射后，剩一个装配级选择：**反射解码发生在 C++ 还是 Lua**。先澄清一个命名误会：「sol2 桥」不是 sol2 的动态绑定能力——sol2 的 usertype 绑定是编译期模板（每字段写死），对运行时才知道结构的 DynamicMessage 无能为力；「桥」指**自写的一段固定通用转换代码**：遍历 descriptor 字段（`GetDescriptor()->field(i)`）、Reflection 取值（`GetInt64/GetString/…`）、sol2 搬运进 Lua 表，嵌套 message 递归、packed repeated 展开为 Lua 数组，反向（Lua 表 → DynamicMessage，供回复/广播）同构——几十行、全部消息通用、不随契约变。「动态绑定」的效果来自 descriptor 数据驱动的循环，不是任何库的现成机制。

| | 案一：C++ 反射 + sol2 桥（**默认**） | 案二：字节透传 + Lua 侧 lua-protobuf |
|---|---|---|
| 解码位置 | C++ DynamicMessage → 桥 → Lua 表 | C++ 只做帧/L1/按 id 路由，payload 字节递 Lua：`pb.load(bin)` + `pb.decode(name, bytes)` |
| C++ 消息面 | **完整**——按内容审计/限流细分/进程内 GM/网关按内容路由共用 DynamicMessage 设施 | 不透明——中间件只能按 msg id/name/dir（帧路由层映射，覆盖绝大多数用例） |
| 自研代码 | 通用桥几十行（固定） | 无桥（pb.decode 返回即 Lua 表） |
| 依赖 | protobuf 主库（**已是依赖，零新增**） | lua-protobuf（C 实现的 Lua 模块，活跃；pbc 停维护不选）——新增构建依赖 |
| 解码性能 | C++ 反射，µs 级 | lua-protobuf 为 C 实现，同量级 |
| 一致性闸执行者 | C++（读 bin ↔ 路由表对齐，§10.6 定案段） | 可移至 Lua（pb.load 后枚举类型 ↔ contract.lua 路由表对齐）——闸的**语义两案相同**：装载期逐条对齐、缺失即启动红 |

**性能量化（估算口径，非基准）**：案二结构性少一次中间表示——案一 = wire→DynamicMessage→Lua 表**两次转换**（每字段一次 Reflection getter + 栈操作），案二 = wire→Lua 表**一次转换**（C 实现边解析边建表）——差异是 ~1.5-2× 常数因子，非库实现优劣。单条 10 字段消息（~100B）：案一 ≈1.5-3µs、案二 ≈0.6-1.6µs；负载基准 25k 条/秒（5000 CCU×5 条/秒）→ 案一 ~50-75ms CPU/秒（20Hz 主线程 ~4ms/帧，8%）、案二 ~15-40ms/秒（~2ms/帧，4%）——**均在帧预算 10% 内，性能不构成决策依据**；解码亦可放 IO 线程池旁路（逻辑线程只收 Lua 表——两案同开放，选它则差异不出主线程）。内存：案一每消息一个 DynamicMessage 堆对象（分配 churn，可 prototype 池化），案二仅 Lua string+table（少一层 C++ 堆分配）。

**装配粒度可以 per-进程（谱系而非二选一）**：网关进程用案二形态（纯透传、零反射设施）+ Zone 服用案一形态（解内容做审计/GM）——契约/golden/route 清单完全不变，各进程只差装不装反射数据面——「两案」实为同一装配谱系的两端。

其余维度精简补记：**接入层内容过滤**——案一网关可解内容做前置校验（坐标/频率/参数范围在 C++ 拦），案二下沉 Lua（同为服务端受信代码，安全性不损失，损失的是网关独立前置过滤）；**调试**——案一 C++ 侧 `DebugString()`/perf 可见内容，案二排查走 Lua（多一跳）；**错误处理**——案一解码错误在 C++ 帧层捕获丢弃，案二冒到 Lua（`pb.decode` 返回 nil+err）需 handler pcall 纪律；**测试**——案一桥需 round-trip 单测，案二 decode 是库的事、契约回放即覆盖。

**契约设计影响：两案零契约分叉。**binding 属性维持两值（native|reflect）只管「框架族 vs 反射」——**「反射在哪解」是部署装配选择，故意不进契约**；两案共享同一契约源、同一批 golden、同一份 descriptor.bin（lua-protobuf 吃的也是标准 FileDescriptorSet——与客户端 bin 同格式）。唯一的产物面增量是**两案共用**的轻量路由清单：C++ 帧路由层在任何案下都需要 msg id → name/dir/domain/Lua handler 绑定（按 id 分发、上行白名单、域过滤都查它）——定为 apollo_gen 固定新产物 `contract_route`（JSON/常量形态，IR 直吐，§10.3 清单已补）。

**切换条件（写明，防摇摆）**：默认案一——保 C++ 消息内容可见性（按内容审计/GM/按内容中间件）+ 零新依赖；仅当确认 C++ 永不需要业务消息内容、且团队偏好更薄 C++ 时切案二——切换是换装配（C++ 侧不装反射数据面，路由清单不变），契约与 golden 零改动。

### 10.7 收束

def 契约与 protobuf 的关系一句话：**契约是源，proto 是它最重要的投影之一；语义层（掩码/predict/继承/分段）不进 wire，wire 层（varint/zigzag/缺省零字节）不生语义；zstd 在帧层消 protobuf 消不掉的跨条目冗余**。多端一致由「单 golden + schema_hash + protoc pin + 装载期一致性闸」闭环；路线定案为**反射为默认、代码为两端增强**（§10.6 v3：框架固定消息族内建强类型守热路径——schema 不随契约变、永不因契约重编；业务消息 bin 反射经 sol2 桥进 Lua，服务端契约变更零重编；有代码热更管线的客户端走生成代码、bin 兜底——服务端重编与端侧发版两个最慢环节同时解耦，严谨性由装载期一致性闸承接）；业务动态性由通用容器字段 + configId + Lua 三扇门承接——协议结构层的稳定不再是对演进速度的牺牲，而是对「带病上线」的免疫。

---

## 11. 内外契约分域：KBE「内外契约不分离」缺陷登记与规避设计（2026-09-29 追加）

> 缺陷来源：用户实测 KBE 代码发现（行号为本会话浅克隆工作副本实读核对）。规避设计定稿口径：**域 = 消息上的逻辑属性，不是物理文件**——契约物理组织不做强制，唯一硬规则是域分段逻辑（ID 分段 + 双 hash + 按域过滤 + 跨域禁令）必须成立，与物理组织正交。分域只作用于 messages（attrs 的 SELF/TEAM/GUILD/WORLD 是「客户端内」的可见域细分，不属本节的内外之分；服务端私有数据走 internal 消息或存储层）。

### 11.1 缺陷登记：KBE 内外契约不分离（A 级实证）

**同一文件混布三类受众的消息**——`kbe/src/server/baseapp/baseapp_interface.h`：

| 受众 | 消息（行号） | 标记 |
|---|---|---|
| 对客户端 | `hello` :99-100、`loginBaseapp` :144-145、`logoutBaseapp` :150、`reloginBaseapp` :156-157、`onClientActiveTick` :112-113、`reqAccountBindEmail` :264、`reqAccountNewPassword` :289 | `BASEAPP_MESSAGE_EXPOSED` |
| 对 dbmgr | `onDbmgrInitCompleted` :90 | 无标记（内部） |
| 对 cellapp | `onMigrationCellappStart` :326、`onMigrationCellappEnd` :331 | 无标记（内部） |

**EXPOSED 只是导出标记，不是命名空间隔离**——`baseapp_interface_macros.h:27-36`：`BASEAPP_MESSAGE_EXPOSED(NAME)` 展开为 `NETWORK_MESSAGE_EXPOSED(Baseapp, NAME)`，仅影响「是否允许客户端通道调用」的检查，不参与 ID 分配语义。

**病根：消息 ID 共享单一分配表**——全部 handler（不分受众）经 `MessageHandlers::add`（`kbe/src/lib/network/message_handler.cpp:139`）注册进同一张表，`msgID_` 是单一自增计数器（`message_handler.h:116` `lastMsgID() {return msgID_ - 1;}`、`:133`）；`FixedMessages`（`fixed_messages.cpp`）可给个别消息钉死固定 id，但钉的是散点、非按受众分段。后果：**任何内部消息的增删都推动后续 ID 排布**——内部演进与客户端 SDK 重生成绑死，内外无法独立演进。文件混不混只是表象，**ID 空间不分为病根**。

**运行时协商掩盖契约不分**：`importClientMessages`（`baseapp.cpp:4859`→`:4891`、`loginapp.cpp:1416`→`:1469`）——客户端连接后请求，服务端把整张消息表（名称/id/参数类型）经 `ClientInterface::onImportClientMessages`（`client_lib/client_interface.h:174`）动态下发，客户端运行时建表。消息表内容即契约，却不存在一份构建期事实源——每连接一次协商，「两端一致」靠运行期自证而非构建期锁定。

### 11.2 BigWorld 对照：同病异形（Mercury 消息表查证）

- **声明层混布同型**：.def 按实体单文件混布 `<ClientMethods>`/`<BaseMethods>`/`<CellMethods>` 段（Account.def，architecture-review §16.2 已录）——客户端可见方法与服务端内部方法同文件同源。
- **ID 空间同为单表**：Mercury 每进程一张 `InterfaceMinder` 表，`add()` 以 `elements_.size()` 顺序自增分配 ID（`lib/network/interface_minder.cpp:38`）；表上限 255（`interface_minder.hpp:35` `interfaceElement(uint8 id)`）。客户端可调用的暴露方法经 `ExposedMethodMessageRange`（`lib/entitydef/method_description.hpp:31`）调 `addRange`（`interface_minder.cpp:53-70`，按剩余空间的 1/x 等分）在**同一张表**内占连续保留段。
- **判读**：BW 比 KBE 前进一步——把暴露方法收敛为保留段，段内增删不推动表内其它消息的排布；但保留段本身按注册序在单表内划分，内部消息与暴露方法共享同一 ID 空间的病根同型（且 uint8 上限使空间更紧）。**两家同病：单一 ID 分配表不分受众。**

### 11.3 规避设计：域分段逻辑（硬规则，与物理组织正交）

四条规则全部落在生成器/XSD/hash 机制上，不依赖文件怎么拆：

**① ID 空间按域分段。**messages 的 msg id 空间（独立于 attr id）划两段：client 域与 internal 域各占一段（示例：client 1-899 / internal 900+，段值落地时定、XSD 锁段边界——沿 attrs.xml id 分段同型纪律，docs/05 §6.1 先例——已删 git 可溯）。**段内自由增删，互不推动对方排布**——对 KBE 单一分配表病根的直接反制。落地形态：msg 元素带 `domain="client|internal"` 属性（xs:enumeration），id 与 domain 的段约束进 XSD。

**② schema_hash 按域算两份。**`client_hash` 只覆盖 client 域的 canonical bundle，`internal_hash` 只覆盖 internal 域（算法同 §6，输入按域过滤）。internal 变更不改 client_hash——**客户端握手稳定、bin 不重发**；握手用 client_hash（§6 的 N/N-1 窗口语义不变，对象收窄为 client 域），internal_hash 只做服务端部署期同批断言（同批重编同批起，无需灰度窗口）。落地口径（P2 已实现）：client bundle = client 域消息 + attrs + errors（与客户端产物面内容一一对应）；internal bundle = internal 域消息 + entities；**手工 version 不进域 bundle**——version 随任何变更走，若入 bundle 则 internal-only 变更也推动 client_hash，握手稳定即失效（全量身份含 version 仍由 schema_hash 锚定）。配套携带面纪律见 §11.6。

**③ apollo_gen 按域过滤投影。**同一份契约源出两种产物面：客户端 bin（及 TS/C#）**只投影 client 域**，服务端 `.pb.cc` 全量——一个源文件两种产物，**单一事实源不破、diff 集中**（变更永远只看一处）。域过滤与 §10.6 定案的 bin 投影是同一机制，同批实现。

**④ XSD 跨域引用禁令。**client 域消息的参数引用 internal-only 类型（含 internal 域消息/内部 struct）直接报错——域边界进校验器，不靠评审自觉；与 §2.3 的 xs:key/xs:keyref 纪律同层。

演进节奏由此解耦（②③的直接推论）：internal 域变更 = 服务端同批重编部署；client 域变更 = 字段号兼容 + bin 热更（§10.6 定案的资源管线）——两类变更互不牵连。

```
            ┌─────────────────────────────────────────────┐
            │ sdks/contract（单一事实源；物理组织任选，§11.4）   │
            │ messages：msg 带 domain 属性 + id 按域分段        │
            │   client 段 1–899       internal 段 900+         │
            └────────────┬──────────────────┬────────────────┘
                         │ apollo_gen 按域过滤（同一机制，两投影）
          ┌──────────────▼───────────┐    ┌──▼──────────────────────┐
          │ client_hash              │    │ internal_hash           │
          │ = SHA-256(client 域       │    │ = SHA-256(internal 域    │
          │   canonical bundle)      │    │   canonical bundle)     │
          └──────────────┬───────────┘    └──┬──────────────────────┘
                         ▼                   ▼
          客户端 bin / TS / C#            服务端 .pb.cc（全量）
          只含 client 域                  internal 变更 → 同批重编
          internal 变更 → hash 不变        client 变更 → 字段号兼容
          → 握手稳定、bin 不重发              + bin 热更（§10.6）
```

### 11.4 契约物理组织：三种形态的事实记录（不设强制）

物理组织交给使用方决定。生成器格式层提供 **include 聚合**能力——pugixml 不内建 XInclude，展开在 apollo_gen 读取层自实现：读根文件、递归展开引用为零一棵文档树（零新依赖），供需要分文件的人用。XSD 校验与 schema_hash 一律对**聚合后整体**计算，物理分文件不影响 hash 稳定性。三种组织形态的事实：

落地注记（P2 第五批已实现）：`expandIncludes` 在目录级读取层（parseContractDirectory）对四类文件统一展开——include 只认根直接子级（XSD 把位置锁在根子级开头；深层 include 由 strictWalk 白名单拦）；被引文件根元素同名且 version 一致；环/缺文件/根不匹配/version 不一致/缺 href 各有专属诊断；重复 include 允许拼接（重复 key 由既有规则拦）；「分文件目录与单文件目录的模型与 schema_hash 完全相等」由测试锁死。

| 组织形态 | 事实 | 域分段逻辑（硬规则） |
|---|---|---|
| 单文件 + domain 属性 | 现契约 v1 即此形态（补 domain 属性即是）；diff 最集中；无展开层 | 成立——分段看 id+domain，不看文件 |
| 多文件 + include 聚合 | client.xml / internal.xml（或按模块）各自维护，根文件引用聚合；消息量增长后可按评审域拆 diff | 成立——展开后即单树，hash/校验对聚合整体 |
| 独立两契约 | client/internal 各一套 XML+XSD，无聚合根 | 各自成立，但**源分两处**——与「唯一事实源」前提（执行摘要 1、§1）冲突；跨域引用禁令与统一 diff 闸均按单源建设，采用它须先推翻该前提 |

**唯一硬规则重申：域分段逻辑（ID 分段 + 双 hash + 按域过滤 + 跨域禁令）必须成立——它约束 ID 空间与产物面，不约束文件布局。**

### 11.5 落地批次

随 P2 proto/bin 双后端同批（§8 已补行）：domain 属性与 id 分段进 XSD 是契约侧小改；按域过滤是 bin 投影的同一趟代码；双 hash 是 §10.6 schema_hash 两态一致的直接推广（client_hash 锚定 client 域 bin 字节，internal_hash 锚定服务端 .pb.cc 批次）；客户端契约包（§11.6 三件套）是 bin 后端的同一趟产出。

### 11.6 客户端契约包与 manifest：多文件交付的一致性闭环（2026-09-29 追加）

对 §10.6 的补漏：descriptor.bin 只覆盖 wire 层——§10.6 已声明掩码/predict 不进 .bin，语义消费端走 JSON golden 与生成常量表。客户端实际还需要**语义侧小件**：predict 表（→ 预测混合器）、attr id→name（→ 调试/UI 绑定）、错误码表。「多个文件」是事实，不回避——解法不是多文件各自校验，而是**交付单元从文件升成包**：

```
dist/client/<client_hash>/          ← 目录名即版本指纹（CDN 版本目录按 hash 寻址，缓存友好）
├── manifest.json                   # 包指纹：client_hash、生成器版本、逐文件 SHA-256 清单
├── descriptor.bin                  # wire 层（protoc 标准产物，任何 protobuf 运行时直接吃）
├── semantic.json                   # 语义侧小件：predict 表、attr id→name、错误码
└── (可选) 生成的 .cs/.ts           # 有代码热更管线的端（§10.6 v3）：protoc/pbjs 产物
```

apollo_gen 同批吐三件（与 .pb.cc 同一次跑；有代码热更管线的端加第四件：protoc/pbjs 生成的 .cs/.ts——**同进 manifest 清单**，「新代码旧 bin」混搭同被加载期逐文件 hash 挡住）——manifest 把「同批性」从口头纪律变成**端侧可验的物证**。服务端侧同构三件互检：bin + contract.lua + 白名单（§10.6 v3 加载期一致性闸）。**携带面纪律（P2 已实现）**：包成员文件只携带 client_hash 与生成器标识——不带全量 schema_hash、不带手工 contract_version（两者都会被 internal-only 变更推动：目录名 client_hash 未变而文件字节变了，CDN 版本目录不变性即破）；已落地的 semantic.json 与 client 域 .proto 头按此携带，manifest 字段同口径（§11.3 ② 的 bundle 划分与之配套——version 不进域 bundle）。

校验链（两层，全在加载期/握手期，不进运行期）：

```
拉包（热更管线）→ 读 manifest → 逐文件算 SHA-256 比对 → 任一不符【加载期红】
               → manifest.client_hash 与服务端握手比对 → 不匹配【握手红（§5：拒绝+提示升级）】
               → 全过 → bin 进 DescriptorPool + semantic.json 进 SDK 第 2 层
```

不一致路径逐条封堵：

| 场景 | 挡在哪 |
|---|---|
| 新 bin + 旧 semantic.json（热更部分失败/CDN 缓存错配） | manifest 逐文件 hash——加载期红，非运行期数据错位 |
| manifest 与文件不符（篡改/半包） | 同上，加载期红 |
| 整包旧版本（bin+semantic+manifest 自洽） | **合法**——正是 N/N-1 窗口成员（§6 语义不变） |
| 整包新、服务端旧 | 握手红（§5） |

关键性质：**版本校验只有一个点——client_hash**（§11.3 ②的握手对象）。不存在「每文件一个版本号」的多点漂移面；文件间一致性是 manifest 的机械校验，加载器写一次、全端共用同一段逻辑。

「合成单文件」三案否决：

| 候选 | 否决理由 |
|---|---|
| 自定义容器 `[len][json][bin]` | 每端写剥壳器；丧失 C 路线核心优势——bin 是 protoc 标准产物，任何运行时直接 `ParseFromString` |
| 语义塞进 .proto（custom option/enum） | 语义硬编进 wire 描述，扭曲 §10.0 两层模型；protoc 校验面管不了它 |
| 单 blob 打包 | 无增量更新——semantic.json 很少变、bin 随消息变，分文件只重发变化的文件；manifest 是纯 JSON，与 AssetBundle/小游戏分包 manifest 惯例同构，零学习成本 |

一句话：**多文件是真，但「不一致的可能」被 manifest 逐文件 hash + 单点 client_hash 在加载期/握手期全部闭环，不进运行期。**

---

## 12. 契约工具链与工作流：生成流程、CI/CD 与编辑器集成（2026-09-29 追加）

> 定位：§10.3 v3 定了生成器形态与分发纪律，本节把**端到端流程写明确**——从改契约到两端生效的每一步、每步的工具与失败挡点，以及 CI/CD job 与编辑器集成的分期提供。纪律红线：**所有工具只消费 apollo_gen 与其产物，绝不做第二个契约解析实现**（XSD 即时校验除外——那是标准 XML Schema 机制，由编辑器扩展执行，非自研）。

### 12.1 端到端生成流程（谁、用什么、错在哪挡）

```
① 编辑契约       服务端开发者（改契约的人，天然在服务端仓库）
   sdks/contract/*.xml —— VSCode + XML 扩展关联 apollo.xsd
   → 实时校验/补全/枚举提示（编辑时红线波浪，零自研，§12.3 零成本层）
② 本地生成       cmake --build --target apollo_gen && apollo_gen sdks/contract
   → 重吐全部 golden：.h/.json/.proto/descriptor.bin/contract.lua/
     semantic.json/端侧薄常量与端代码包装
   → 四层漏斗（XSD→解析→static_assert→启动前置检查，xml-generation §5）
③ 提交           契约源 + 全部 golden 同一提交（漏了④兜底）
④ PR 校验        CI 重跑 apollo_gen → 与提交产物 byte-diff
   → 不一致 = 红（「改了契约忘生成」挡在合并前——§3 既有纪律的执行体）
⑤ 发版打包       CI（main 合并）：protoc（vcpkg pin）定型 → 组包
   dist/client/<client_hash>/（manifest+bin+semantic+可选代码，§11.6）
   + internal 侧部署件（bin+contract.lua）→ 上传制品库/CDN 版本目录
⑥ 服务端装载     部署脚本换 bin + contract.lua
   → 装载期一致性闸（§10.6 v3）→ N/N-1 窗口灰度（§6）
⑦ 客户端更新     热更管线拉 dist/client/<client_hash>/（manifest 校验）
   或代码热更（HybridCLR 程序集/TS 脚本）
```

| 步骤 | 责任人 | 工具 | 失败挡点 |
|---|---|---|---|
| ① 编辑 | 服务端开发者 | VSCode + XSD 关联 | 编辑时 |
| ② 生成 | 同上 | apollo_gen（CMake 目标） | 四层漏斗 |
| ③ 提交 | 同上 | git | —（④兜） |
| ④ PR 校验 | CI | apollo_gen 重跑 + byte-diff | 合并前 |
| ⑤ 打包 | CI | apollo_gen + protoc（pin） | 发版前 |
| ⑥ 装载 | 运维/部署脚本 | 一致性闸 | 服务端启动期 |
| ⑦ 拉包 | 端插件 | manifest 校验 | 客户端加载期 |

前端同学零工具链（§10.3 v3 分发纪律）：⑦ 只做导入，①-⑤ 永远发生在服务端仓库——「编译/生成软件」的分发问题在流程上不存在。

### 12.2 CI/CD 工具（P2 提供）

| Job | 触发 | 动作 | 挡什么 |
|---|---|---|---|
| `apollo_gen_golden_check` | 每个 PR | 重跑生成器，byte-diff 全部 golden | 改契约忘生成/手改产物 |
| `contract_pack` | main 合并 | protoc 定型 → 组 `dist/client/<client_hash>/` 包（manifest+逐文件 SHA-256）→ 传制品库/CDN | 发版件一致性——CI 即 manifest 的**签发者**（同批性的机器物证） |
| `schema_hash_report` | PR 含契约变更时 | PR 评论：hash N→N+1、变更域（client/internal）、影响的消息/属性清单 | 评审可见性——这个 PR 动没动客户端契约一眼可见 |
| `compat_window_check`（P3） | 发版前 | 对 N/N-1 两 hash 各起最小实例跑契约回放测试 | 新契约破坏旧灰度窗口（§6 语义） |

落地注记（P2 已交付 `contract_pack`/`schema_hash_report` 两 job，2026-09-29）：`scripts/ci/contract_pack.sh` + workflow `.github/workflows/contract.yml`——从**已提交 golden** 组包（不重建：闸二已保证 golden 与源一致），双包齐发：`dist/client/<client_hash>/`（manifest+descriptor.bin+semantic.json）与 `dist/server/<schema_hash>/`（manifest+descriptor_full.bin+contract.lua+contract_route.json——§10.6 v3 装载期三件互检的部署单元）；manifest 携带域 hash + 生成器 + **protoc 版本** + 逐文件 SHA-256（不带 contract_version——携带面纪律 §11.6/§11.3 ②）；protoc pin 29.3（换版本 = 换包重签，版本入 manifest 可追）。`scripts/ci/schema_hash_report.sh`——读两 ref 已提交 golden 出三 hash 对照/变更域判定/消息属性集合 diff，PR 评论幂等（bot 评论改同一条）；两脚本 CI 与本地同一份逻辑，本机可复验。包级不变性已实测：internal-only 变更 + version bump 后客户端包三文件（含 manifest）逐字节不变、服务端包变。

### 12.3 编辑器集成：三层递进（按成本分档）

| 层 | 提供 | 自研成本 | 批次 |
|---|---|---|---|
| **零成本层** | VSCode XML 扩展（Red Hat）+ 契约文件头 `xsi:noNamespaceSchemaLocation="apollo.xsd"`（+ 仓库内 `.vscode` 关联配置）→ 实时 XSD 校验、标签/属性补全、enumeration 提示 | **零**——标准 XML Schema 机制 | P1 随契约 v1 即可用 |
| **脚手架层** | 仓库内 `tasks.json`：「生成契约」一键跑 apollo_gen；「契约变更预览」展示本次 diff 对应的产物/hash/域变化（读 apollo_gen 的 JSON 输出渲染） | 配置文件，无扩展开发 | P2 尾 |
| **插件层** | VSCode 扩展：契约树视图（attrs/messages/entities 分域浏览）、id↔生成代码/文档跳转、变更影响分析（属性被哪些消息引用）、紧凑风格 snippet（§2.3）、右键重新生成 | 扩展开发（TS）——**只调 apollo_gen 二进制/读其产物 JSON，不解析契约** | P3 按需（脚手架层不够用时） |

插件层纪律再强调：扩展是 apollo_gen 的**视图**，不是第二个解析器——全部语义来自生成器产物，「单一事实源」在工具侧不破。

### 12.4 落地批次汇总

- **P2**：`apollo_gen_golden_check`（升级为全量产物 byte-diff）+ `contract_pack` + `schema_hash_report` + VSCode 脚手架层（tasks.json）。（前三已交付：golden 闸走 ctest `apollo_gen_golden_check`（ci.yml 矩阵）；contract_pack/schema_hash_report 走 `contract.yml` + `scripts/ci/`；脚手架层随后续批。）
- **P3**：`compat_window_check` + VSCode 插件层（按需）+ apollo_gen 预编译单二进制分发（本地快速迭代进阶，§10.3）。

---

*基线：apollo main @ 35a9c528（`sdks/`、`skds/` 读码，git log 38656f90 目录改名记录）；KBEngine 参照其公开文档的 .def/生成器/SDK 结构（非源码评审）。2026-09-28 同步修订（①⑤）：契约形态 TOML → XML+XSD（architecture-review §15.3/§15.4）、.def 表述按 C-50 修正（改以 architecture-review §16 源码证据为准：BigWorld entity_description.cpp:184-190、KBEngine entitydef.cpp:188-210，两工作副本为浅克隆/官方包）；行号基线仍为 35a9c528。2026-09-29 追加 §10（契约→protobuf 编码：两层模型/映射规则/proto 后端/差分配合/zstd 边界/三路线权衡——编译期生成 vs descriptor.bin 反射池 vs 运行时解释，含各运行时动态加载 API 查证与 C++ DescriptorPool 换代注意点；**定案 A+C 双轨、C 为主**：A 收窄服务端热路径，C 为默认通道走客户端热更资源管线，生成器同批吐 .pb.cc 与 descriptor.bin，schema_hash 两态一致）——引用 attribute-sync §3/§5/§7、net-abstraction §3/§5.5、architecture-review §16.7.2、contract_model.hpp:19-27 掩码位（@ a5334014 实读）、sdks/contract v1 全部契约文件（@ a5334014）；§8 P2 分期同步扩为 proto/bin 双后端行；决策同步 docs/36 决策追溯表 #19。同日再追加 §11（内外契约分域：KBE `baseapp_interface.h` 三类受众混布/EXPOSED 仅标记/单一 ID 分配表/importClientMessages 运行时协商——实证登记；BigWorld Mercury `InterfaceMinder` 单表顺序分配 + `ExposedMethodMessageRange` 同表保留段——同病异形对照；规避 = 域分段四条硬规则（ID 按域分段/双 hash/按域过滤/跨域引用禁令），物理组织三形态只记事实不设强制、include 聚合为生成器读取层能力）——KBE/BigWorld 行号对应各自浅克隆/官方包工作副本本会话实读；§8 P2 补分域行；docs/36 §2.2 失败教训补 ④。同日续加 §11.6（客户端契约包与 manifest：bin 只覆盖 wire 层的补漏——语义小件第二文件是事实，交付单元升为包，manifest 逐文件 SHA-256 + 单点 client_hash 握手，不一致全闭环在加载期/握手期；单文件三案否决）。同日定案修正 v3（**反射为默认、代码为两端增强**，替代首版「A+C 双轨、C 为主」）：修正触发=端侧代码热更通道（HybridCLR/TS 脚本）使 A 发版代价消失 + 服务端零重编诉求使 .pb.cc 投影退役；核心=消息两分法（框架固定消息族 schema 不随契约变→内建强类型守热路径；业务消息→bin 反射+sol2 桥+contract.lua，服务端契约变更零重编）、客户端按热更能力分档（生成代码主通道、bin 兜底）、装载期一致性闸（bin↔internal_hash 锚定/bin↔Lua 路由逐条对齐/白名单存在性——严谨性从编译期改装载期承接）；§10.3 补 v3 后端与工具链分工（无自建 AST——pugixml DOM 即解析树、IR+薄 writer；前端零工具链、protoc CI 单点、contract.lua 为 IR 的 Lua 表 dump 无编译环节）；§10.7/§8 P2/§11.6 包结构（代码文件进 manifest）/docs/36 #19 行同步改口径。同日加 §12（契约工具链与工作流：端到端七步流程——编辑/生成/提交/PR 校验/发版打包/服务端装载/客户端拉包，每步责任人+工具+失败挡点，前端零工具链在流程上成立；CI/CD 四 job——golden_check/contract_pack（CI 即 manifest 签发者）/schema_hash_report/compat_window_check；VSCode 集成三层递进——零成本 XSD 关联（P1 已可用）/脚手架 tasks.json（P2 尾）/插件层（P3 按需，只调 apollo_gen 读产物、不做第二个契约解析实现））；§8 P2/P3 行同步。同日补 §10.6 附节（业务消息解码位置两案：案一 C++ 反射+sol2 桥**默认**——sol2 无动态绑定能力，桥为自写固定通用转换（descriptor 数据驱动循环）；案二字节透传+Lua 侧 lua-protobuf 备选——C++ 只帧路由 payload 透传；对照维度=解码位置/C++ 消息面可见性/自研代码/依赖/性能/一致性闸执行者；**契约零分叉**——binding 不编码解码位置（装配选择故意不进契约）、两案同源同批同 bin 格式、新增共用产物 contract_route 路由清单（§10.3 已补）；切换条件=确认 C++ 永不需要消息内容时换装配、契约与 golden 零改动）；docs/36 #19 理由列同步。同日补性能量化与混布（案二少一次中间表示=1.5-2× 常数因子——两次转换 vs 一次转换；25k 条/秒基准下两案各占帧预算 8%/4%，性能不构成决策依据，解码可 IO 池旁路两案同开放；装配粒度 per-进程——网关透传+Zone 解码混布，契约/route/golden 不变，「两案」为装配谱系两端；接入层过滤/调试/错误处理/测试四维精简补记）。同日 P2 第三批代码落地（§11.3 ② 双域 hash：computeDomainHash 算法同 §6 输入按域过滤——bundle 划分与 version 剥离如上文口径；§11.6 携带面纪律——semantic.json/client proto 去 schema_hash 与 contract_version 只带 client_hash，服务端五产物带全量+双域三 hash；internal-only 变更+version bump 后客户端包成员逐字节不变、服务端产物全变，本机实测通过；ctest 17/17、编译期三 hash 互异断言入 gen_compile_test）。同日第四批落 CI 工具链（§12.2：contract_pack——scripts/ci/contract_pack.sh 从已提交 golden 组双包（dist/client/<client_hash>/ + dist/server/<schema_hash>/），manifest 携带域 hash+生成器+protoc 版本+逐文件 SHA-256，protoc pin 29.3；schema_hash_report——读两 ref 已提交 golden 出三 hash/变更域/集合 diff，PR 评论幂等；workflow .github/workflows/contract.yml；包级不变性实测——internal-only 变更+version bump 后客户端包含 manifest 逐字节不变）。同日第五批落 include 聚合（§11.4 落地注记如上文：expandIncludes 目录级读取层 + XSD include 声明 + 分文件/单文件 hash 相等测试锁死；xsd_gate job 补闸一接线）。同日粘贴 ⑨（§3 链接边界段：gen 为构建期工具、运行期零链接、运行期代码禁 include gen 内部头——architecture-review §16.9.3）。*