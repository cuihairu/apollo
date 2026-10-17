# MMO 机制源码分析

本文比较传输、实体、脚本、持久化、模拟、可见性、生命周期、运维与身份机制。外部实现不定义 Apollo 的进程拓扑或功能承诺。

## 0. 证据与源码基线

A 级使用源码路径与行号，B 级使用仓库文档，C 级使用外部官方资料，D 级为需注明范围的社区材料。

取证基线为 BigWorld `27446bab`（14.4.1）、KBEngine `0bc93d5` 和 skynet `4f76d75`。引用描述这些源码副本，不代表所有版本；缺失判断仅适用于明确的检索范围。出处保留真实文件与行号，不依赖已经移除的评审章节。

Apollo 当前边界见[架构现状](./architecture-review.md)、[SDK 契约](../design/sdk-contract.md)与[跨服场景](../design/cross-server-scenes.md)。

---

## §1 网络分层与帧管线

**实现位置（A 级，基线源码）**

| 机制 | BigWorld（Mercury） | skynet |
|---|---|---|
| 事件多路复用 | `lib/network/event_poller.hpp:95-137` 抽象基类（doRegisterForRead/Write + processPendingEvents 纯虚）；**唯一后端 select**：`lib/network/event_poller.cpp:496` `select( fdLargest_+1, … )` | socket 线程独占 epoll：`skynet-src/socket_epoll.h:20`（kqueue 变体 socket_kqueue.h 并存） |
| 帧定界 | Bundle 分片：`lib/network/udp_bundle.cpp:567` | gate 长度头：`service-src/service_gate.c:353`（sscanf 启动参数）/:362（header 只许 `'S'`/`'L'`）；2B 路径大端读长 `lualib-src/lua-netpack.c:184-185`（`buffer[0]<<8 \| buffer[1]`） |
| filter 插件位 | `lib/network/udp_channel.hpp:81` 构造注入 `PacketFilterPtr`、`:139-140` 存取；双族接口 `packet_filter.hpp:37/:48/:56`、`stream_filter.hpp:46/:55` | 无 filter 层——WS 帧定界在 Lua 服务组装层（`lualib/http/websocket.lua`，源码基线 已核） |
| 双协议 | `lib/network/channel.hpp:104` `isTCP()` | TCP only（gate） |

**数据结构与关键字段（A 级）**
- skynet gate（`service_gate.c:15-35` 基线源码）：`struct connection { int id; uint32_t agent; uint32_t client; char remote_name[32]; struct databuffer buffer; }`；`struct gate { ctx, listen_id, watchdog, broker, client_tag, header_size, max_connection, struct hashid hash, struct connection *conn, messagepool }`——**连接表是数组 + hashid 索引**，每个连接一个流式 `databuffer`。
- BW 通道生命周期数据结构：`lib/network/condemned_channels.hpp` / `delayed_channels.hpp`（目录级证据，基线目录检索）——关闭中的通道独立成表，收尾期不受新建通道影响。
- BW UDP 窗口：`lib/network/udp_channel.cpp:92-108` 滑窗 / `:866` 超窗即停 / `:1052` 起重发驱动。

**状态机/时序**
- skynet gate 收包路径：socket 线程收到 fd 数据 → gate 服务消息 → netpack `filter`（`lua-netpack.c:191-241` 基线源码 uncomplete 逻辑：半包存 `struct uncomplete`，凑齐 `pack_size` 才发）→ 按 conn 的 agent/client 转发（`service_gate.c:169` `_forward`）。
- BW Mercury 主循环：`EventDispatcher::processNetwork`（`lib/network/event_dispatcher.cpp:366-374` 基线源码）→ `pPoller_->processPendingEvents( maxWait )`——就绪通知 + 定时器最早期限作为最大等待。

**配置参数默认值**
- skynet：`skynet-src/skynet_main.c:160` `optint("thread",8)`——worker 线程默认 8（`:161-162` 上限 SKYNET_MAXTHREAD 校验）；gate `max_connection` 启动参数必填（`service_gate.c:356-360`，无默认）。
- BW：poller 无配置项（编译期选定 select）；通道窗口等参数见 源码基线 已核各条。

**已知缺陷**
- BW select-only：fd 数量受 FD_SETSIZE 限制，扩展即换后端；此处只说明已读基线，不推断后续上游演进。
- skynet gate：帧定界（2B/4B）与消息体（sproto）两层耦合在 netpack/gate 组装里，不像 BW 有统一 filter 槽位——加封装要改 gate/netpack 本体。
可分别研究 filter 注入位置与 Mercury 的帧格式、传输绑定，不将两者作为同一项架构要求。

