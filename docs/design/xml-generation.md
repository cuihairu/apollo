# Apollo XML 生成与数据装载设计（xml-generation）

> 状态：设计稿（评审中）。定位：回答「框架里 XML 用在哪、从 XML 生成什么、怎么校验、怎么热更」——把 ioc-review §15.3/§15.4/§15.5 的三个 XML 决策（契约源 XML+XSD、分文件布局、storage 语句即数据）落成一条统一的生成与装载管线。与 `sdk-contract.md`（四投影与 gen/ 目录）、`attribute-sync.md`（L1 静态配置装载）、`scripting-lua.md`（热替换协议）、`net-abstraction.md`（帧 ver→filter 栈声明）、`docs/analysis/ioc-review.md` §16（行业 XML 对照）互为引用。

---

## 执行摘要

1. **现状：apollo 运行期 XML 消费为零**——全仓唯一 XML 引用是 config_manager.cpp 的 `parseXml` **恒 false 桩**（:564-570，注释自认「XML解析需要专门的库（如tinyxml2）」，与 parseLua 桩同类）；已实现解析器只有手写 INI/JSON 两套；vcpkg 无任何 XML 库。XML 的用途不是既有代码惯性，而是**本轮立规矩**：行业一致形态（BigWorld/KBEngine 全生态 XML 契约、MyBatis mapper XML）+ §15.3 已定决策。
2. **用途白名单四个、禁区四个**。白名单：① 契约源（sdks/contract/*.xml + apollo.xsd）；② L1 数值配置表（config/tables/*.xml + tables.xsd）；③ storage.xml 存储语句（§15.5 已定，手写不生成）；④ 帧管线 filter 栈声明（随契约，net-abstraction 落点）。禁区：线上协议（恒二进制）、Lua 脚本域、关卡/几何大块数据（二进制 chunk）、日志。
3. **生成源三选一，选定「XSD 预校验的 XML 实例」**：C++ 结构体反射（C++20 无稳定静态反射 + 真相反转为代码）与 Lua 表为源（丢 XSD 红利 + 脚本域数据域混淆）均否决——对照表见 §3。
4. **产物五类 + v3 契约反射后端**：强类型数据结构、typed loader（pugixml，错误带 file:line+xpath）、启动校验器、schema_hash 头、文档投影——契约源另按 sdk-contract §10.3 v3（2026-09-29）同批追加反射后端五产物（.proto/descriptor.bin/contract.lua/semantic.json/contract_route，P2，见 §4）；storage.xml 手写不生成。
5. **校验四层漏斗**：xmllint XSD 门禁（CI）→ 生成器语义规则（XSD 表达不了的：ID 分段/环检测/跨表引用）→ 生成代码编译期（static_assert/constexpr）→ 启动 fail-fast（聚合报错，一次报全不遇错即停）。原则同 §0：能生成期报的不留编译期，能编译期报的不留启动期，能启动期报的不留运行期。
6. **MyBatis 范式的取与舍**：取「XML 描述 + 语句/映射声明化 + 启动全量解析即败 + 重建式热更」；舍「运行期 ORM 一切」（会话/懒加载/二级缓存/动态 SQL/`${}` 拼接）。游戏服映射与不适配清单见 §6。

---

## 1. 现状盘点（读码结论）

### 1.1 apollo 代码 XML 消费普查（本轮 grep 实测）

| 事实 | 位置 | 判定 |
|---|---|---|
| 全仓唯一 XML 库引用（tinyxml/pugixml/rapidxml/libxml/expat/xerces） | `modules/core/config/src/config_manager.cpp` | 仅此一处 |
| `parseXml` 恒 false 桩 | config_manager.cpp:564-570 | 「声明不实现」——C-49 stub 家族成员（与 parseLua 桩 :572+ 同批） |
| 已实现解析器：手写 INI/JSON | config_manager.cpp:412/:493 | 配置主格式实为 JSON（vcpkg 唯一数据格式依赖 nlohmann-json） |
| `ConfigFormat::Xml` 路由与 .xml 扩展名分发已存在 | config_manager.cpp:115-116/:131-132/:632-633 | 接缝不补实现——路由随桩一并删除（ioc-review §16.8.3-③）：XML 装载只走本设计 §4 产物 2 的生成 typed loader，不回灌 config_manager 通用树 |
| 客户端 SDK（sdks/skds）XML 使用 | 无命中 | 客户端同样零 XML |

### 1.2 行业对照（ioc-review §16 源码证据的延伸）

| 框架 | XML 用途面 | 不用 XML 的地方 |
|---|---|---|
| BigWorld | 实体 .def（XMLSection/BWResource，entity_description.cpp:184-190）、资源与配置生态 | 线上协议（Mercury 二进制 Bundle）、空间/几何数据（二进制 chunk） |
| KBEngine | entities.xml + entity_defs/*.def（tinyxml2，entitydef.cpp:188-210）、服务端配置 kbengine_defaults.xml | 协议二进制、navmesh 二进制 |
| skynet | **零 XML**（反例：sproto schema + Lua config）——其无版本/无校验的 schema 正是 §15.3 否决路径 | 一切 |
| MyBatis | mapper XML（语句/映射声明化，§6 详） | 运行期数据本身（在 DB） |

结论：**「契约/配置用 XML、热路径数据用二进制」是 MMO 服务端的主流形态**；apollo 零存量包袱，直接按此立规矩，无迁移成本。

## 2. XML 用途界定：白名单与禁区

| # | 用途 | 文件 | 谁写 | 谁读 | 变更频率 | 校验 |
|---|---|---|---|---|---|---|
| ① | 契约源 | `sdks/contract/{apollo.xsd, attrs.xml, messages.xml, entities.xml, errors.xml, version}`（§15.4 布局） | 程序（messages/errors）+ 策划（attrs/entities） | 生成器 + xmllint | attrs 高频、messages 低频 | apollo.xsd + 生成器规则 |
| ② | L1 数值配置表 | `config/tables/*.xml + tables.xsd`（升级曲线/怪物参数/buff 数值等，attribute-sync §2.1 的 L1） | 策划工具导出（Excel/CSV → 导出器 → XML，导出器属工具链不在本设计内，接口只有 XML+XSD） | 服务端启动装载器 | 高频（热更） | tables.xsd + 启动校验 |
| ③ | 存储语句 | 服务端私有 `storage.xml`（表/列提升/journal 语句，§15.5） | 程序 | DB 执行器 | 低频 | storage.xsd（keyref 锁回契约 attr id） |
| ④ | filter 栈声明 | 随契约 messages.xml 的通道/版本字段 | 程序 | 生成器 → L1 装配代码（net-abstraction §3 落点） | 低频 | apollo.xsd |

**禁区（写进评审红线）**：
- **线上协议恒二进制**——XML 只是契约源与配置源，生成器产物才上线上（§15.3 不变项）。
- **Lua 脚本域不进 XML**——公式/行为是代码（scripting-lua 边界），配置表只承载数值。
- **关卡/几何大块数据不进 XML**——二进制 chunk + 索引（行业一致），XML 只存引用元数据。
- **日志/BI 数据不进 XML**——结构化文本/列式采集（attribute-sync §8.2 BI 分流）。

## 3. 生成源决策：三选一

| 候选 | 评估 | 判定 |
|---|---|---|
| **XSD 预校验的 XML 实例** | 语言中立（C++/C#/TS 三端同源）；XSD 四红利现成（§15.3：key 唯一/keyref 引用/enumeration/零校验器开发）；策划可读可 diff；与 BigWorld/KBEngine .def 同构（C-50 修正后的血统） | **选定** |
| C++ 结构体反射 | C++20 无稳定静态反射（26 才落地）；真相反转——代码成源、契约退化为生成物，多端投影失去语言中立源；改一个属性名要动头文件+重编译全端 | 否决 |
| Lua 表为源 | 丢 XSD 红利（Lua 无 schema 层）；脚本域与数据域混淆（scripting-lua §1 边界：属性值存储永不进 Lua）；策划手改 Lua 表 = 无校验直上 | 否决 |

边界说明：**生成器不消费 XSD 本身**——XSD 定义结构规则（校验用），生成器的类型/语义输入是 XML 实例 + 每类文件的生成规则；XSD 变更（加枚举值）需要同步生成器规则，两者一致性由 ② 层生成器断言兜底（§5）。

## 4. 生成器：输入/产物/实现

- **实现形态**：C++ 单二进制 `sdks/gen/`（sdk-contract §3 已定，入 CI）；XML 解析用 **pugixml**（vcpkg 新增一项，§15.3 已定）；**XSD 校验不在生成器内做**——pugixml 无 XSD 能力，也不值得为此引 libxml2，CI 门禁前置 `xmllint --schema`（§15.3「校验器零开发」）；**include 聚合在读取层自实现**（sdk-contract §11.4：pugixml 不内建 XInclude，读根文件递归展开引用为单一文档树，XSD 校验与 schema_hash 一律对聚合后整体——零新依赖）。链接边界（ioc-review §16.8.3-②）：gen 运行期零链接——产物被消费模块链接，生成器本体不进任何运行期目标（sdk-contract §3 同步此边界）。
- **契约反射后端（v3，sdk-contract §10.3/§10.6 定案，P2）**：契约源同批追加五类反射产物——`.proto`（golden，protoc 的源）→ protoc 出 `descriptor.bin`；`contract.lua`（IR 的 Lua 表 dump：attr 表/消息路由/白名单，服务端 require 即用）；`semantic.json`（客户端语义小件）；`contract_route`（C++ 帧路由清单：msg id→name/dir/domain/handler）；端侧代码包装（protoc `--csharp_out`/pbjs，CI 单点吐 golden 入库）。生成语义按 `binding="native|reflect"`（sdk-contract §10.6 v3 消息两分法）：native（框架固定消息族）进内建强类型代码，reflect 只进 bin/路由。实现口径：**无自建 AST**——pugixml DOM 即解析树（选 XML 的直接红利），核心资产是 IR（ContractModel）+ 四层漏斗，各产物皆「读 IR→渲染文本」薄 writer；业务消息 `.pb.cc` 不默认产出。**首批已落地（2026-09-29，apollo-gen 0.2.0 + 契约 v2）**：messages `domain`（必填）/`binding`（缺省按通道）属性、域分段（client 1-899 / internal 900+，段值就此定值）与跨域引用禁令进解析器第 ② 层；`contract.lua`/`semantic.json`/`contract_route.json` 三产物随 .h/.json 同批吐出并进 golden 闸（Lua handler 名约定 = 消息名）。**`.proto` 双投影同日第二批落地**：`apollo_contract.proto`（全量，服务端 bin 源）+ `apollo_contract_client.proto`（client 域投影，客户端 bin 源）——§10.2.2 类型映射（sint32/sint64 zigzag、alias 零特判、列表 repeated）、MsgId 枚举（MSG_ 前缀——protobuf 枚举值是包级兄弟作用域）、字段号 1..N 声明序；本机 protoc `--descriptor_set_out` 验证通过（descriptor.bin 仍按纪律由 CI 单点定型，不入开发者环境）。**双域 hash 第三批落地**：`computeDomainHash`（算法同 §6，输入按域过滤——client 面 = client 域消息 + attrs + errors，internal 面 = internal 域消息 + entities；**手工 version 不进域 bundle**，否则 internal-only 变更也推动 client_hash）；产物携带面分档——服务端五产物带 schema_hash + 双域 hash，**客户端包成员（semantic.json/client proto）只带 client_hash**、去 schema_hash 与 contract_version（两者都被 internal-only 变更推动，会破坏 `dist/client/<client_hash>/` 目录内文件字节不变性，§11.6）；internal-only 变更 + version bump 后客户端包成员逐字节不变、服务端产物全变——本机实测。端代码包装与装载期一致性闸随后续批。
- **产物（五类）**：
  1. **强类型数据结构**：contract → `enum class AttrId`/`enum class MsgId` + constexpr 默认值表（attribute-sync §2 的契约机器可读形态）；tables → 每表一个 struct（字段类型按 tables.xsd 类型集定标：int64 万分比/字符串/数组）。
  2. **typed loader**：pugixml DOM → struct 的生成装载函数；错误带 `file:line + xpath + 期望/实际`（pugixml 的 node.offset_debug 可得行号）——禁止通用 ConfigNode 树二次反射（热路径类型安全在编译期定型）。
  3. **启动校验器**：keyref/范围/跨表引用/派生 DAG 拓扑（16.7.2 的环检测落点）；**聚合报错**——收集全部错误一次输出（策划工具友好），非遇错即停。
  4. **schema_hash 头**：SHA-256(契约源+生成器版本)（sdk-contract §6 握手不变）。
  5. **文档投影**：协议文档/错误码表（sdk-contract §3 产物 ④）。
- **明确不生成**：storage.xml（手写 + storage.xsd 校验，§15.5——语句是程序的知识不是生成的对象）；C#/TS SDK 投影细节（sdk-contract §10.3 v3 同一生成器排期——protoc/pbjs 现成工具吐代码，本文不重复）。
- **CI 双闸**（sdk-contract §3 已定，此处给实现口径）：闸一 `xmllint --schema` 全量 XML；闸二生成产物 diff 检查（改契约忘生成 → 红）。生成器自查规则即 §5 第 ② 层。两闸均已接线（2026-09-29）：闸一 = workflow `contract.yml` 的 `xsd_gate` job（目录内任何 *.xml 含 include 分片一律过 apollo.xsd——分片自身是完整根元素，独立可验）；闸二 = ctest `apollo_gen_golden_check`（七产物 byte-diff，随 ci.yml 矩阵跑）。
- **include 聚合已落地（§11.4，同日第五批）**：`expandIncludes` 进目录级读取层——递归展开根直接子级 `<include href>` 为单一文档树（深度优先；环/缺文件/根元素不匹配/version 不一致/缺 href/未知属性/携带子元素各有专属诊断；深层 include 不展开由 strictWalk 白名单拦；重复 include 允许拼接、重复 key 由既有规则拦）。XSD 四根加 include 声明（位置锁根子级开头，主子元素放宽 minOccurs=0——纯 include 根合法）；**分文件目录与单文件目录解析出的模型与 schema_hash 完全相等**（测试锁死——物理分文件不影响 hash 稳定性）。单文件 parseXxxXml 不做展开（无文件系统上下文，include 是目录级读取层能力）。

## 5. 校验策略：四层漏斗

| 层 | 时机 | 工具 | 拦什么 |
|---|---|---|---|
| ① XSD 门禁 | CI / 编辑器实时 | xmllint --schema（IDE 免费补全） | 结构合法性：xs:key（attr/msg id 唯一——C-35 分段冲突无法入库）、xs:keyref（派生 DAG 悬垂引用、storage 列提升引用、16.7.2 的 parent 引用）、xs:enumeration（域/通道/所有权拼错） |
| ② 生成器规则 | CI（闸二前半） | 生成器断言 | **XSD 表达不了的语义**：ID 分段不重叠、派生 DAG 环检测（两家 .def 解析器都没做、16.7.2 实证必须自己写）、跨表引用存在性、同名改型拒绝（Avatar←Monster 出处链）、XSD 与生成器规则的版本一致性 |
| ③ 编译期 | 构建 | static_assert / constexpr | 枚举界、默认值表完整性（constexpr 数组按 enum 索引，漏项即编译错）、schema_hash 常量与头文件同步 |
| ④ 启动 fail-fast | 服务启动 | 启动校验器 | 数据行范围/单位定标、跨表引用、契约-配置版本匹配；**聚合报错**后拒绝启动 |

顺序即原则：错误暴露层级的下移都是成本翻倍（② 的生成错误打印即可修，④ 的启动失败要等部署现场）。

## 6. MyBatis 参考调研：游戏服语境的映射与不适配

### 6.1 MyBatis 范式速览

mapper XML 三件套：**语句声明化**（namespace + 语句 id + SQL，`#{}` 预编译参数 vs `${}` 字符串拼接）、**resultMap 映射声明化**（列↔字段按名绑定 + 类型转换声明，association/collection 处理关联）、**接口绑定**（namespace=Mapper 接口全名、语句 id=方法名，运行期动态代理到语句）。框架行为：SqlSessionFactory 构建期**全量解析** mapper XML——重复语句 id、坏语句、引用缺失**启动即败**；二级缓存 `<cache/>` namespace 粒度默认关闭；官方**不支持运行期热换 mapper**（只能重建 SqlSessionFactory 整体替换）。

### 6.2 游戏服映射（对齐点）

| MyBatis 概念 | apollo 落点 | 状态 |
|---|---|---|
| 语句即数据（mapper XML） | storage.xml mapped statement（§15.5 已定：三条 write-behind 语句 + 列提升读写，按名绑定、启动校验） | 已采纳 |
| 启动全量解析即败 | 四层漏斗的 ②/④（生成器断言 + 启动校验 fail-fast）——「能启动期报的错不留到运行期」同一哲学 | 已采纳 |
| resultMap 列↔字段映射声明 | **配置表装载**：tables.xsd 声明列（字段名/类型/缺省），生成器产 row↔struct 按名绑定装载代码——类型转换在声明层（万分比定标/枚举串→enum class），不做运行期反射猜测 | 本设计采纳（§4 产物 2） |
| 重建式热更（rebuild SqlSessionFactory） | **配置表热更管线**：FileWatcher/GM 触发 → 工作线程全量重解析+全漏斗校验 → tick 边界原子换表 → 失败保旧表+告警（scripting-lua §3.2 换表协议同构；§3.3 版本偏序联动）。类比成立的点：**不是增量改表对象，而是整体重建再换**——MyBatis 官方只给重建一条路，恰恰是游戏服要的形态 | 本设计采纳 |
| `#{}` 预编译 vs `${}` 拼接 | 唯一绑定路径（按名绑定+参数化）——C-44 实测缺陷簇（不转义/条件转义/静默默认）在「唯一路径」下结构性消失（§15.5 已论） | 已采纳 |

### 6.3 不适配点清单（游戏服不是 ORM 语境）

| MyBatis 机制 | 不引入理由 |
|---|---|
| SqlSession/一级缓存/identity map | 内存权威模型（attribute-sync §3）：进程内属性即真相，无需会话级身份映射 |
| 懒加载 / N+1 | 表装载是启动期一次全量 + 热更全量重建，无逐行惰性取数场景 |
| 二级缓存 `<cache/>` | 破坏单写者与 write-behind 纪律（§15.5 已拒，Redis 层同理 attribute-sync §8.2） |
| 动态 SQL（`<if>/<foreach>` 运行期解释器） | 语句面小而稳（§15.5 已拒）：迷你语言解释器是负资产 |
| `${}` 字符串拼接 | 注入面，反面教材——apollo 侧唯一 SQL 生成点（storage 语句参数）强制参数化 |
| Mapper 动态代理 | C++ 无此机制也无需：生成期直接产强类型函数（接口绑定思想保留——「按名找语句」收敛为「按名生成的函数」，名字错在编译期红） |

## 7. 分期

- **P1**：契约生成器 v1（contract/*.xml → 产物 1-4 + 文档投影；CI 双闸 xmllint+diff 上线）+ tables 装载器 v1（tables.xsd + 生成 loader + 启动校验）+ config_manager 的 parseXml/parseLua 桩与 ConfigFormat::Xml/Lua 路由**诚实化删除**（stub 家族清理，C-49 纪律；ioc-review §16.8.3-③：删除含路由本身、不补通用 XML 实现——config_manager 保持 INI/JSON 通用树职责，XML 装载只走本设计产物 2；改动点记录在案，随代码阶段执行）。
- **P2**：配置表热更管线（FileWatcher → 全量重建 → tick 边界换表）+ **契约反射后端**（sdk-contract §10.3 v3：.proto/bin/contract.lua/semantic.json/contract_route 五产物 + binding 生成语义 + include 聚合展开；**前三批已交付**——domain/binding+域分段校验+contract.lua/semantic.json/contract_route.json+.proto 双投影+双域 hash（client_hash/internal_hash），见 §4 落地注记）+ 文档投影接 sdk-contract 排期（端代码包装）+ 帧管线 filter 栈生成（net-abstraction P1 会话层联动）。
- **P3**：storage 投影工具化（storage.xml 的启动校验器复用四层漏斗）+ 体积敏感表的二进制打包选项（生成期打包 + schema 版本，运行期零 XML 解析）。

## 8. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| sdk-contract.md | 本设计 = 其 gen/ 生成器的内部设计（同一二进制、同一 CI 双闸）；四投影与目录布局、schema_hash 握手全部沿用不重复定义 |
| attribute-sync.md | §2.1 L1 静态配置的装载与热更即本设计用途 ②；契约 attrs.xml 即用途 ① 的核心实例 |
| scripting-lua.md | 热更走同一换表协议（§3.2 三阶段）；配置热更与脚本热更的版本偏序（§3.3）；「数据不进脚本」边界是 §3 生成源否决 Lua 表的同一条线 |
| net-abstraction.md | 帧头 ver 字段 → filter 栈声明（16.7.1）；契约 channels/enumeration 由 §5 第①层校验 |
| ssengine-reference.md | utils/config/FileWatcher（inotify/poll）为热更触发器移植件；「头文件注释撒谎」教训 ↔ config_manager 桩的诚实化 |
| ioc-review.md | §15.3/15.4/15.5 决策源；§16 行业 XML 对照（C-50 .def=XML 修正后本设计与两家先例同构）；C-49 stub 家族 → §7 P1 的桩清理项 |

---

*基线：apollo main @ 047d0002（modules/core/config 读码；行业对照行号见 ioc-review §16.2/§16.7，其框架基线在各自行注明）。本设计与 ioc-review §16.7 随同一次提交落盘。2026-09-29 同步 sdk-contract v3 定案（§10.3/§10.6/§11.4：契约反射后端五产物、binding native|reflect 生成语义、include 聚合读取层展开）——执行摘要 4/§4/§7 P2 对应扩写。同日 P2 反射后端首批代码落地（apollo-gen 0.2.0 + 契约 v2：messages domain/binding 属性、域分段 client 1-899/internal 900+ 定值、跨域引用禁令进解析器、contract.lua/semantic.json/contract_route.json 三产物进 golden 闸；luac/lua5.4 装载验证通过）；同批续落 `.proto` 双投影（apollo_contract.proto 全量 + apollo_contract_client.proto client 域投影，§10.2.2 类型映射/MsgId 枚举 MSG_ 前缀/字段号声明序，protoc --descriptor_set_out 本机验证通过——descriptor.bin 按 CI 单点纪律不入库）——§4/§7 P2 加落地注记；端代码包装/装载期闸随后续批。同日第三批落双域 hash（§11.3 ②：computeDomainHash 输入按域过滤——client 面=client 域消息+attrs+errors、internal 面=internal 域消息+entities、手工 version 不进域 bundle；携带面分档——客户端包成员只带 client_hash，semantic.json/client proto 去 schema_hash 与 contract_version；internal-only 变更+version bump 后客户端包成员逐字节不变实测通过；ctest 17/17 + 编译期三 hash 互异断言）。同日第四批落 CI 工具链两 job（sdk-contract §12.2：scripts/ci/contract_pack.sh 组 dist 双包 + manifest 签发（protoc pin 入 manifest）、scripts/ci/schema_hash_report.sh 出契约变更报告，workflow contract.yml——两脚本 CI 与本地同一份逻辑；包级不变性实测：internal-only 变更+version bump 后客户端包含 manifest 逐字节不变）。同日第五批落 include 聚合（§11.4：expandIncludes 进目录级读取层 + XSD 四根 include 声明 + xsd_gate job 补闸一接线——分文件与单文件 hash 相等测试锁死）。同日粘贴 ⑦⑨（ioc-review §16.9.1/§16.9.3：§1.1 表行改「路由随桩删除」口径、§7 P1 补删除范围与不补通用 XML 实现声明、§4 补 gen 运行期零链接边界）。*