---

## §2 进程间消息层的代际更替

**实现位置（A 级，基线源码）**
- harbor（第一代，C 服务）：`skynet-src/skynet_handle.c:30-40` `struct handle_storage { rwlock; uint32_t harbor; uint32_t handle_index; slot…; name… }`；`:110` `handle |= s->harbor;`——**句柄高位拼 harbor id**（harbor 预移至高位，8 位节点号，源码基线 已核 skynet_handle.c:33,111）。服务头注释 `service-src/service_harbor.c:6-14` 自述文本协议命令族：`N name`（更新全局名）/ `S fd id`（主动连新节点：先发自 id、收对方 id 校验、再发队列）/ `A fd id`（被动接受）/ `D id`（断连上报）/ `Q name`（未知全局名询问）。
- cluster（第二代，Lua 服务族）：`service/clusterd.lua:40-50`（node_sender 表 + double-check 建 clustersender）；发送走 `service/clustersender.lua:27/:29/:52` `channel:request(...)`（socketchannel 上的请求/响应）；配置装载 `clusterd.lua:88-108` loadconfig（`__` 前缀作选项）。
- 常量：`service_harbor.c:24` HASH_SIZE 4096、`:25` DEFAULT_QUEUE_SIZE 1024、`:28` HEADER_COOKIE_LENGTH 12。

**数据结构与关键字段**：harbor 侧 = handle_storage（如上）；cluster 侧 = clusterd 的 `node_sender{name→clustersender}` + clustersender 持有的 socketchannel。

**状态机/时序**：harbor 连接握手（S/A 命令，先交换 id 再清队列）→ 断连发 `D id` 文本消息给 slave。cluster：clusterd 按节点名懒建 clustersender → TCP socketchannel → request/padding 发送。

**配置参数默认值**：harbor id 由启动参数（`skynet_handle_init(int harbor, int thread)`，`skynet-src/skynet_handle.h:20`）；cluster 节点表来自 clustername 配置文件（clusterd loadconfig）。

**已知缺陷**
- 8 位 harbor = 至多 255 节点；master/slave 模式复杂（cmaster/cslave 服务族仍在 `service/` 目录）。
**源码观察**：已读基线包含两代消息实现；不能据此推断上游更替时间线。
- 本机 skynet `docs/architecture/harbor-cluster.md` 为「编写中」占位文档（基线源码 sed 头部）——**上游文档未给 harbor 弃用时间线的权威口径，如实标注查不到**；代码事实（两代并存、cluster 为 Lua 全家桶）不能替代上游时间线核验。

---

## §3 实体契约声明（.def 解析）

**实现位置（A 级）**
- BW：`lib/entitydef/entity_description.cpp:184-190`（BWResource::openSection 解析 .def）、`:194-203`（`<Parent>` 解析期递归展开，先父后子）、`:406`（`volatileInfo_.parse( pSection->openSection( "Volatile") )`，基线源码）；字段旗标解析 `lib/entitydef/data_description.cpp:220`（Persistent）/`:228-233`（Identifier 隐含 Indexed+Unique）/`:238-250`（Indexed）/`:314`（DatabaseLength）。样例 `examples/client_integration/python/simple/res/scripts/entity_defs/Guard.def:2-6`（基线源码）：
  ```xml
  <Volatile>
      <position/>
      <yaw/>
      <pitch>	20	</pitch>
  </Volatile>
  ```
- KBE：`kbe/src/lib/entitydef/entitydef.cpp:188-210`（tinyxml2 先 entities.xml 后逐实体 .def；源码基线 已核，本轮抽检一致）；DetailLevels 解析 `:409-417`（NEAR/MEDIUM/FAR 各 radius/hyst 子节点，基线源码）。

**数据结构与关键字段（A 级，基线源码）**
- KBE `kbe/src/lib/entitydef/property.h:23` `class PropertyDescription : public RefCountable`——**构造参数即 .def 属性字段全集**：`dataTypeName, name, uint32 flags, bool isPersistent, bool isIdentifier, std::string indexType, uint32 databaseLength, std::string defaultStr, DETAIL_TYPE detailLevel`（:27-40 区域，含 getter 族 isPersistent()/getFlags() 等）。
- BW `lib/entitydef/volatile_info.hpp:13-47` `class VolatileInfo { float positionPriority_, yawPriority_, pitchPriority_, rollPriority_; }`——per 轴优先级浮点，`shouldSendPosition() = positionPriority_ > 0`（:23），静态常量 `ALWAYS`（:36）。
- KBE `kbe/src/lib/entitydef/detaillevel.h:21-23`：`struct Level { float radius; float hyst; }`，默认构造 `radius(FLT_MAX), hyst(1.0f)`。

**状态机/时序**：无运行期状态机——纯启动期解析（这正是 apollo 把解析+校验前移 CI 的对照面）。

**配置参数默认值**：BW VolatileInfo 默认 -1（不发）；KBE Level 默认 radius=FLT_MAX/hyst=1.0（如上）。

**已知缺陷**
- **两家都只解析、无 schema 层**（C-50）：错拼标签静默缺省——tinyxml2 无校验（entitydef.cpp:193 直证）、BW XMLSection 同（源码基线）。
- 继承无环守卫：BW `<Parent>` 无环检测（源码基线：rg cycle/recursi/visited 零命中）；KBE loadParentClass↔loadDefInfo 互递归无深度守卫（entitydef.cpp:890-916/:372，源码基线 已核）。

---

## §4 生成器与工具树布局

**实现位置（A 级，基线源码目录清点）**
- KBE 源码树外工具：`kbe/tools/{server, xlsx2py}`；`kbe/tools/xlsx2py/xlsx2py/` 含 `xlsx2py.py / ExcelTool.py / config.py / functions.py / py2excel.py / itemchar.py / petchar.py / syschar.py / character.py / xlsxtool.py / xlsxError.py`——Excel → 运行期 py 配置的完整工具族（gb2312 编码头，xlsx2py.py:1 基线源码）。
- KBE SDK 模板与下发：`kbe/res/sdk_templates/{client,server}`；SDK 下发即登录链路一环：`kbe/src/server/loginapp/clientsdk_downloader.{h,cpp}`（源码基线 已核存在性，本轮复核路径）。
- KBE 运维工具进程：`kbe/src/server/tools/{bots, guiconsole, interfaces, kbcmd, logger}`。
- BW 独立工具二进制族：`server/tools/{bwmachined, bw_profile, message_logger, sync_db, snapshot_helper, consolidate_dbs, clear_auto_load, remove_db, transfer_db, bots}`（基线目录检索）——`sync_db` 即「.def 变更 → 数据库结构同步」的契约邻接工具。

**数据结构与关键字段**：xlsx2py 以 sheet 为单位产 py 模块（ExcelTool.py 装载/解析；config.py 存路径与开关）——本轮仅读文件头与目录，**未逐行核 ExcelTool 解析逻辑，如实标注**。

**状态机/时序**：N/A（构建期工具）。

**配置参数默认值**：xlsx2py 的 config.py 为用户侧可编辑配置（未展开比对，不引用具体默认值）。

**已知缺陷**：Excel→py 无 schema 校验（类型错误到运行期才炸）；BW entitydef 走运行期解析而非生成期。
**机制对照**：两家「源→工件」转换器全部位于运行期链接图之外（kbe/tools 源码树外、BW server/tools 独立二进制）——apollo sdks/gen 同型。

---

## §5 契约与存储的耦合面

**实现位置（A 级）**
- KBE：`kbe/src/lib/db_mysql/entity_table_mysql.cpp:699-758`（ARRAY/FIXED_DICT/PYTHON/ENTITYCALL/Component → FIELD_TYPE_BLOB）、`:236-300`（`<Indexed>` 属性建真实索引）、`:113`（ALTER TABLE ADD INDEX）。
- BW：`lib/db_storage_mysql/column_type.cpp:95-129`（Indexed/Persistent → MySQL 列类型映射；源码基线 已核）；其余 blob 序列化 `lib/db_storage_mysql/mappings/{blob,class,composite}_mapping.cpp`；MySQL 后端目录 `lib/db_storage_mysql/{buffered_entity_tasks.*, bw_meta_data.*, column_type.cpp, …}`、XML 后端 `lib/db_storage_xml/xml_database.cpp`（基线目录检索）。

**数据结构与关键字段**：KBE 列类型的源头旗标即 PropertyDescription 的 `isPersistent/isIdentifier/indexType/databaseLength`（§3 基线源码）——**契约字段与存储列在同一构造函数参数表里**，这就是「耦合」的实现形态。BW 同型（DataDescription 的 Persistent/Indexed 旗标，data_description.cpp 源码基线 已核）。

**状态机/时序**：启动期（或 sync_db 工具）按 .def 生成/同步表结构；运行期无迁移机制。

**配置参数默认值**：KBE `kbengine_defaults.xml` 各数据库段（本轮仅核 cellapp/baseapp 段，DB 段默认值未展开——不引用）。

**边界**：这些基线实现将持久化旗标放在 PropertyDescription/DataDescription 中。Apollo 存储契约状态见 [SDK 契约](../design/sdk-contract.md)，不能把 storage.xml 设计当成已实现的装载器。

---

## §6 备份/归档的平滑摊写

**实现位置（A 级，基线源码）**
- BW BaseApp 备份：`server/baseapp/backup_sender.hpp:52-79`（字段清单见下）+ `server/baseapp/backup_sender.cpp:102-121`（tick 主流程与小数余数平滑）；默认 `server/baseapp/baseapp_config.cpp:23` `BW_OPTION( float, backupPeriod, 10.f )`、`:84` `backupPeriodInTicks`（secondsToTicks 换算）。
- KBE Archiver：`kbe/src/server/baseapp/archiver.cpp:26-63`（基线源码，含作者中文注释的算法说明：`baseEntity的数量 * idx / tick周期 = 每次在vector中移动的一个区段`，公式落 `:39 startIndex = size * archiveIndex_ / periodInTicks` / `:43 endIndex`）；销毁即落库 `kbe/src/server/baseapp/entity.cpp:698-731`（onDestroyEntity → 组 DbmgrInterface::removeEntity Bundle 发 dbmgr）。
- KBE 写路径：SQL 语句按表注册成映射表 `kbe/src/lib/db_mysql/entity_sqlstatement_mapping.h:9-27`（源码基线-② 已核）。

**数据结构与关键字段（A 级，基线源码）**
- BW `BackupSender`（backup_sender.hpp:52-79）：`int offloadPerTick_` / `float backupRemainder_`（**小数余数跨 tick 结转**）/ `BasesToBackUp basesToBackUp_` / `BackupHash entityToAppHash_, newEntityToAppHash_`（一致性哈希双表：现行表 + 切换中表）/ `bool isUsingNewBackup_, isOffloading_` / `int ticksSinceLastSuccessfulOffload_`。
- KBE `Archiver`：`arEntityIDs_`（**实体 id 随机序列**，archiver.cpp:13 初始化、:69 清空重建）+ `archiveIndex_` 游标。

**状态机/时序**
- BW：正常周期（tick → 算 numToBackUp → 按哈希表发往各 backup baseapp）→ 哈希切换期（新旧双表并用至 ackNewBackupHash）→ 退役期（isOffloading_，offloadPerTick_ 只增不减，全部迁完）。
- KBE：archiveIndex_ 每 tick +1 → 区段 [startIndex,endIndex) 内实体逐个 writeToDB → index ≥ periodInTicks 时 createArchiveTable 并重建随机序列。

**配置参数默认值**：BW backupPeriod **10 秒**（baseapp_config.cpp:23）；KBE archivePeriod **300 秒**（kbengine_defaults.xml:621）、backupPeriod **300 秒**（:625 区域，基线源码同段）。

**已知缺陷**
- KBE 归档**非脏驱动**（按实体轮询、不感知属性 dirty）；窗口 300s 默认粗。
- BW 备份是**快照流**（base 状态整发），无 journal/redo——宕机恢复粒度即备份周期（G-2 论证的参照面）。
- 两家平滑算法同型不同实现：BW 浮点余数（backup_sender.cpp:114-117 基线源码：`numToBackUpFloat = bases.size()/periodInTicks + backupRemainder_; … backupRemainder_ = 小数部分`）、KBE 区段游标.

---

## §7 按观察者记水位

**实现位置（A 级，基线源码）**
- `server/cellapp/witness.cpp:2470-2516`：dumpAoI 的**官方文档注释块**——逐字段解释输出行 `16777218: volatile 129/130. event 45/45. priority !0.000000`：观察者已收 volatile 更新 #129、实体已产 #130；事件 #45/#45；优先级 0.000000 且被 prioritised。首行 `Seen=2 AoIMap=2 AoIRadius=500.000 AoIHyst=5.000`。
- `server/cellapp/witness.cpp:1248-1258`（基线源码）：pop_heap 后重设 `pCache->priority(startingPriority)`、`updatePriority(entity_.position())`、`clearPrioritised()`——**per-(viewer,entity) 优先级堆**的维护点。
- `server/cellapp/witness.hpp:26` `class Witness : public Updatable`、`:158-161` setAoIUpdateScheme/getAoIUpdateScheme、`:275` `EntityCacheMap aoiMap_`（基线源码）。
- 客户端上报更新率：`server/cellapp/server_connection.cpp:2370-2373`、per-client 字节预算 `:2034`。

**数据结构与关键字段（A 级，基线源码 entity_cache.hpp:80-101/:184-189）**
```cpp
EventNumber     lastEventNumber_;          // int32  —— 客户端已收事件序号
VolatileNumber  lastVolatileUpdateNumber_; // uint16 —— 客户端已收 volatile 序号
EventNumber     lodEventNumbers_[ MAX_LOD_LEVELS ]; // int32 × LOD 层数
```
外加 `Priority priority()` 对（:80-81）。

**状态机/时序**：每 tick witness 从优先级堆顶取 cache → 若实体 volatile/event 序号已前进则发增量 → pop_heap 重排。

**配置参数默认值**：AoIRadius/AoIHyst per-entity 可设（dumpAoI 样例 500/5）；AoIUpdateScheme 见 §11。

**Apollo 边界**：AOI 属于 World 内的 Scene；此处不引入独立 AOI 进程。

---

## §8 定 tick 与线程编队

**实现位置（A 级）**
- BW：`lib/server/common.hpp:19` `const long DEFAULT_GAME_UPDATE_HERTZ = 10;` + `lib/server/server_app_config.cpp:28`（BW_OPTION 注册，**默认 10Hz**——基线源码）；`server/cellapp/cellapp.cpp:806-808` `addTimer(1000000/updateHertz, TIMEOUT_GAME_TICK)`、`:946-948` handleGameTickTimeSlice、`:1177-1190` updateLoad 三分统计；主循环 `lib/server/server_app.cpp:240-242` processUntilBreak。
- KBE：`kbe/res/server/kbengine_defaults.xml:5` `<gameUpdateHertz> 10 </gameUpdateHertz>`（**默认 10Hz**）；`kbe/src/server/cellapp/cellapp.cpp:251-261` handleGameTick（基线源码：`updateLoad()` 注释自述「一定要在最前面」→ `EntityApp<Entity>::handleGameTick()` → `updatables_.update()` → `SpaceMemorys::update()`）。
- skynet（反例侧）：`skynet-src/skynet_main.c:160` `optint("thread",8)`（worker 默认 8）；线程编队 monitor+timer+socket+worker `skynet-src/skynet_start.c:209-227`（源码基线 已核，weight 表本轮再读：`{-1×4, 0×4, 1×8, 2×8, 3×8}`）；权重分频 dispatch `skynet-src/skynet_server.c:316-318`、每消息重入队 `:293-315`；过载阈值 `skynet_mq.c:19` MQ_OVERLOAD 1024。

**数据结构与关键字段**：BW ScriptTimeQueue/EntityAppTimeQueue（`lib/server/entity_app.cpp:25-49` 基线源码线索位）；skynet 全局队列 + 每服务私有 mq（spinlock，skynet_mq.c:22 源码基线 已核）。

**状态机/时序**：BW/KBE = 定长 tick 定时器驱动游戏逻辑、消息到达即处理（两者并存）；skynet = 纯消息驱动无 tick、空队列 cond_wait（skynet_start.c:167-174 源码基线 已核）。

**配置参数默认值**：三家默认 10 / 10 / 8 线程（如上）。

**已知缺陷**：skynet 权重分频使重服务饥饿风险显式化（weight 0 = 每消息让出）；BW/KBE 定 tick 下消息处理与 tick 抢同一线程（time slice 分派即为此设计）。
**机制对照**：KBE 10Hz 定 tick（defaults xml:5）+ skynet 单 service 串行（skynet_server.c:293-315）——两实证对应「Apollo 主线程全序 + 池化旁路」的混合取材。

---

## §9 无缝世界与 ghost

**实现位置（A 级，基线源码）**
- BW ghost 文件族（`server/cellapp/` 目录清点）：`buffered_ghost_message{.cpp,.hpp}` / `buffered_ghost_message_factory.*` / `buffered_ghost_message_queue.*` / `buffered_ghost_messages.*` / `buffered_ghost_messages_for_entity.*` / `entity_ghost_maintainer.{hpp,cpp}` / `real_caller.*` / `real_entity.{hpp,cpp}`。
- `server/cellapp/entity_ghost_maintainer.hpp` 类注释（基线源码节选）：*"This class is used to visit all the cells of a space, and create new ghosts for those that are required for a particular entity, as well as unmark those that are still required to exist."*

**数据结构与关键字段**：ghost 消息缓存族（buffered_ghost_message_queue/for_entity）——**ghost 尚未建好时到达的方法调用先进缓冲队列，建好后补投**；real_caller/real_entity 维护「ghost→real 的调用回传」。

**状态机/时序**：实体移近 cell 边界 → EntityGhostMaintainer 遍历空间各 cell → 在需要的 cell 创建 ghost → 移远后 unmark/回收；期间对该实体的调用经 real_caller 转投权威侧。

**配置参数默认值**：本轮未定位 ghost 阈值的独立配置项（BW 以 cell 边界/AoI 半径驱动，未见独立默认值——**如实标注查不到独立参数**）。

**已知缺陷**：ghost 双写、边界协商、跨进程调试不可单步；实现侧佐证：缓冲队列族的存在本身即「跨进程一致性的复杂度税」。
**机制对照**：上述文件族全部服务于「跨 cell 视图一致性」，Apollo 副本制不需要该层。

---

## §10 AOI 结构

**实现位置（A 级，基线源码）**
- KBE 十字链：`kbe/src/server/cellapp/coordinate_node.h:94-99` 六向链存取（`pPrevX/pNextX/pPrevY/pNextY/pPrevZ/pNextZ`，:104-109 对应 setter）；`coordinate_system.{h,cpp,inl}` 维护三轴有序链；`entity_coordinate_node.{h,cpp}` 实体节点。
- BW：无独立 AOI 数据结构模块——AOI = witness 的 aoiMap_（§7）+ 触发半径（AoIRadius/AoIHyst）+ AoIUpdateScheme（§11）。

**数据结构与关键字段**：CoordinateNode（坐标 + 六向链指针，如上）；CoordinateSystem 持三轴链头。**十字链（linked-list on axes）**：移动 = O(链上定位) 更新，邻域查询 = X/Y 链上按半径截取。

**状态机/时序**：实体 add → 三轴按序插入 → move → 局部重链 + 触发邻域回调 → remove → 摘链。

**配置参数默认值**：KBE 查询半径由 DetailLevels 的 radius 提供（detaillevel.h:21 默认 FLT_MAX，见 §3/§11）。

**Apollo 边界**：AOI 属于 World 内的 Scene；此处不引入独立 AOI 进程。

---

## §11 可见域分档与条件同步

**实现位置与证据分级**
- BW（A 级，基线源码）：per 轴 volatile 优先级——`lib/entitydef/volatile_info.hpp:16-47`（四轴 float priority + `ALWAYS` 常量 + `shouldSendPosition() = priority > 0`）；.def 样例 Guard.def:2-6（`<pitch>20</pitch>` 即优先级 20）；**距离分档**：`server/cellapp/aoi_update_schemes.hpp:18-56` `class AoIUpdateScheme { float weighting_, distanceWeighting_; }`，`apply(distanceSq) = (sqrt(d)*distanceWeighting_ + 1) * weighting_`（权重 0 时按 coincident 恒 1.0）；静态 256 桶（`schemes_[256]`）+ 名字映射；**per-EntityCache 挂 scheme id**（entity_cache.hpp:98-99/:177）+ **per-LOD 事件序号数组** `lodEventNumbers_[MAX_LOD_LEVELS]`（entity_cache.hpp:189，访问器 :101/:164-165）。
- KBE（A 级，基线源码）：`kbe/src/lib/entitydef/detaillevel.h:21-23` `struct Level{radius=FLT_MAX, hyst=1.0}` + `entitydef.cpp:409-417`（DetailLevels NEAR/MEDIUM/FAR 的 radius/hyst XML 子节点）+ `detaillevel.cpp/h` 判档实现。

**数据结构**：如上（AoIUpdateScheme 双权重 / Level 半径+滞回 / LOD 序号数组）。

**状态机/时序**：BW：距离 → scheme.apply() → 优先级增量进堆（§7 的堆即消费方）——**「近密远疏」由优先级函数连续实现，不是离散档**；KBE：半径判档 + hyst 滞回防抖（进出档迟滞）。

**配置参数默认值**：BW priority 默认 -1（不发）、ALWAYS 常量、schemes 上限 256；KBE radius 默认 FLT_MAX、hyst 默认 1.0。

**范围**：此处描述 BW/KBE 基线中的可见性声明；Apollo 的契约同步规则见 [SDK 契约](../design/sdk-contract.md)。
**机制对照**：BW 的 LOD 序号数组 + scheme 权重函数 =「按距离/重要性分档同步」的成熟实现；KBE 三档半径 + 滞回 = 简单可用版。

---

## §12 脚本嵌入形态

**实现位置（A 级，基线源码）**
- KBE Python 嵌入：`kbe/src/lib/pyscript/script.cpp:76`（`Py_InitializeFromConfig`）/`:229`（`Py_Initialize` 兼容路径）——**全功能 Python、无沙盒**；`:383` 引擎自身 `PyImport_ImportModule("os")`（取系统信息用）——os 模块直接可达脚本层。
- skynet 热更：`service/debug_console.lua:159`（命令表登记 `inject`）/`:270`（`COMMAND.inject(address, filename, ...)`）；实现 `lualib/skynet/debug.lua:85-87`（`require "skynet.inject"` 后在目标服务上下文执行）——**在线向指定服务注入新代码**，官方 docs/architecture/hot-reload.md 为占位文档（基线源码，机制细节以代码为准）。
- skynet 协程观感：`lualib/skynet.lua:227` `coroutine_yield "SUSPEND"`（基线记录）。

**数据结构与关键字段**：KBE `Script` 类持 `__main__` 模块与 extraModule_（script.cpp:161/:240-243 基线源码）；skynet inject 以源文件为单位替换函数定义（debug.lua 消费 dispatch/register_protocol 引用）。

**状态机/时序**：KBE：进程启动 → Py 初始化 → 注册 KBEngine 模块 → 加载 scripts；**已读范围内未见服务端脚本热更机制**（如实标注：未在 kbe/src 检出 reload/inject 对应物）。skynet：console 下 inject 命令 → 目标服务在自己的消息循环里执行注入代码。

**配置参数默认值**：KBE python 相关配置在 kbengine_defaults.xml（entryScriptFile 等，基线 bots 段样例 `kbemain`；未逐项展开）。

**边界**：脚本能访问 os 会扩大可用能力范围；已读证据未确认完整的服务端热更流程。Apollo 模块发布见 [Lua 脚本](../design/scripting-lua.md)。

---

## §13 进程发现

**实现位置（A 级，基线源码）**
- KBE machine：`kbe/src/server/machine/machine.cpp:646-670` `findBroadcastInterface`——`bhandler.broadcast(KBE_PORT_BROADCAST_DISCOVERY)` 发广播探测 → `receive` 收应答 → 确认默认广播路由接口（逐行与 源码基线 记录一致）。
- **KBE 官方注释自认缺陷**（基线源码，`kbengine_defaults.xml:814-833` machine 段）：*「在某些网络环境由于路由器的设置不允许 UDP 广播造成跨物理机组网不成功时，可在此填入所有相关物理机的地址，引擎将会向具体的地址发送探测包来完成组网」*（`<addresses>` 手工列表 workaround）。

**数据结构与关键字段**：broadcast handler（UDP socket + 接口表 `map<u_int32, string> interfaces`，machine.cpp:661-666 基线源码）；machine 暴露端口段 `externalTcpPorts_min=20099`（defaults xml，基线源码）。

**状态机/时序**：进程启动 → 向 machine 广播/shake → machine 维护组件表（Components）→ 死亡检测（超时清理）。

**配置参数默认值**：externalTcpPorts_min 20099 / max 0（=自动）；`<addresses>` 默认空。

---

## §14 运维观测

**实现位置（A 级，基线源码 + 源码基线 已核）**
- skynet 三板斧：monitor 版本号卡死检测 `skynet-src/skynet_monitor.c:31-45`；debug_console 命令表 `service/debug_console.lua:147-174`（基线源码全表：stat/info/exit/kill/mem/gc/start/snax/clearcache/service/task/uniqtask/**inject**/logon/logoff/log/debug/signal/cmem/jmem/ping/call/trace/netstat/**profactive**/**dumpheap**/killtask/dbgcmd——含 jemalloc 堖分析开关）；独立 logger 服务 `service-src/service_logger.c:9-27`（基线源码 `struct logger { FILE* handle; char* filename; uint32_t starttime; int close; }`）。
- BW 集中日志：`server/tools/message_logger/`（基线目录清点，约 70 文件：log_storage/query 引擎族 + **mongodb/ 子目录**（MongoDB 后端）+ mldb + py_bwlog Python 查询接口）——日志查询已做成带查询语言的独立系统；剖析 `server/tools/bw_profile`；上报端 logger_endpoint（§5.7 已核 lib/network/logger_endpoint.cpp:672-703）。
- KBE：独立 logger 进程 `kbe/src/server/tools/logger/logger.{cpp,h}`（基线源码存在）；watcher 路径树 `kbe/src/lib/server/serverapp.cpp:165-181` + 远程查询 `:181-198`；telnet 在线执行 `kbe/src/server/*/telnet_handler.cpp:801-812`；GUI 消费端 guiconsole（kbe/src/server/tools/guiconsole）。

**数据结构与关键字段**：如上 logger 结构；debug_console 命令表本身即「运维面 API 清单」。

**状态机/时序**：monitor 线程每 dispatch 比对版本号（不变 = 卡死报警不杀，skynet_monitor.c:31-45）；logger 服务收 PTYPE_LOG 落盘。

**配置参数默认值**：未展开各 logger 落盘参数（不引用）。

**边界**：运行时检测与外部聚合是不同职责；源码目录清单不等于运维可靠性验收。

---

## §15 实体/进程标识体系

**A 级事实（基线源码）**
- `lib/network/basictypes.hpp:104` `typedef int32 EntityID;`——**BW 实体 id 是 32 位**（进程内唯一，持久化前）。
- `lib/network/basictypes.hpp:191` `typedef int64 DatabaseID;`（含 `PENDING_DATABASE_ID = -1`）——**64 位的是数据库 id**，由 db 侧分配，**未见分段语义**。
- `lib/cstdmf/unique_id.hpp:17-25` `class UniqueID { uint a_,b_,c_,d_; }`——128 位、四段十六进制点分格式（头注释 *"4 uints represented in hex separated by periods"*），用于 machined/日志等组件标识（配套映射 `lib/db_storage_mysql/mappings/unique_id_mapping.cpp`）。
- 进程在网络层的身份 = Mercury `Address`（ip:port）。

---

## §16 战斗独立实例与回放

**A 级负空间证据（基线源码）**：BW `server/` 进程清单 = `{baseapp, baseapp_extensions, baseappmgr, cellapp, cellapp_extensions, cellappmgr, dbapp, dbapp_extensions, dbappmgr, loginapp, reviver, tools}`——**无战斗独立进程**；KBE `kbe/src/server/` = `{baseapp, baseappmgr, cellapp, cellappmgr, dbmgr, loginapp, machine, tools}`——同；skynet 无进程拓扑概念（单节点多服务）。
**结论**：三家均无「战斗独立实例进程 + 确定性回放」的对应实现。Apollo 当前 World 与 Instance 的边界见架构概览；本文不为 Apollo 增加目标进程角色。

---

## §17 数据库支持矩阵

**A 级事实（基线源码目录清点）**
- BW：`lib/db_storage/`（抽象）+ `lib/db_storage_mysql/`（entity_table 映射、buffered_entity_tasks、bw_meta_data、column_type.cpp）+ `lib/db_storage_xml/`（xml_database.cpp 等）——**MySQL + XML 双后端**。
- KBE：`kbe/src/lib/db_interface/`（抽象：db_interface.{h,cpp} + entity_table.h 基类）+ `kbe/src/lib/db_mysql/`（entity_table_mysql.* 等）+ **`kbe/src/lib/db_redis/`（完整实体存储后端：entity_table_redis.{h,cpp,inl}、kbe_table_redis.*、db_interface_redis.*、db_transaction、redis_watcher——entity_table_redis.h:10 `#include "db_interface/entity_table.h"` 实现同一抽象基类）**。

**结论**：已读 BW 基线包含 MySQL 与 XML 后端；已读 KBE fork 包含 MySQL 与 Redis 实现。Redis 后端的生产可用性及上游主线状态未验证。

**数据结构/默认值**：沿用 §5 各条；KBE 各后端 SQL/Redis 语句映射见 entity_sqlstatement_mapping（§6）。

---

## §18 帧同步——检索范围

记录的基线源码关键词检索未发现 lockstep 匹配。这仅限定关键词和目录范围，不能据此证明这些引擎没有确定性模拟或回放机制，也不为 Apollo 引入独立战斗进程。

---

## 19. 查不到与未验证清单（汇总，防后续误引）

| 项 | 状态 | 处理 |
|---|---|---|
| BW ghost 触发阈值的独立配置默认值 | 未定位（未见独立参数项） | §9 已如实标注 |
| skynet harbor→cluster 更替时间线的上游权威口径 | 本机 docs 为占位文档 | §2 已如实标注；代码事实足以支撑判定 |
| KBE db_redis 后端的生产可用性/上游主线状态 | 仅证实体存在与接口实现 | §17 已如实标注 |
| KBE 服务端脚本热更机制 | 已读范围内未检出 | §12 已如实标注（负空间） |
| UE COND_* 的源码级实现 | 本机无 UE 源码 | 未核验，不作为 Apollo 设计依据 |

---
