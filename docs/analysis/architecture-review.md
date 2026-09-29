# Apollo 架构评审与审计链（architecture-review）

> 更名记录（2026-09-29）：本报告原名 `ioc-review.md`（Apollo IoC/DI 设计评审）。起点是 IoC/DI 评审，后续扩展为全仓架构审计（§8–§17：C-1…C-52 缺陷登记、G-1…G-7 能力缺口、三框架对照、模块归属、决策落盘、DI 六域分析、§16.10.2 登记簿、附录 A 合规记录），故更名以名副其实。历史名称在 git 历史与 sdks/contract 注释（冻结纪律内，随下一代码批次同步）中仍可见。

> 分析性文档：只评审，不改动任何源码。评审对象为 `include/apollo/framework/ioc`、`include/apollo/starter`、`src/framework/ioc`、`src/starter` 及其全部调用方与设计文档（docs/03、06、08、14、34、architecture/starter-and-module-assembly-design）。
> 结论立场：**逐项分析可取之处，不预设保留**——值得留的给出落地形态，不值得留的明确建议删除。
> 核心论证（2026-09-28）：为什么 Spring 式运行时容器不适合游戏服务端（生命周期/编译期/热路径/部署形态/行业佐证/思想与形态之分）见 §0——本报告删除建议的总依据。复核追加（2026-09-27）：docs/design 四份设计文档与本评审的交叉一致性复核见 §8；源码级核对第二轮（承接 C-1/C-2 的消费方普查与承重断言复核）见 §9；第三轮（迁移路线调用点/收敛清单/快照验收口径/条件装配实证）见 §10。复核追加（2026-09-28）：第三轮·续（C-16 FileWatcher 阶段 2 改造点细化 + §6 阶段 1 死代码清单逐项消费方复核）见 §11，其中 §11.5 为第四轮（未评审子系统：定时器/日志/场景与 AOI，C-23…C-28，含对「无定时器模块」结论的证伪修正）；第五轮（网络与网关 / 实体与属性，C-29…C-36，含四套网络栈盘点与网关数据路径 Null 桩、三代属性容器定型）见 §12；§13 为第四轮·续（边界子系统与形态一致性，C-37…C-42）；第六轮（数据与持久化 / 多端 SDK 与契约，C-43…C-49，含命名空间冒充模块、线格式三重漂移、数据层构建归属断裂）见 §14；§15 为评审后决策落盘（网络全自研与 nng 退役、契约 XML+XSD 取代第六轮自行假设、MyBatis 语句即数据对比与待同步文档清单）。第七轮（2026-09-28 追加）：BigWorld / KBEngine / skynet 三框架源码对照（文档论断修正 C-50…C-52、能力缺口 G-1…G-7、正面核对）见 §16；§16.7 为其中两项增量的深化（Mercury filter 双族 → L1/L2 蓝本、entities.xml 继承机制）。§15.6 待同步清单 ①–⑥ 已闭环（④ 判定不改写评审记录，落地记录见 §15.6）。第八轮（2026-09-28 追加）：上轮遗留 G-4 展宽为「新增模块归属」总问题——filter 插件体系/契约生成器/config 桩清理/定时器轮/运维观测通道在 apps/ 运维形态下的归属边界与依赖方向（三家框架同类部件位置实证），见 §16.8。§16.9（同日再追加）：⑦–⑪ 以「修订文本落盘」形式完成（门禁收窄为只写本报告——锚点原文 + 逐字替换文本备妥，粘贴即闭环）。§16.10（同日终轮）：⑩ 升级为审计链登记（判定权威记录 = 本报告），⑦⑧⑨⑪ 维持搁置并立登记簿（§16.10.2）。

---

## 执行摘要（最严重 5 条）

1. **运行时字符串键 Service Locator（P0，类型安全）**：`ApplicationContext` 是全局单例 + 字符串注册/查找 + 每次查找全局互斥锁 + `dynamic_pointer_cast`（`ApplicationContext.h:18-21,79-113`）。字符串拼错、类型不匹配全部推迟到运行时才暴露——这正是 Java 反射式 BeanFactory 在无反射语言里的错位模仿。**建议：整体删除**，收敛到仓库内已有的正确答案 `apollo::core::di`（类型键、构造期注入、零锁读，`modules/core/include/apollo/core/di/application_context.hpp`）。
2. **注册期"探针构造"副作用 bug（P0，正确性）**：`registerComponent` 为了读 phase/dependencies 把工厂**真的执行一次并丢弃实例**（`ApplicationContext.h:34-39`）——每个组件被构造两遍，GUID/随机状态不一致；工厂若申请连接、起线程即产生幽灵资源。**建议：删除探针**，依赖/phase 用编译期 trait 或注册参数显式声明。
3. **四套配置系统并存（P1，双份真相）**：`Apollo::ConfigManager`（framework/ioc）+ `ConfigEnvironment` + `apollo::core::config::ConfigManager` + `DefaultConditionContext.properties_`，靠 `syncConfigEnvironmentToManager()` 单向清空重灌来对齐（`ConfigEnvironment.h:178-194`、`ApolloApplication.cpp:351,365`）；且旧 ConfigManager.cpp 被编进新模块（`modules/core/config/CMakeLists.txt:6`）。**建议：删除旧 `Apollo::ConfigManager` 与 ConfigProperty**，配置收敛到 core::config 一套。
4. **Starter 层的三处死代码/假实现（P1，启动复杂度）**：`!HAVE_FRUIT` 时**伪造 `fruit` 命名空间**（`Starter.h:12-30`，ODR 隐患）；`getService<T>()` 无条件返回空 `shared_ptr`（`ApolloApplication.h:127-131`）；Fruit 组合与 Injector 全是 TODO（`ApolloApplication.cpp:284-292,409-419`）。**建议：删除整个 Fruit 维度**，`ApolloStarter` 收敛为纯装配模板接口。
5. **条件装配在 C++ 里半数无意义 + 求值时序缺陷（P2）**：`ConditionalOnClass` 用运行时字符串集合模拟链接期事实（`Conditional.h:67-80`）；`matches()` 先于 `registerBeans()` 求值，`ConditionalOnBean` 看不到任何 Starter 注册的 Bean（`ApolloApplication.cpp:261 vs 275-277`）。**建议：删除 OnClass/OnBean，保留 OnProperty（配置驱动开关）**，条件求值改为注册时单趟。

**关键背景**：真正跑起来的 game-server（`apps/game-server/src/main.cpp`）用的已经是 `apollo::core::di`；旧 IoC/Starter 栈的全部生产代码调用方为零，只有 examples/tests 在用。**README 把"IoC 容器系统"列为第一核心特性，而实际服务器并不使用它**——迁移成本因此非常低，主要工作是"删"而不是"改"。

---

## 0. 核心论证：为什么 Spring 式运行时容器不适合游戏服务端（2026-09-28 追加，本报告删除建议的总依据）

> 本节回答一个必须正面回答的问题：Spring 在 Web 后端是经过千锤百炼的成功范式，凭什么到 apollo 里就该删？结论先行：**要删的不是"依赖注入"思想，而是"Java 企业级运行时容器"形态**——六个维度逐条论证，每条落到 apollo 代码。

### 0.1 结论矩阵

| 维度 | Spring 形态的成立前提 | 游戏服务端的要求 | apollo 现状 | 判定 |
|---|---|---|---|---|
| 生命周期 | 容器全托管，对象短命、状态外置、可随时重建 | 长驻 game loop、帧驱动、确定性启动序 | 探针双构造（P0-2）、autoStart/lazyInit 死字段 | 模型错位，删容器生命周期 |
| 绑定时机 | Java 反射/动态代理/运行时元数据兜底 | C++ 无此层；错误应编译期暴露 | 字符串键 + `dynamic_pointer_cast` 运行时炸 | 运行时模拟反射，删 |
| 热路径 | 查找/代理开销相对请求周期可忽略 | 每 tick 硬延迟预算，热路径零查找零锁 | 每次查找 = 哈希 + 全局互斥 + 动态转换 | 硬伤，启动期定型 |
| 启动/部署 | 几十秒装配几百 bean 是 Web 常态 | 快速拉起；动态性归脚本与配置层 | 双倍构造成本 + 运行期注册与 Lua 热更打架 | 反向价值，删运行期动态 |
| 行业实践 | （Web 业界标准） | 主流引擎无运行时反射容器 | 伪 fruit/伪 @Conditional 孤例 | 佐证见 §0.6 |
| 思想遗产 | 构造注入/分层启动/解耦 | 同样需要 | Starter 分层、core::di 已是正解 | **保留**（§0.7） |

### 0.2 维度一：生命周期语义错位

Spring bean 模型的隐含前提是 **"请求周期 + 无状态服务"**：对象默认 singleton、由容器全托管（实例化→注入→init→destroy），Web 线程池按请求并发调用它们；状态放在 DB/session 里，对象随时可丢弃重建。游戏服务端相反：**长驻 game loop、帧驱动、确定性预算**——60fps 即 16.6ms/帧、tick 周期固定，对象生命周期挂在世界/场景/实体上（enter-view 创建、leave-view 销毁、重连恢复），启动顺序必须逐 tick 可复现。

apollo 的错位实证：

- **探针构造（P0-2，`ApplicationContext.h:34-39`）**：Spring"容器托管生命周期"的第一步是容器**不构造对象就知道它的元数据**——Java 靠反射读注解，零构造成本。C++ 无反射，apollo 的实现退化成"把工厂真的执行一次、读 phase/dependencies、丢掉实例"——每个组件构造两遍，GUID/随机/连接类副作用直接翻倍。**生命周期托管的第一个机制，在移植中就变成了正确性 bug**——这不是实现手艺问题，是无反射语言里硬搬"容器先于对象知道一切"的必然结果。
- **autoStart/lazyInit 双死字段（`BeanDefinition.h:15-16` 与 `:36`）**：Spring 的 lazy-init/SmartLifecycle 语义被抄进了结构体，但游戏侧没有任何读者（§10 复核：autoStart 唯一"读"是 `ApplicationContext.h:270` 的字段拷贝）。游戏要的启停语义是**确定性启动分层**（Starter phase）与**帧内 tick 序**，不是"首次 getBean 时才构造"。
- **仓内已有的正确形态**：`IHostedService`（`modules/runtime/include/apollo/runtime/application_host.hpp:40-47`）——`start/stop/is_running/tick`，生命周期挂在**主循环**上（tick 是显式成员），不挂在容器上。C-11 已证它与被评审的 `IComponent` 胖接口形成正反对照。

判定：生命周期语义错位不是调参问题，是模型错位。→ 删容器托管生命周期，保留显式分层启动（§6 阶段 4 Bootstrap 模板）。

### 0.3 维度二：运行时模拟编译期能做的事（无反射语言里的必然变形）

Spring 的全部"魔法"垫在 Java 运行时层：反射读注解、动态代理做 AOP、类路径扫描（@ComponentScan）、@Conditional 读运行时元数据。C++ 没有这一层，于是 apollo 的每一处模仿都变形了：

| Spring 原型 | Java 里的成立方式 | apollo 的 C++ 模仿 | 变形结果 |
|---|---|---|---|
| BeanFactory 按名取 bean | 反射 + 字符串名，类型由泛型擦除兜底 | 字符串键 + `dynamic_pointer_cast`（`ApplicationContext.h:79-117`） | 拼错 key/类型不符**运行时才炸**（P0-1） |
| @Component/@Qualifier 注解 | 注解处理器与反射读取 | 伪 `fruit::createComponent`（`Starter.h:93-94,115-116`） | 无处理器，只剩 squatting 壳（P1-2、C-20） |
| @Conditional 条件装配 | 读**编译进 classpath 的字节码元数据**，编译期即成立 | `ConditionalOnClass` 用运行时字符串集合模拟（`Conditional.h:67-80`）；matches() 时序缺陷（`ApolloApplication.cpp:262-278`，C-14） | OnClass 模拟**链接期事实**注定又晚又错；OnBean/OnStarter 恒 false |
| @ComponentScan | 类路径扫描 + 反射实例化 | 宏注册 + 静态初始化期 AutoRegister（P2-1；数据库层第二套见 C-15） | 初始化顺序不可控的脚手架 |

"类存在与否"在 C++ 是**链接期事实**——用运行时字符串集合去回答它，既晚（到运行时才知道编译期就定的事）又错（时序上不可满足）。行业惯例与仓内正解一致：**编译期装配定型**——直接构造注入、CRTP/static registry、模板/类型键。`apollo::core::di`（`modules/core/include/apollo/core/di/application_context.hpp`，TypeKey 编译期类型键 + 构造期注入）就是这条路线，类型不匹配在编译期报错、依赖缺失在启动期报错、运行期零查找。

游戏业文献早把这笔账算清：Robert Nystrom《Game Programming Patterns》Service Locator 章把 **compile-time binding** 列为运行时查找的头号替代——"Since the locator owns the service now and selects it at compile time... if the game compiles, we won't have to worry about the service being unavailable"（出处见 §0.6）。运行时查找在游戏语境里从来是需要辩护的让步，不是默认。

### 0.4 维度三：热路径零容忍

游戏逻辑服每帧/每 tick 有硬延迟预算，性能纪律是**热路径零查找、零锁、零间接层**（net-abstraction.md §3 的单写者线程模型把这写成设计红线）。

- 旧容器每次查找 = **字符串哈希 + 全局互斥锁 + `dynamic_pointer_cast`**（`ApplicationContext.h:79-113`，全局单例一把锁）——哪怕每 tick 只查一次也是无谓成本；查 N 次即按帧线性付费。它没有任何"增值服务"可交换：Java 侧 AOP 代理换来事务/日志织入是笔买卖，C++ 侧没有代理机制，**只剩裸查找成本，零收益**。
- 容器思维的二次污染更值得警惕：它训练调用方养成"每次用都查"的习惯。同构反模式已在新代码复现——`core::config` 每次 `get` 都 `shared_lock` + 路径解析 + 类型转换（`modules/core/config/src/config_manager.cpp:241-247`，数值型 `get*` 再加每调用转换 `:251-293`，C-7）。**容器/查找式 API 对调用方代码的塑造，比容器本身的删留更长效**。
- 正确形态：启动期解析一次、持引用直调——指针在构造期交到你手里（core::di 的做法），运行期不再经过任何容器。这也是 §6 阶段 4 Bootstrap 把"装配"压缩到启动单趟的原因。

### 0.5 维度四：启动与部署形态相反

- **Web 侧**：应用服务器几十秒装配几百 bean 是部署常态，redeploy/reload 频繁，运行期容器的动态性（热部署、profile 切换、refresh scope）有真实业务价值——动态性是这个形态的**卖点**。
- **游戏侧**：动态性的价值主张正好相反。游戏服要**快速拉起**（崩溃恢复、滚服开新、版本重启），C++ 对象层要**静态可预测**；真正的动态性在另外两层——配置热更（reload，C-19/C-22 的驱动机制）与 Lua 脚本热替换（scripting-lua.md §3.2：工作线程预编译 → tick 边界原子换表 → 回滚）。
- apollo 实证：
  - 探针双构造把启动期构造成本直接 ×2（`ApplicationContext.h:34-39`）；
  - 条件装配的多趟启动扫描（`ApolloApplication.cpp:258-278`）增加启动复杂度，却因 C-14 时序缺陷**无一可用**——纯增熵；
  - 运行期 register/unregister API 与 Lua 热更体系**打架**：scripting-lua.md §3.2 已把热替换边界定为脚本模块表，C++ 侧对象启动定型（§5 结论：运行期注册随删除）；scripting-lua.md §4.2 红线 1 明确禁止把旧字符串容器绑进 Lua。「哪些对象可热换、哪些必须稳定」在游戏运营里是**语义问题**（数值表可热更、战斗物理不可），按名字动态增删的容器只会模糊这条边界，不会支撑它。

### 0.6 维度五：行业实践佐证（公开资料）

1. **Unreal Engine（Epic 官方文档）**：引擎级"服务定位/依赖提供"由 **Subsystems** 承担——"automatically instanced classes with managed lifetimes"（自动实例化、生命周期受管），注册方式是**继承对应 C++ 基类**（`UGameInstanceSubsystem`/`UWorldSubsystem`/`ULocalPlayerSubsystem`…），实例随父对象（GameInstance/World/LocalPlayer）自动创建销毁。没有字符串注册表、没有运行时反射容器：**声明靠继承与链接，生命周期锚定游戏对象树**。apollo 对应形态：Starter 分层 + core::di 构造注入 + IHostedService tick。[Programming Subsystems in Unreal Engine — Epic Games Dev](https://dev.epicgames.com/documentation/unreal-engine/programming-subsystems-in-unreal-engine)
2. **Unity DOTS / Entities（Unity 官方文档）**：数据导向栈把"运行时形态构建期烘定"推到极致——组件是 unmanaged `struct`、系统是 Burst 编译的 `ISystem`，创作态数据经 SubScene **baking**（构建期烘焙）变为运行态纯数据。[SubScenes and Baking — Unity Entities 手册](https://docs.unity3d.com/Packages/com.unity.entities@1.0/manual/conversion-subscenes.html)。主流商业引擎的演化方向与"运行时反射容器"背道而驰：**把一切都前移到编译期/构建期，运行期只剩数据和直接调用**。
3. **《Game Programming Patterns》（Robert Nystrom，游戏业通用文献）Service Locator 章**：明确列出 compile-time binding 替代及其保证（"selects it at compile time... if the game compiles, we won't have to worry about the service being unavailable"），并指出运行时配置的代价（"Locating the service takes time"、未注册即失效——"Any code accessing the service presumes that some code somewhere has already registered it"）。后者正是 apollo P0-1 的运行时失效模式。[Service Locator · Decoupling Patterns](https://gameprogrammingpatterns.com/service-locator.html)

范围界定（公允起见）：Java/Go 生态写网关、平台服、运营后台时 Spring 形态完全成立；本节论证范围是 **C++ 游戏逻辑服的帧驱动内核**——帧预算、确定性、无反射三条约束同时成立的地方。

### 0.7 公允结论：思想保留，形态移除

| Spring 遗产 | 判定 | apollo 落点 |
|---|---|---|
| 构造注入 / 依赖显式化 | **保留 ✓** | `apollo::core::di` 构造期注入（已是仓内正解，game-server 在用） |
| 分层启动 / 装配序 | **保留 ✓** | Starter 分层思想（§4 肯定项）；§6 阶段 4 Bootstrap 模板 |
| 模块化解耦、面向接口 | **保留 ✓** | modules/ 模块化 + 显式接口；依赖关系进构造签名而非字符串 |
| 容器托管生命周期（init/destroy/lazy/autoStart） | **移除 ✗** | P0-2 删探针；P3 删 autoStart/lazyInit 死字段；生命周期归主循环（IHostedService.tick） |
| 字符串注册 + 运行期按名查找 | **移除 ✗** | P0-1 整删（全局锁查找 + dynamic_pointer_cast，零生产调用方） |
| 运行期 register/unregister | **移除 ✗** | §5 结论：随删除；动态性归 Lua 热更与配置 reload |
| 运行时条件装配（OnClass/OnBean） | **移除 ✗** | P2-2 + C-14：恒 false 且模拟链接期事实；保留 OnProperty（配置驱动开关）改注册时单趟 |
| 静态注册宏脚手架（AutoRegister/宏注册） | **移除 ✗** | P2-1 + C-15/C-20：初始化顺序不可控，零或内部消费方 |
| AOP 式横切（代理/拦截器） | **不移植** | C++ 无代理层，横切用显式组合/模板策略（日志 sink、net adapter 均已此形态） |

一句话收束：**删除的不是依赖注入，而是 Java 企业级运行时容器在 C++ 游戏服里的错位形态**；本报告所有"删除"级建议（P0/P1 与 §6 阶段 1-3）以本节为共同依据，所有"保留"级建议（Starter 分层、core::di、显式装配）同样以本节为背书。

---

## 1. 分析范围与方法

- 通读 `include/apollo/framework/ioc`（7 个头文件）+ `src/framework/ioc/ConfigManager.cpp` + `include/apollo/framework/base/BaseComponent.h` + `include/apollo/utils/config/ConfigProperty.h`。
- 通读 `include/apollo/starter`（5 个头文件）+ `src/starter/ApolloApplication.cpp`。
- 梳理全部调用方（rg 全仓）：examples、tests、`src/starter`、BaseComponent/ConfigProperty；确认 game 模块（ECS/AOI）与 apps（game-server 等）是否使用。
- 对照设计文档：docs/14（Spring 功能对照）、docs/34（ApplicationContext 2.0 设计）、docs/08（Spring Bean 生命周期笔记）、docs/architecture/starter-and-module-assembly-design.md（明确写了"不做 C++ 版 Spring Boot 克隆"）。
- 对照仓库内第二套实现：`modules/core` 的 `apollo::core::di`（新容器）与 `apollo::core::config`（新配置），作为评审的"参照系"。

## 2. 现状全景：容器到底怎么被使用

### 2.1 注册方式（三条路，全是运行时字符串）

| 方式 | 入口 | 键 | 时机 |
|---|---|---|---|
| 字符串+工厂 | `registerComponent(name, factory)` | 运行时字符串 | 任意运行时刻 |
| 模板便捷版 | `registerComponent<T>()` | `T::getStaticName()`（由 `DECLARE_COMPONENT` 宏注入） | 任意运行时刻 |
| 静态自动注册 | `REGISTER_COMPONENT` 宏 → 全局对象构造 | `T::getStaticName()` | **main 之前**（静态初始化期，跨 TU 顺序不确定） |

Starter 侧对应 `APOLLO_REGISTER_STARTER`（`StarterRegistry.h:186-199`，`__COUNTER__` 保名字唯一）：静态期 `make_shared<T>()` 实例化所有 Starter——即 **Starter 构造函数先于 main、先于配置加载执行**。

### 2.2 生命周期与依赖解析

- 依赖图是**字符串边**：`IComponent::getDependencies()` 返回 `vector<string>`；`LifecycleProcessor::sortBeanDefinitions`（`LifecycleProcessor.h:15-86`）在 `initializeComponents()`/`startComponents()`/`stopComponents()`/`destroyComponents()` **每次调用都重新拓扑排序**（Kahn + phase/名字确定性破平，实现本身是对的）。
- 缺依赖、成环都是启动期 `throw std::runtime_error`——错误暴露时机已经晚（运行时），错误信息还不带完整环路径（`:82`）。
- 解析时机：`getComponent(name)` 惰性实例化 + 缓存（`ApplicationContext.h:79-107`），**每次调用都过全局 mutex**。

### 2.3 与业务/游戏回路的接线

- **ECS 战斗系统**（`include/apollo/game/battle/ecs/ecs.h`）：自己的编译期 `World::getComponent<T>`，模板实现，**与 IoC 容器零关系**。
- **AOI**（`include/apollo/game/aoi/aoi.hpp`）：无容器依赖。
- **game-server**（`apps/game-server/src/main.cpp:96-102`）：用 `apollo::core::di` 的 Builder 显式装配 `GameClockService`/`LoginPipeline`，tick 走 `apollo::runtime::ServiceHost`。
- **结论：旧 IoC/Starter 栈是平行宇宙**——README 的招牌特性与实际服务器管线（`core::di` + `runtime::ServiceHost`）互不相接。旧栈的存在形态 = examples + tests + 两个"样板间"基类（BaseComponent/ConfigProperty）。

### 2.4 条件装配

`ApolloApplication::Builder`（enable/disable/withProperty/configFile）→ 静态注册的 Starter 经 `matches(ConditionContext)` 过滤 → `registerBeans()` → `onInitialize()`（build 阶段）→ `start()` 时统一 initialize/start 组件再逐个 `onStart()`。条件原语五种：OnProperty/OnClass/OnBean/OnStarter/Custom + AllOf/AnyOf/NoneOf 组合器。

## 3. 问题清单（按严重度：热路径 > 类型安全 > 启动复杂度 > 风格）

> 每条：位置 → 现状 → 为什么不合适（C++/MMO 语境）→ 建议（删除/简化/编译期替代）→ 影响面与迁移成本。

### P0-1 运行时字符串键 Service Locator + 全局锁查找

- **位置**：`ApplicationContext.h:18-21`（单例）、`:79-107`（`getComponent(name)` 全局锁 + 字符串哈希 + 惰性实例化）、`:109-113`（`getComponent<T>()` = `dynamic_pointer_cast`）、`BaseComponent.h:111-114`（组件内部再次 Locator）。
- **现状**：唯一取服务途径是"按名字查全局单例"。类型安全靠运行时 `dynamic_pointer_cast`，查不到返回 `nullptr`，无诊断。
- **为什么不合适**：
  - Spring 的 BeanFactory 依赖反射做类型擦除和按名注入，C++ 没有反射，于是"名字"从注入的辅助手段变成了**唯一身份**——拼错一个字符串，编译器全程沉默，直到某次运行时返回空指针。编译期能表达的依赖关系（类型）被降格成运行期约定。
  - 每次查找过全局 mutex：MMO 服务器长驻、热路径确定性延迟优先。当前它没进热路径（因为没人用），但 `BaseComponent::getComponent` 把"循环里随手查一次服务"变成了合法写法——这是把 Java 请求式模型（每请求查一次容器，锁开销被请求间隔摊薄）带进了每 tick 数万次调用的游戏循环语境。
  - 双重间接：字符串哈希 + RTTI cast，每一层都在丢编译期信息。
- **建议**：**删除** `Apollo::ApplicationContext` 及 `BaseComponent::getComponent`。服务获取一律改为两种形态：(a) 构造期注入引用（`apollo::core::di` 已实现：`add_singleton<LoginPipeline, GameClockService>()` → `new Impl(ctx.get<Deps>()...)`，`application_context.hpp:336-339`）；(b) 极少数运行时多态点（脚本/GM 命令）用显式的服务句柄表，由启动期一次性填充。
- **影响面**：生产代码零调用方；examples 6 个、tests 若干、BaseComponent/ConfigProperty 两个样板。迁移 = 删除 + 重写 examples（每个 <100 行）。**成本：低。**

### P0-2 注册期探针构造（factory 被执行两次）

- **位置**：`ApplicationContext.h:34-39`。
- **现状**：`registerComponent(name, factory)` 里 `auto probe = definition.factory();` ——为了读 `probe->getPhase()/getDependencies()`，把组件**真实构造一次然后丢弃**。
- **为什么不合适**：Spring 的 BeanDefinition 是纯元数据（class/method/factory），实例化是受控的（lazy/eager/scope），元数据读取**不产生实例**。这里因为 C++ 没有"从类型读注解"的反射，作者把元数据放进了实例方法，于是只能构造实例来读元数据——副作用链完全失控：构造函数里开线程/占端口/生成随机 GUID（`BaseComponent::generateGuid`，`BaseComponent.h:130-144`）都会发生两次，第二次的实例才被容器持有；探针实例上的任何状态丢失。
- **建议**：删除探针。phase/dependencies 声明移到编译期或注册参数：
  - trait 形态：`struct X { static constexpr int phase = ...; static constexpr std::array deps = {...}; }`（`if constexpr` 消费，没有实例、没有运行时开销）；
  - 或 Builder 形态（新容器已示范）：`builder.add_singleton<A, B>().tag("net")`。
- **影响面**：只影响旧容器路径（examples/tests）。**成本：随 P0-1 一并消失。**

### P1-1 四套配置系统并存，单向 sync 缝合

- **位置**：
  1. `Apollo::ConfigManager`（`framework/ioc/ConfigManager.h`：string 存储 + `std::any` 双缓存 + `getValue<T>` 每次字符串解析，`ConfigManager.h:46-63`）；
  2. `Apollo::ConfigEnvironment`（来源优先级模型，`ConfigEnvironment.h:16-24`）；
  3. `apollo::core::config::ConfigManager`（新模块，game-server 实际使用）；
  4. `DefaultConditionContext::properties_`（`ConditionContext.h:158`，第 5 份 if 算上 Builder 的 `properties_`）。
  缝合点：`syncConfigEnvironmentToManager()`（`ConfigEnvironment.h:178-194`）**先 `clear()` 再逐 key setValue**；`ApolloApplication::loadConfiguration` 同时喂 2/3/4（`ApolloApplication.cpp:331-351`）；`createConditionContext` 拿 1 的键集配 2 的值（`:365`，键在 1 不在 2 时写入空属性）；且旧 `src/framework/ioc/ConfigManager.cpp` 被直接编进新模块（`modules/core/config/CMakeLists.txt:6`）。
- **为什么不合适**：
  - Spring 的 `Environment` 是单一抽象、多来源分层——可取之处在**优先级模型**（defaults < file < env < cmdline，`ConfigSourcePriority` 这层设计是对的），错位在于落地成了**两套并存系统 + 单向搬运**：任何一处直接改值都可能被下一次 sync 静默抹掉。
  - 旧 `getValue<T>` 热路径形态：mutex + unordered_map 查找 + `std::any` try_cast 失败再 `stoi` 字符串解析——每 tick 读一次战斗数值就是一次锁+解析。游戏服的正确形态是**启动期绑定成类型化快照**（读成员变量，零锁零解析），变更走显式 reload 事件。
  - 细节缺陷伴生：`removeGlobalChangeListener` 是**未实现的空函数**（`ConfigManager.cpp:159-181`，注释自述"无法比较 std::function"）；FileWatcher 热加载 `onFileChanged → loadFromFile` **全程不通知任何 listener**（`:222-224`）——"配置热更新"特性实际是哑的；`getValue(key, default)` 先 `hasValue` 再 `getValue` 双重加锁，且键存在但不可解析时**抛异常而不会返回 default**（`ConfigManager.h:65-71` + `:135` 的 `stoi`）；`ConfigEnvironment::getString` 无法区分"配置为空串"与"未配置"（`ConfigEnvironment.h:105-109`）；`ConfigProperty` 首读后 `hasValue_` 永真、**永不回读**（`ConfigProperty.h:28-33`）——文件变了它也不变，与热更新诉求直接矛盾。
- **建议**：**删除** `Apollo::ConfigManager`、`ConfigProperty`（含 `CONFIG_PROPERTY` 宏）；`ConfigEnvironment` 的优先级/来源追踪（`explain()`）保留并下沉为 `apollo::core::config` 的唯一实现；条件上下文直接引用 ConfigEnvironment，不再复制一份 properties。
- **影响面**：examples/config_example、all_features_demo、core config 模块编译清单。**成本：低**（新配置 API 已存在且更完善）。

### P1-2 Starter 层：伪 fruit 命名空间、空指针 stub、TODO 死路

- **位置**：`Starter.h:3-31`（`#ifndef HAVE_FRUIT` 时定义 `namespace fruit { struct Component; struct Injector; createComponent(); }` **占位类型**）；`ApolloApplication.h:123-132`（无 Fruit 时 `getService<T>()` 返回**默认构造的空 shared_ptr**）；`ApolloApplication.cpp:284-292`（"TODO: 正确构造 Fruit Injector"）、`:409-419`（`combineStarterComponents` 返回空组件）。
- **为什么不合适**：
  - 在自家头文件里**伪造第三方库命名空间**，若工程里真的链接 fruit（或任何同名符号）即 ODR/名字冲突；读者看到 `fruit::PartialComponentVoid` 无法分辨真假。
  - `getService<T>()` 是**承诺了签名却永远返回 null 的 API**——调用方拿到空指针没有任何编译期/启动期诊断，比删掉这个方法更糟。这是"接口先行、实现未至"留在主干上的典型尸体。
- **建议**：**删除整个 Fruit 维度**（条件编译、`getComponent()` 虚函数返回 `fruit::PartialComponentVoid`、`getInjector`/`getService`、combineStarterComponents）。C++ 的编译期依赖装配不需要运行时 injector 对象——新容器证明构造期注入即可。
- **影响面**：`HAVE_FRUIT` 在构建里从未定义（vcpkg.json 无 fruit 依赖）——纯死代码。**成本：零风险删除。**

### P1-3 第二套依赖图 `DependencyManager`（无锁、无消费方）

- **位置**：`DependencyManager.h:12-223`：又一个全局单例，自带一份 `dependencies_` 字符串图、自己的拓扑排序（`:129-183`，与 `LifecycleProcessor` 重复实现）、自己的环检测。
- **为什么不合适**：
  - **生命周期主路径根本不消费它**——`initializeComponents` 用 `LifecycleProcessor::sortBeanDefinitions(definitions_.dependencies)`。`DependencyManager` 是平行的第二真相：`addDependency("A","B")` 之后组件的 `getDependencies()` 不会变，两份图可以互相矛盾。
  - 全类**无任何锁**（对比 ApplicationContext 每方法加锁）——同样的"注册可并发"假设下这是一处数据竞争（对照 docs/qa/q78-race-condition.md 自己的论述）。
  - `getMissingDependencies` 拿 `getComponentNames()` 做 O(n²) 线性查找；`hasCircularDependency` 用异常当控制流。
- **建议**：**删除**。依赖声明唯一入口 = BeanDefinition/Builder（一张图）。若需要"运行时诊断"（/beans 端点），从同一张图派生只读视图，不设第二可写入口。
- **影响面**：仅 examples/dependency_example.cpp 与 tests。**成本：低。**

### P2-1 宏注册 + 静态初始化期注册

- **位置**：`ComponentRegistry.h:6-21`（`REGISTER_COMPONENT`/`DECLARE_COMPONENT`）、`:25-31`（`AutoRegister<T>`）、`StarterRegistry.h:166-199`（`StarterAutoRegistrar` + `APOLLO_REGISTER_STARTER`）。
- **为什么不合适**：
  - 全局构造对象在 **main 之前**注册：跨 TU 顺序不确定（= 链接顺序决定注册顺序），而 LifecycleProcessor 恰恰声称"禁止依赖注册顺序"（docs/34 §3.1）——机制与自己的规约打架。
  - 静态期注册使"先选 profile、再决定注册哪些模块"不可能：所有 Starter 在你来得及读配置前就已构造完毕（构造函数里 setMetadata 只能写常量）。
  - `DECLARE_COMPONENT(ClassName, "Name")` 把**字符串身份**硬编码在类体内——类名与注册名可以不一致，类型检查帮不上忙。
  - Java 侧等价物（classpath 扫描 + 注解处理）是编译器/容器基础设施；C++ 的宏只是手写自动化，代价是静态初始化顺序惨案与不可调试的隐式行为。
- **建议**：**删除全部注册宏**。注册发生在 `main()` 早期、profile/配置就绪之后，用显式 Builder（新容器模式：`apps/game-server/src/main.cpp:96-99` 已示范）。模块想提供"默认装配"，提供 `registerXxxBeans(ApplicationContextBuilder&, const Config&)` 自由函数即可——见 §4 对插件机制的分析。
- **影响面**：examples 全部改写；BaseComponent 用户。**成本：低（用例量小、目标形态已存在）。**

### P2-2 条件装配：一半无意义，剩下一半有时序缺陷

- **位置**：`Conditional.h:67-80`（`ConditionalOnClass`→`ctx.isClassAvailable(string)`）、`ConditionContext.h:120-126`（可用类=**手工 mark 进字符串集合**）；时序缺陷 `ApolloApplication.cpp:261`（matches 求值）vs `:275-277`（markBeanPresent 在所有 registerBeans **之后**）。
- **为什么不合适**：
  - Spring `@ConditionalOnClass` 检查的是**类路径**——因为 Java 可以运行时装载类，"类是否存在"是运行期事实。C++ 里类在不在 = 链接进没链接进 = **构建期事实**：代码在，条件必真；代码不在，写条件的 Starter 也不存在，条件根本没机会求值。运行时字符串集合 + 手工 mark 是用人肉模拟一个链接器已经回答过的问题，且答案可以和链接器相反（mark 了不存在的类）。
  - `ConditionalOnBean` 时序：matches 先于 Starter 间 registerBeans 执行，条件只能看到静态注册的 Bean——Starter A 想说"有数据库 Bean 我才装"，永远判假，除非数据库 Bean 恰好是静态注册的。语义与 Spring 的延迟求值模型错位。
  - `AllOf/AnyOf/NoneOf` 对四种条件类逐一重载 `add()`（`Conditional.h:143-264`）——`template<typename... C> AllOf(C... c)` 一行的事，是"无变参模板的 Java 8 思维"写 C++。
  - 值得肯定并保留的内核：**配置驱动的模块开关**对游戏服真实存在（dev/prod、单机/BigWorld、开/关某玩法模块）。落点应是 `ConditionalOnProperty` 一种——配置读 `ConfigEnvironment`，在注册阶段（Builder 循环里）求值一次，不过虚接口、不建条件对象图。
- **建议**：删除 OnClass/OnBean/OnStarter（OnStarter 的信息在注册循环里本就唾手可得）；删除组合器类；保留"OnProperty + 注册期单趟过滤"约 30 行的实现。
- **影响面**：starter tests。**成本：低。**

### P2-3 Starter 元数据：dependsOn 声明了但没人实现；autoEnabled 过滤逻辑自相矛盾

- **位置**：`Starter.h:67`（`dependsOn` 字段）、`StarterRegistry.h:84-98`（注释宣称"同 order 按依赖关系排序"，实现**只按 order 排序**）；`ApolloApplication.cpp:381-407`（`filterEnabledStarters`：`autoEnabled=false` 的 Starter **即使被显式 `enableStarter()` 也永远不会启用**，`:401`）。
- **为什么不合适**：文档承诺依赖感知排序，代码没有——启动顺序错误只能靠人肉排查；enable/disable/white-list/autoEnabled/Conditional **五重开关语义叠加**，出现"显式启用打不过默认关闭"这种反直觉组合。Spring 的 auto-configuration 背后是 spring.factories + 条件评估器一套自洽机制；这里只搬来了开关名词没搬机制。
- **建议**：开关收敛为一层：`profile/config 决定候选集 → 依赖图排序 → 注册`。`autoEnabled` 删除（用配置表达默认值）；`dependsOn` 要么实现进排序（复用 LifecycleProcessor），要么删字段。
- **影响面**：starter 模块。**成本：低。**

### P2-4 `IComponent` 胖接口 + 可被绕过的状态机

- **位置**：`IComponent.h:35-54`（11 个虚函数：initialize/start/stop/destroy/getState/getName/**getGuid**/**setState**/dependsOn/getDependencies/getPhase）；`BaseComponent.h:88-91`（`setState` 公开，任何人可把组件状态改成立即可 `start()` 的样子）；`:148-151`（`atomic<int> phase_`/`atomic<ComponentState> state_` 与 `stateMutex_` 混用：`getState` 无锁读 atomic，`initialize` 全程持锁——两种同步模型并存但都不完整）。
- **为什么不合适**：
  - Spring bean 生命周期回调（InitializingBean/DisposableBean/@PostConstruct）的本质是**两个可选钩子**；这里膨胀成 11 方法契约，每个组件被迫回答"你的 GUID 是什么"（随机 UUID 在服务端组件上无任何消费方，还在注册探针里制造了两倍的随机数消耗）。
  - 状态机不变量（UNINITIALIZED→INITIALIZED→STARTED→…）靠约定维护，却把 `setState` 公开——任何持有指针的代码一行业务代码就能破坏它。
  - `dependsOn(string)`/`getDependencies()` 是字符串图渗入接口——与 P0-1 同根。
- **建议**：接口瘦身为 `start()/stop()`（或直接复用 `apollo::runtime::IHostedService`——`service_name/start/stop/is_running/tick`，game-server 已在用）。元数据出接口，进 trait/Builder。`setState` 删除，状态由宿主管理。`getGuid` 删除。
- **影响面**：BaseComponent 及 examples。**成本：低。**

### P3 风格/细节类（随主线删除即消失）

| 位置 | 问题 | 处置 |
|---|---|---|
| `BeanDefinition.h:15-16` | `lazyInit` 全仓无消费；`autoStart` 只被拷贝进 runtimeInfo（`ApplicationContext.h:270`），不影响任何行为——Spring 词汇表里的死字段 | 随容器删除 |
| `ApplicationContext.h:163-178` | `destroyComponents` 清 `components_` 但不清 `runtimeInfo_`；stop/destroy 每次重算拓扑序 | 随容器删除 |
| `Starter.h:115` | `getComponent()` 返回伪 fruit 类型，语义是"starter 贡献 DI 组件"但无任何调用方 | 随 P1-2 删除 |
| `ConditionContext.h:85-88` | `loadFromJson` 解析失败**静默吞掉**（注释自认"静默失败"）——配置错误不可见 | 随条件系统收敛删除 |
| `ApolloApplication.h:94-104` | `build()` 里调用 `initialize()`——**构造器里做配置加载+Starter 过滤+注册**，构造即半个启动；异常从 build() 抛出时 unique_ptr 尚未接管，资源清理路径含糊 | 重构为两阶段：build() 只存配置，start() 才装配 |
| `README.md` 核心特性第一条 | 宣称"IoC 容器系统"为主特性，实际 game-server 不用旧容器 | 改为描述 `core::di` 与 runtime ServiceHost |

## 4. 逐项可取之处分析（结论：留 or 删）

| Spring 模仿点 | 可取之处分析 | 结论 |
|---|---|---|
| BeanFactory 式字符串容器 | C++ 无反射，字符串身份只剩成本没有收益；仓库已有类型键容器证明正解 | **删除** |
| Bean 生命周期分阶段 | 分阶段启动/逆序关闭 + 失败回滚（`rollbackInitializedComponents`）对游戏服真实必要（gateway 最后开、最先关） | **保留思想**；落地为 ServiceHost 的 start/stop(+tick)，阶段数砍到 init/start/stop 三档 |
| 拓扑排序 + phase + 确定性破平 | `LifecycleProcessor` 是全仓质量最高的容器代码：phase 分层、同 phase 按名字稳定序，重启可复现 | **保留实现**（迁移给 core::di 或 starter 装配层复用） |
| Starter/自动配置（插件机制） | "模块提供默认装配、app 只选模块"对 mono-repo 多 app（gateway/login/base/cell）真实有效——docs/architecture/starter-and-module-assembly-design.md 的 Module/Starter/Profile 模型本身就是对的 | **保留思想，删除现有实现**：starter 改为"链接期模块 + 注册函数 + 配置过滤"，运行期不设 Starter 对象图、不设条件求值器 |
| 条件装配 | 配置驱动开关（OnProperty）在游戏服有真实场景；OnClass 在 C++ 无意义、OnBean 时序缺陷 | **只留 OnProperty**，注册期一趟求值 |
| 配置 Environment 分层来源 | `ConfigSourcePriority` defaults→cmdline + `explain()` 溯源是对的（运维要回答"这个值哪来的"） | **保留**，收敛进 core::config 单实现 |
| 管理端点设想（docs/34：/health /beans /metrics） | 游戏服运维刚需；`BeanRuntimeInfo`（stage/lastOperation/lastError）就是为它准备的 | **保留设想**；数据源从新容器导出（类型名 + 状态枚举），不依赖字符串容器 |
| EventBus/Scheduler 塞进 ApplicationContext | docs/34 §15 自己已警告风险；调度进主循环会与场景 tick 冲突 | **拆出**：EventBus/Scheduler 独立模块，容器只管装配 |
| Fruit DI 集成 | 编译期 DI 库与 core::di 定位重复，且现有集成是 TODO 空壳 | **删除** |
| `apollo::core::di` 新容器 | 类型键（`type_key.hpp:7-13`，函数局部静态地址）、构造期注入、启动期 eager 解析、读路径零锁、名字查询仅可选 | **保留并作为唯一容器**；补两处：`try_get` 在 release 下静默返回 null（`:248` 的 assert 编译掉）应改为启动期完整性校验 + 显式错误；懒加载单例并发首建无锁保护（`ensure_singleton_created:171-193`）——game-server 全 eager 用法下无碍，文档化"禁止运行期并发 get"即可 |

## 5. 脚本化与热更视角（Lua 规划对容器设计的要求）

> 详细脚本层设计见 `docs/design/scripting-lua.md`；本节只回答"IoC 侧要为 Lua 热更准备什么"。

1. **动态性分层：容器保持"启动期定死"，动态性全部让给 Lua。** Spring 的动态能力（运行时注册/刷新 scope/热替换 bean）在 C++ 侧全部是负债（见 P0/P1 各条）；MMO 需要的热更——改数值、改公式、改任务/AI 逻辑——恰好都在脚本层。容器的正确姿态是：**C++ 服务在启动期完成装配后，把稳定句柄一次性注入 Lua 注册表**，运行期容器不增删 bean。
2. **禁止在 Lua 层重现字符串查找容器。** 旧 `ApplicationContext` 按字符串名查 bean 的模型若被 Lua 仿效（`apollo.find_service("db")`），P0-1 的全部问题会在脚本层原样复发且更难查（拼错只报运行时 nil）。暴露给脚本的应是**具名模块表**（`apollo.attr`、`apollo.db`），绑定即校验、缺失在启动期报错。
3. **热替换时哪些"bean"重绑、哪些必须稳定：**
   - 可重绑（Lua 侧）：玩法逻辑模块、属性公式（docs/03 的 Formula 移到脚本层）、任务/AI 行为、掉落表等**数据驱动的值对象**——替换的是 Lua module table，C++ 对象不动。
   - 必须稳定（C++ 侧）：网络会话池、DB 连接池、定时器轮（新建件，见 ssengine-reference.md §4.3——现状无定时器模块）、ECS World、AOI 网格、属性存储本体。C++ bean **不做运行期 unregister/re-register**（当前 `unregisterComponent`/`destroyComponents` 这类 API 在热更语境是危险品，随旧容器删除）。
   - 桥接物：脚本钩子注册表（如 `onAttrChanged` 回调表）由 C++ 持有 Lua ref，热替换时**原子换表、下一 tick 生效**，失败回滚旧表。
4. **Spring 式动态性里对热更有用 / 有害的清单：**
   - 有用：配置外部化 + 来源优先级（ConfigEnvironment 的模型）→ 热更配置的载体；`@RefreshScope` 的"变更以事件通知"思想 → `reload(changedKeys)` 事件分发（注意现状旧 ConfigManager 热加载不通知 listener 的缺陷，P1-1）；Profile → dev/prod 脚本目录切换。
   - 有害：运行时 bean 重注册/注销；per-request scope；条件求值依赖注册顺序；整文件 reload 无事务（旧 `loadFromFile` 先 clear 后填充，中途失败配置半空——热更必须双缓冲后原子切换）。
5. **热更执行点与游戏回路：** 替换动作放在 tick 边界（帧间隙）执行，脚本侧加载/编译在线程池完成，主线程只做 O(1) 换表；换表期间到达的消息按旧表处理完当前帧——确定性优先（详见 scripting-lua.md §热替换协议）。

## 6. 迁移路线建议（删除导向）

| 阶段 | 动作 | 风险 |
|---|---|---|
| 1. 死代码清理（无行为变化） | 删伪 fruit 命名空间、`getService` stub、Fruit TODO、`DependencyManager`、`lazyInit` 等死字段、`AutoRegister` 未用模板 | 极低：无生产调用方 |
| 2. 配置收敛 | 删 `Apollo::ConfigManager`/`ConfigProperty`；ConfigEnvironment 下沉进 core::config；条件上下文直读 | 低；examples 随改 |
| 3. 容器收敛 | game-server 保持 core::di；删除 `Apollo::ApplicationContext`/`BaseComponent`/注册宏；examples 重写为 core::di 用法；README 特性描述改为实情 | 低；对外无 API 承诺 |
| 4. Starter 收敛 | ApolloStarter 削成装配模板接口（registerBeans + onStart/Stop），开关收敛为 config+注册期过滤，删条件类只留 OnProperty；启动两阶段化（build 存配置 / start 装配） | 中：starter tests 重写 |
| 5. 与 runtime 合流 | Starter 的"默认装配"与 `runtime::ServiceHost`/模块 manifest（docs/architecture 已规划）合流，容器/宿主职责按 docs/34 §12 演进——但以 core::di 为底座 | 与模块化计划（docs/34、35）合并推进 |

## 7. 附录：新旧容器对照

| 维度 | `Apollo::ApplicationContext`（旧，建议删） | `apollo::core::di`（新，建议留） |
|---|---|---|
| 键 | 运行时字符串 | `TypeKey` = 类型局部静态地址（编译期） |
| 注入 | 使用方主动按名查找（Locator） | 构造期 `new Impl(ctx.get<Deps>()...)`（真 DI） |
| 类型安全 | `dynamic_pointer_cast`，运行时 nullptr | 模板 + `static_cast`，编译期 `is_base_of` 校验（`:119`） |
| 读路径 | 全局 mutex | 无锁（哈希查找 + 指针解引用） |
| 元数据 | 构造探针实例读取 | Builder 参数/编译期 |
| 解析时机 | 首次 get 惰性 + 每 tick 可再查 | 启动期 eager 单遍 + 运行期只读 |
| 错误暴露 | 运行时（拼错名/nullptr） | 启动期 assert/初始化失败（可再前移到 build 期校验） |
| 使用方 | examples/tests | **apps/game-server** |

## 8. 跨文档一致性复核（2026-09-27 追加）

### 8.1 范围与方法

- 复核对象：`docs/design/attribute-sync.md`、`docs/design/scripting-lua.md`、`docs/design/net-abstraction.md`、`docs/design/sdk-contract.md`、`docs/analysis/ssengine-reference.md` 与本评审之间的全部互相引用与结论依赖；重点三点：core::di 与旧 IoC/Starter 栈的取舍口径、四套配置系统的收敛口径、README「IoC 容器」第一特性问题是否在设计文档层复发。
- 本节引用的文档行号与源码路径/行号**均于本次复核时实读核对**（记录见 §8.5）。
- 任务约束：本次复核只写本文件，其他文档的修订需求记录于此（§8.2 的"修订建议"），不直接改动。非交互说明：引用落点存在歧义时，以被引文档中该概念的定义处为准；六份文档随提交 a2ab6525，源码基线仍为 35a9c528（无源码变更）。

### 8.2 发现的矛盾（M-1 … M-6）

**M-1 异步 DB 模型："复用"（不存在的资产）vs"新建"——最值得警惕的一条。**
- 位置：`attribute-sync.md:14`、`:274` 写"复用 apollo `modules/data` 的异步 DB 模型（见 ssengine-reference.md §4.1）"；而 `ssengine-reference.md:10`、`:65` 判定 modules/data 目前**只有同步 mock**（MemoryConnection/SimpleDataSource/SqlTemplate），异步命令模型是"最大缺口"、落地形态是**模块级新建**（§4.1）。
- 实测：`modules/data/orm/src/` 现存 memory_connection.cpp / datasource.cpp / sql_template.cpp；全仓 rg `OnExecuteSql|DbCommand|AsyncDb` 无命中（仅 redis 相关文件命中 async 词汇）——异步 DB 模型确实不存在。
- 为什么不合适：attribute-sync 把**不存在的资产当成既有设施引用**——与"README 宣称 IoC 容器而实际无人使用"（本评审执行摘要关键背景、P3 表）是同一类"文档宣称不实"，且发生在设计文档层。
- 修订建议：attribute-sync 两处改为"按 ssengine-reference §4.1 **新建**异步 DB 模型（modules/data 内落地）"；`:274` 的悬空引用"复用 §异步 DB 模型"（不存在的节名）一并修正。

**M-2 契约形态：yaml/idl vs TOML。**
- 位置：`attribute-sync.md:331`（附录表"契约（**yaml/idl 形态**见 sdk-contract.md）"）vs `sdk-contract.md` §2.2/§2.3（契约 = **TOML** `attrs.toml`/`messages.toml`）。
- 修订建议：attribute-sync 附录统一为 TOML（以 sdk-contract 为准）。

**M-3 慢观察者对流控的影响口径：聚合拖慢 vs 隔离重置。**
- 位置：`net-abstraction.md:125`（"flow control 聚合窗口｜多观察者下行聚合：**慢观察者拉低整批发送速率**（AOI 广播的 per-viewer 预算，attribute-sync §5）"）vs `attribute-sync.md` §3.3（ACK 超时 → **该 viewer 本人**降级重发、再超时 → 仅对其快照重置）与 §5.1（per-client token bucket，无聚合拖慢机制）。
- 为什么是矛盾：attribute-sync 的模型是**慢观察者隔离**——慢客户端拖的是自己的 acked_seq，超时只重置它自己，其他观察者的速率与预算不受影响；net-abstraction 把 Aeron 的聚合 flow control 语义硬映射到 per-viewer 模型上，且把 attribute-sync §5 引为出处——出处不成立。
- 修订建议：net-abstraction:125 改为"慢观察者触发**其本人**的 acked_seq 停滞 → 快照重置（attribute-sync §3.3），不拖慢他人；实体级聚合带宽上限如需要，作为 attribute-sync §5 的独立扩展项另行立项"。

**M-4 tick 阶段编号错位。**
- 位置：`scripting-lua.md:96`（"禁止在**阶段 3（simulate 之后）**以外的 tick 阶段改属性"）vs `attribute-sync.md:296-303`（阶段编号：simulate=1、recalc=2、aoidecay=3、collect=4、budget/flush=5、persist-batch=6）。
- 说明：scripting-lua 自身 §5.1 时序图（simulate→recalc→钩子窗口→collect）与 attribute-sync 编号一致，执行摘要 `:12` 的"阶段 2/4 之间"也对——仅 `:96` 一处把"simulate（阶段 1）之后"误写为"阶段 3"。
- 修订建议：scripting-lua:96 改为"禁止在 simulate 阶段（attribute-sync §10 阶段 1）之后的收集/发送阶段改属性…"。

**M-5 术语漂移：`attr_schema_hash` / `attr_schema_version`（含悬空引用）。**
- 位置：`attribute-sync.md:254` 定义 `attr_schema_hash`（登录握手，全文档唯一版本术语）；`scripting-lua.md:70` 使用 `attr_schema_version` 并标注"见 attribute-sync.md §7.2"——**§7.2 并无此概念**；`sdk-contract.md:114`、`:134` 同样使用 `attr_schema_version`；而概念本体在 `attribute-sync.md:58`（L1 行"进视野时**按模板版本**下发引用"）存在但未命名。
- 为什么不合适：实为**两个不同概念**被一个词族混用——① 契约语义哈希（握手校验两端协议一致）；② L1 静态配置的模板版本（随实体快照/模板引用下发，供客户端选解码表）。混用后"握手 hash"承担不了"按版本选表"，"版本号"也替代不了"hash 防漏改"。
- 修订建议：拆成两个词——握手一律 `attr_schema_hash`（算法定义在 sdk-contract §6，attribute-sync §7.2 补算法引用，见 M-5 伴随项 C-5）；实体携带的配置版本命名 `template_version`，补进 attribute-sync §2.1 L1 行与 §6 Spawn（CreateEntity 随 template_id 带上）；scripting-lua:70 与 sdk-contract:114 改引该定义并修正落点。

**M-6 定时器轮"存在性"口径（轻，非矛盾，防误读）。**
- 位置：本评审 §5（`:189`）把"定时器轮"列入"必须稳定（C++ 侧）"清单；`ssengine-reference.md:12`、`:98` 判定 apollo **目前没有**定时器模块、需按目标设计新建。
- 判定：清单描述的是目标架构（热更完成后的稳定面），现状缺件——两文不冲突，但清单易被误读为现状存在。建议 §5 该行补注"（新建件，见 ssengine-reference §4.3）"。

### 8.3 三个重点维度的复核结论（一致确认）

1. **core::di 与旧 IoC/Starter 栈的取舍：四份设计文档与本评审完全同向，未发现矛盾。** scripting-lua §4.1（`:78-86`）把装配唯一锚定在 core::di（启动期注入具名模块表）；net-abstraction §8（交集表）明确"适配器经 core::di 启动期装配、链接期选择、不进旧字符串容器"；sdk-contract 不触及容器；attribute-sync `:36` 甚至把旧模式（`AttributeManager` 全局单例+全局锁查找）作为本评审 P0-1 批判对象引入对照。设计文档均未假设旧容器/旧 Starter/条件装配存在——"删旧留新"结论在设计层无反例。
2. **四套配置系统的收敛口径：无冲突，但收敛目标自身带缺陷（见 C-1/C-2）。** 四份设计文档零处引用 `Apollo::ConfigManager`/`ConfigProperty`/`CONFIG_PROPERTY`；配置相关需求只有 scripting-lua §3.3（版本目录）与 attribute-sync §2.1 L1（模板版本），均与 P1-1"删旧、收敛 core::config"方向一致。但实测发现收敛目的地（core::config）存在与旧系统同款的"reload 不通知 listener"缺陷及内部双单例——P1-1 的收敛动作必须包含修复，否则设计文档的配置热更依赖落空（详见 §8.4）。
3. **README「IoC 容器」第一特性问题：未在设计文档层复发为同类宣称，但出现了一例变体。** 无任何设计文档依赖 README 特性宣称；四份均自标"设计稿（评审中）"。但 M-1（把不存在的异步 DB 当既有资产引用）说明"文档宣称不实"模式已从 README 渗入设计文档——对策固化为 C-3。

### 8.4 补充结论（对收敛路线与文档纪律的增量要求）

- **C-1（收敛目标先自查——新 core::config 复刻了旧缺陷）**：实测 `include/apollo/core/config/config_manager.h:29`（ConfigChangeListener）/`:84`（reload）/`:162`（addListener）/`:242`（notifyListeners 接口）齐全，但实现里 `notifyListeners` **全模块零调用**（`modules/core/config/src/config_manager.cpp:691` 仅有定义处；rg 确认无调用点）——即 reload/loadFile 成功后**不触发任何 listener**，与本评审 P1-1 批判的旧 `ConfigManager.cpp:222-224`"热加载不通知"缺陷**同款**。同时 `FileWatcher.cpp` 被编入 apollo_core_config（`modules/core/config/CMakeLists.txt:7`）但模块内无任何引用（死接线）。→ **收敛动作（§6 阶段 2）必须追加**：loadFile/reload 成功路径调用 notifyListeners；FileWatcher → reload → notify 链路接通；否则 §5.4 的 `reload(changedKeys)` 事件分发与 scripting-lua §3.3 配置热更均无载体。
- **C-2（core::config 内部先收敛）**：实测 apollo::core::config 内部存在**两个并存单例**——`ConfigManager`（`include/apollo/core/config/config_manager.h:41`）与 `ConfigRegistry`/`global_config()`（`modules/core/include/apollo/core/config/config_registry.hpp:41-49`，独立 `shared_mutex + unordered_map` 值存储）。"收敛为一套"的目的地自己就是两套；P1-1 落地时先合并（或明确弃用其一），否则四套收敛后仍剩两套。
- **C-3（设计文档引用纪律）**：由 P3 README 行 + M-1 固化为规约——设计文档引用仓库资产必须标注已实读核对（文件:行）；引用"待建/规划"资产必须显式写"规划中（见 XX §）"，**禁用"复用/已有"等既有一词**。写入 §6 阶段 5 验收标准。
- **C-4（版本化配置目录缺口）**：scripting-lua §3.3 的"分版本目录、新旧并存"与 attribute-sync §2.1 L1 的"模板版本下发"，要求 core::config 新增"版本目录加载 + template_version 解析"能力；实测 core::config 无任何 version 相关代码（rg 无命中）。→ 随收敛动作（阶段 2）立项，作为 P1 配置热更前置。
- **C-5（hash 算法归属闭环）**：attribute-sync §7.2 只说 hash"由契约生成"，sdk-contract §6 定义为 `SHA-256(契约源+生成器版本)`——后者是前者的精化、不矛盾；随 M-5 修订在 attribute-sync §7.2 补一句"算法见 sdk-contract §6"。

### 8.5 实读核对记录（本节引用的路径与行号）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| `README.md:21`「🏗️ IoC容器系统…」为核心特性第一条 | rg + sed（`:20` 标题、`:21` 首条） | 属实 |
| `apps/game-server/src/main.cpp:96-99` core::di Builder | sed 90-105 | 属实（`:96` builder、`:97-98` add_singleton、`:99` build()；本评审 :96-102 引用范围覆盖） |
| `modules/core/config/CMakeLists.txt:6`（旧 ioc ConfigManager.cpp 编入）、`:7`（FileWatcher.cpp） | cat | 属实 |
| `include/apollo/core/config/config_manager.h:29/41/84/162/242` | rg | 属实 |
| `modules/core/config/src/config_manager.cpp:691` notifyListeners 零调用 | rg 全模块仅定义处 | 属实（新发现 C-1） |
| `modules/core/include/apollo/core/config/config_registry.hpp:41-49` 独立单例 | cat | 属实（新发现 C-2） |
| `modules/data` 无异步 DB（orm/src/{memory_connection,datasource,sql_template}.cpp 为同步 mock；全仓 rg OnExecuteSql/DbCommand/AsyncDb 无命中） | find + rg | 属实（支撑 M-1） |
| 全仓 `*.proto` 零命中 | find | 属实（net-abstraction §1 断言成立） |
| `include/apollo/net/adapters/sdnet_adapter.h:8,12-80` 伪 SSCP | 读码复核 | 属实（net-abstraction §6 删除判定的事实基础） |
| 文档互引行号：attribute-sync `:14/:58/:254/:274/:331`、scripting-lua `:12/:70/:96`、net-abstraction `:125`、sdk-contract `:13/:108/:114/:134`、ssengine-reference `:10/:65` | rg 逐条 | 属实（矛盾即当条所指） |
| `docs/qa/q101-script-language.md` 存在 | ls | 属实（scripting-lua 引用成立） |

## 9. 源码级核对第二轮（2026-09-27 追加，承接 C-1/C-2）

### 9.1 范围与方法

- 下一批线索：① 收敛目标 `apollo::core::config` 的生产消费方普查（承接 C-2 双单例）；② FileWatcher 全仓接线普查（承接 C-1 死接线）；③ 新 ConfigManager 的热路径形态（P1-1 批判是否延伸到收敛目标）；④ P1-2「HAVE_FRUIT 从未定义」断言复核；⑤ 旧栈影响面计数复核（P0-1「examples 6 个」）；⑥ 保留路径 `runtime::IHostedService/ServiceHost` 落点核实（P2-4/§4）；⑦ 承重断言抽查（探针构造/条件时序/autoEnabled/依赖排序注释/旧 ConfigManager stoi 路径）；⑧ 引用文档（docs/34、docs/architecture）引语逐条对文。
- 方法不变：每条结论先实读文件：行号再落笔（核对记录见 §9.4）；源码零改动，本文件之外无写入。

### 9.2 新发现（C-6 … C-12）

**C-6 core::config 生产消费方普查（承接 C-2）：双单例实为「Registry 有用户没内容，Manager 有内容没用户」。**
- `ConfigRegistry`（`global_config()`）：生产使用仅 `apps/game-server/src/main.cpp:68-70`（set `server.name`/`server.max_players` 两键）与 `:122-123`（读回打印）——自设自读的演示级用法。
- `ConfigManager`（`instance()`）：**生产读者为零**。唯一生产写入者是旧 Starter：`src/starter/ApolloApplication.cpp:327-328` `loadFile(..., "starter" section)`——把文件喂进去之后没有任何生产代码读它。
- `modules/runtime/src/application_host.cpp:1` 仅 include `config_registry.hpp`，全文件无任何 config 符号使用（`notify_config_loaded`，`:133-135`，是生命周期事件广播，与配置值无关）——未使用的 include。
- 便捷宏 `APOLLO_CONFIG_INT/STR/BOOL/DOUBLE`（`modules/core/config/include/apollo/core/config/config_manager.hpp:12-18`）：生产零调用，仅 `tests/test_config.cpp`。
- **收敛动作细化（更新 P1-1 建议）**：收敛为一套时保留 Registry 形态（game-server 已在用的那半），把 ConfigManager 的文件加载/多格式/listener 能力并入或独立为 Loader，删除宏层；否则 C-2 的双单例在收敛后仍会以「一空一满」形态存续。

**C-7 新 ConfigManager 热路径形态：P1-1 的批判延伸到收敛目标本身。**
- 每次 `get*` = `shared_lock`（`modules/core/config/src/config_manager.cpp:241-247` 的 `get()`）→ `configs_.find(section)` → `root.getByPath(key)`（`include/apollo/core/config/config_value.h:138-140`，children_ 树逐段路径解析）→ 字符串存储值每次调用 `std::stoll/std::stod` 现场解析（`config_value.h:61-75`，try/catch 吞异常返回默认值）。
- 比旧系统好的三点：`shared_mutex`（读并发）、variant 单一存储（无 string+any 双缓存）、解析失败回退默认值而非抛异常。
- 但形态仍是**每调用「锁 + 查找 + 解析」**——P1-1「每 tick 读一次战斗数值就是一次锁+解析」的批判同样适用于收敛目标。→ 收敛验收标准追加：高频读取走「启动期绑定类型化快照」（P1-1 建议原文），ConfigManager/Registry 只承担低频与运维读。

**C-8 P1-2 断言修正：「HAVE_FRUIT 在构建里从未定义」不准确。**
- 实测根 `CMakeLists.txt:336-345`：`HAVE_FRUIT=1` **是有条件定义的**——条件为硬编码路径 `${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/packages/fruit_x64-osx` 存在（注意 **x64-osx 三元组硬编码**，Linux/CI 上不存在该目录）；else 分支定义 `APOLLO_USE_BUILTIN_DI=1`（`:346-348`）。
- 修正后表述：在本仓库实际构建平台（Linux/CI）上 HAVE_FRUIT **实际不会定义**，伪 fruit 分支照常编译；但在硬编码路径存在的机器（macOS 开发机）上会链接 `libfruit.a` 并整个跳过伪 fruit——**ODR 风险从「死代码」升级为「平台相关的条件编译」**。删除建议不变，且构建系统记一笔新缺陷：平台路径硬编码。
- 附带核实：`ENABLE_FILEWATCHER` 选项默认 ON（`CMakeLists.txt:36`、`:348-352`）——FileWatcher 默认参与编译，但其唯一消费者见 C-9。

**C-9 FileWatcher 全仓接线普查（承接 C-1）：唯一真实消费者是注定被删的旧 ConfigManager。**
- 全仓消费者仅三处：`include/apollo/framework/ioc/ConfigManager.h` + `src/framework/ioc/ConfigManager.cpp`（旧配置管理器，`:222-224` 的 `onFileChanged → loadFromFile` 即 P1-1 批判的哑热加载）；`tests/CMakeLists.txt`；`modules/core/config/CMakeLists.txt:7`（编入 apollo_core_config 但模块内零引用，C-1 已记录）。
- 头/实现分居：`include/apollo/utils/config/FileWatcher.h` vs `src/utils/io/FileWatcher.cpp`。
- **结论**：P1-1 收敛完成后 FileWatcher 即整体死代码——收敛动作清单必须显式二选一：按 C-1 接线进新配置（watch → reload → notify，配置热更需要它），或随旧栈一并删除并在新配置里重新实现。

**C-10 旧栈影响面计数修正：**
- P0-1 影响面原文「examples 6 个、tests 若干」→ 实测（rg `BaseComponent|ConfigProperty|CONFIG_PROPERTY|framework/ioc|apollo/starter`）：examples **5 个**（all_features_demo / basic_example / config_example / dependency_example / starter_example）、tests **6 个**（component/config/context/dependency_test_simple、main_test、test_simple）+ tests/CMakeLists.txt。
- 结论不变（生产调用方为零），计数修正。

**C-11 保留路径落点核实：`runtime::IHostedService` 与 P2-4 建议完全对齐。**
- `modules/runtime/include/apollo/runtime/application_host.hpp:40-47`：`IHostedService` = `service_name()/start()/stop()/is_running()/tick()` 五方法——与 P2-4「接口瘦身为 start/stop（或直接复用 IHostedService：service_name/start/stop/is_running/tick）」逐字吻合；`ServiceHost` 在 `:91`，`run_once()` 在 `:115-116`。
- 结论：P2-4 的落地目标真实存在且已在 game-server 使用（main.cpp:105），保留建议可执行。

**C-12 引用文档引语逐条对文（全部属实，含一处加固）：**
- `docs/34-ApplicationContext_2.0_Design.md:80`「dependsOn 必须显式声明…禁止依赖『注册顺序』」（§3 约束块）——P2-1 引用属实。
- `docs/34:415`「## 15. 风险与约束」存在，`:419`「调度器若进入高频游戏逻辑主循环，会和场景 Tick 职责冲突」——§4「docs/34 §15 自己已警告风险」引用属实；另有 `:323`（§10 内）「scheduleCron 只用于管理和运维任务，不进入高频业务 Tick」加固同一论点。
- `docs/34:332-335`、`:406` /health /metrics /beans 三端点——§4 管理端点行属实；`docs/34:350`「## 12. 与 ApolloApplication 的衔接」——§6 阶段 5 的「按 docs/34 §12 演进」引用属实。
- `docs/architecture/starter-and-module-assembly-design.md:19`（「像 Spring Boot 那样…不照搬 Java 那套运行时反射模型」）、`:103`（「Apollo 不该照搬 Spring Boot 的部分」）、`:128`（「C++ 版 Spring Boot 克隆」）——§1/§4 引用属实。

### 9.3 对既有结论的修正汇总

| 位置 | 原表述 | 修正 | 结论是否变化 |
|---|---|---|---|
| P1-2 影响面 | 「HAVE_FRUIT 在构建里从未定义」 | 有条件定义（硬编码 x64-osx 路径，CMakeLists.txt:336-345），Linux/CI 上实际不定义 | 不变（删除建议不变），风险定性升级（C-8） |
| P0-1 影响面 | 「examples 6 个、tests 若干」 | examples 5 个、tests 6 个 | 不变（C-10） |
| P1-1 细节 | 旧 getValue 双重加锁 + `:135` stoi 抛异常 | 复核属实（getValue(key,default)=`:66-71`，stoi=`:135`） | 不变，行号精确化 |
| §4 core::di 行 | 「补两处：try_get assert、懒加载并发」 | 本轮未复触及，维持原判 | 不变 |
| §4 EventBus/Scheduler 行 | 「docs/34 §15 自己已警告」 | 属实且 §10:323 加固 | 加固（C-12） |

### 9.4 实读核对记录（第二轮）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| `apps/game-server/src/main.cpp:68-70,122-123` global_config 自设自读 | rg + sed | 属实（C-6） |
| `src/starter/ApolloApplication.cpp:327-328` ConfigManager::instance().loadFile("starter")；`:351` sync；`:365` 旧 getAllKeys | rg | 属实（C-6，佐证 P1-1 缝合点） |
| `modules/runtime/src/application_host.cpp:1,133-135` 仅 include 无使用 | rg | 属实（C-6） |
| `modules/core/config/include/apollo/core/config/config_manager.hpp:12-18` APOLLO_CONFIG_* 宏；生产零调用 | rg + cat | 属实（C-6） |
| `modules/core/config/src/config_manager.cpp:241-247` get() shared_lock+getByPath；`:251-293` get* 现场转换 | sed | 属实（C-7） |
| `include/apollo/core/config/config_value.h:16`（variant 存储）、`:61-75`（stoll/stod per read）、`:138-140`（getByPath 声明） | sed | 属实（C-7） |
| `CMakeLists.txt:336-348` FRUIT_ROOT 硬编码 x64-osx / HAVE_FRUIT=1 / else APOLLO_USE_BUILTIN_DI=1；`:36,348-352` ENABLE_FILEWATCHER ON | sed | 属实（C-8，修正 P1-2） |
| FileWatcher 消费者=旧 ConfigManager 两文件 + tests/CMakeLists + core/config CMake | rg 全仓 | 属实（C-9） |
| 旧栈 dependents：examples 5、tests 6 | rg 清单 | 属实（C-10，修正 P0-1 计数） |
| `modules/runtime/include/apollo/runtime/application_host.hpp:40-47,91,115-116` | rg + sed | 属实（C-11） |
| `ApplicationContext.h:36` 探针构造；`ApolloApplication.cpp:262 vs 271,276-278` 条件时序；`:401` autoEnabled 压过显式启用；`StarterRegistry.h:84-98` 注释与实现不符 | sed 逐条 | 属实（承重断言复核） |
| `src/framework/ioc/ConfigManager.cpp:218-224` listener 循环属 setValue 路径；onFileChanged 仅 loadFromFile | sed | 属实（P1-1「哑热加载」） |
| docs/34 `:80` / `:323` / `:332-335,406` / `:350` / `:415,419`；architecture `:19,103,128` | rg + sed | 属实（C-12） |

## 10. 源码级核对第三轮（2026-09-27 追加）

### 10.1 范围与方法

- 四个候选区域：① §6 迁移路线各阶段对应的真实调用点；② P1-1 收敛清单与 FileWatcher 去留二选一；③ C-7 启动期类型化快照验收口径；④ P2-2 条件装配时序缺陷实证。普查外溢出的新线索一并收录（条件原语全灭、database 模块第二套静态注册脚手架、属性层双代实现与双树复制）。
- 方法同前：先实读文件：行号再落笔（记录见 §10.4）。非交互说明：对「二选一」类决策（FileWatcher 去留），本轮给出判据与倾向、不替执行者拍板，倾向已注明（C-16）。

### 10.2 新发现（C-13 … C-17）

**C-13 属性层「双代同目录 + 双树同文件」——attribute-sync.md 现状审计的补正。**
- `include/apollo/game/attributes/` **同一目录、同一命名空间**下有两个同名类 `AttributeContainer`：生成 A `attribute.hpp:12,52`（mutex + variant + `AttributeManager` 单例，attribute-sync.md §0.2 的批判对象）与生成 B `attribute_value.h:11-12,74`（getInt/ComVal 形态，另有 `AttributeContainerManager` `:230`）。同名同类名同命名空间——同二进制同时链接即 ODR 冲突，读者必然混淆。
- 生成 B 的实现文件**双树复制且字节一致**：`src/apollo/game/attributes/attribute_value.cpp` 与 `modules/game/attributes/src/attribute_value.cpp` diff IDENTICAL（各 403 行）——与 `sdks/`/`skds/` 事故（sdk-contract.md §1）同类的源码版。且 `src/apollo/` 是一整棵平行遗留树（ipc/database/config/game…）。
- 影响：attribute-sync.md §0.2 的现状批判只覆盖生成 A；「重建骨架」结论**加强**而非削弱，但 §0.2 应补记生成 B 与双树事实，删除清单扩大为「两代容器 + src/apollo 遗留树处置」。

**C-14 条件原语普查：五种条件中三种是恒假死谓词，且全部五种零用户——P2-2 的实证要求在此闭合。**
- `markClassAvailable`（`ConditionContext.h:120-122`）**全仓零调用** → `ConditionalOnClass`（`Conditional.h:67-73`）恒假。
- 求值时序实证：`src/starter/ApolloApplication.cpp` 的执行顺序为 createConditionContext(`:258`) → getMatchingStarters(`:262`，`matches()` 在此执行，`StarterRegistry.h:105-113`) → markStarterEnabled(`:267-269`) → registerBeans(`:271-273`) → markBeanPresent(`:276-278`)。**matches 执行时三个 mark 集全部为空** → OnBean 与 **OnStarter** 同样恒假——原 P2-2 只判了 OnBean 时序缺陷，实测 OnStarter 同灭（同一时序缺陷的另一受害者）。
- OnProperty 可用：properties 在 `:258` 已由 createConditionContext 灌入，先于 `:262` 求值。
- 用户普查：五种条件原语在 src/examples/tests 中**零使用**（唯一的条件实现是 `tests/main_test.cpp:158` 自定义 `matches()` 覆盖）。
- 修正：P2-2「保留 OnProperty」措辞不当——现实现无任何用户，随删随建等价，应为「**新建**约 30 行的 OnProperty + 注册期单趟过滤」。

**C-15 第二套静态注册脚手架（database 模块，暂无用户）。**
- `include/apollo/database/sql_template.h:272-284` `APOLLO_REGISTER_DATABASE` 宏：匿名命名空间静态对象在 main 之前调用 `DatabaseRegistry::registerFactory`——与 P2-1 批判的 `REGISTER_COMPONENT` 同构且是**独立复制的第二份**；`DatabaseRegistry::loadPlugins(pluginDir)`（`:266`）还预埋了运行时插件目录加载。宏内的本地 `struct AutoRegister`（`:277-283`）与 ioc 的 `AutoRegister<T>`（`ComponentRegistry.h:25-31`）无引用关系。
- 全仓零用户（rg 仅宏定义处）。→ 不构成活体风险；§6 阶段 1 死代码清单应扩入该宏与 loadPlugins 的去留，且 P2-1 的规约（注册发生在 main 早期、显式 Builder）应写成**全仓约束**而非仅 ioc 栈约束。

**C-16 FileWatcher 去留二选一：判据齐备，倾向保留并接线。**
- 实测 `include/apollo/utils/config/FileWatcher.h`：Linux inotify（`:23`）、macOS 分支注释（`:19`）、`poll.h` 轮询兜底（`:17`）、专用线程 `watchLoop`（`:46,58`）+ `watchesMutex_`（`:60`）+ `pollInterval_`（`:61`）、API `addWatch(path, callback)`（`:36`）。**是真实现，不是空壳**——ssengine-reference.md:130「已有 inotify/poll 实现」引语属实。
- 判定倾向：**保留并按 C-1 接线**（watch → reload → notify）。理由：配置热更（scripting-lua.md §3.3）与脚本 dev 热载（ssengine-reference.md §6）都以它为读取底座，重写无收益。接线时须改造两点：回调从 watcher 线程改为投递回主线程 tick 边界执行（对齐 scripting-lua.md §3.2 的线程分工）；消费者从旧 ConfigManager 迁至 core::config。倾向非决定，拍板留迁移阶段 2。

**C-17 C-7 验收口径的前置事实核查：现状零违例，验收属预防性约束。**
- game 模块（`include/apollo/game`、`modules/game`）**零配置读取**（rg getValue/APOLLO_CONFIG/ConfigManager 零命中——命中的 getInt 是属性容器自身接口，`attribute_value.h:142-143`）；game-server 对 config 仅启动期 set×2 / get×2（`main.cpp:68-70,122-123`）。
- 警示先例在库：`ConfigProperty`（`ConfigProperty.h:22-25` set 写穿、`:31-35` 首读缓存永不刷新）正是「无刷新事件的类型化快照」半成品——C-7 验收要防的形态已有前车。
- 验收口径落为三条：①绑定发生在启动期装配点；②`reload(changedKeys)` 事件驱动刷新（依赖 C-1 接线）；③高频读取零锁零解析（纯成员读）。现状无违例，违约风险集中在未来 game 玩法代码接入配置时。

### 10.3 对既有结论的修正

| 位置 | 原表述 | 修正 | 结论是否变化 |
|---|---|---|---|
| P2-2 | 「删除 OnClass/OnBean，保留 OnProperty」 | 实证升级：OnClass/OnBean/**OnStarter** 三者恒假（matches 时 mark 集全空，`:258-278` 顺序）；五种条件原语全部零用户 → OnProperty 为**新建**而非保留 | 删除范围扩大（OnStarter 提前入列），落地形态不变 |
| attribute-sync §0.2（外部文档，记录待补） | 现状批判只覆盖 attribute.hpp 一代 | 补记生成 B（attribute_value.h/comval.h 同名类）与 src/apollo 双树复制（C-13） | 重建结论不变、删除清单扩大 |
| §6 阶段 1 | 死代码清单：伪 fruit、getService stub、Fruit TODO、DependencyManager、lazyInit、AutoRegister | 逐项核实零消费方 ✓；清单扩入 database 模块 `APOLLO_REGISTER_DATABASE`/`loadPlugins` 去留（C-15） | 扩充 |
| P1-3 影响面 | 「仅 examples/dependency_example.cpp 与 tests」 | 实测精确化：examples 1 个 + tests 3 个（dependency_test_simple / test_simple / main_test） | 不变 |
| §6 阶段 5 | 「模块 manifest（docs/architecture 已规划）」 | `docs/architecture/starter-and-module-assembly-design.md:344`「十一、推荐 manifest 结构」属实 | 确认 |

### 10.4 实读核对记录（第三轮）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| DependencyManager 消费方 = examples 1 + tests 3 | rg 全仓 | 属实（P1-3 精确化） |
| `BeanDefinition.h:16` lazyInit 零代码消费（docs/07:123,672、docs/34:69 为文档层） | rg | 属实（P3 行） |
| ioc `AutoRegister`（ComponentRegistry.h:25-31）零用户；sql_template.h:277-283 为 database 本地同名 struct | rg | 属实（§6 阶段 1；C-15） |
| `Starter.h:67` dependsOn 唯一出现（声明处），全仓零读取 | rg | 属实（P2-3「声明了没人实现」升级为「连读取者都没有」） |
| `unregisterComponent` 仅 tests/main_test.cpp 调用（8 处） | rg | 属实（§5.3「危险品」判定） |
| `ConditionContext.h:120-122` markClassAvailable 零调用 | rg 全仓 | 属实（C-14 恒假判据） |
| 求值顺序 `ApolloApplication.cpp:258→262→267-269→271-273→276-278`；`StarterRegistry.h:105-113` matches 求值点 | sed | 属实（C-14 实证） |
| 条件原语用户 = 零（tests/main_test.cpp:158 Custom matches 唯一） | rg examples tests | 属实（C-14） |
| `sql_template.h:266` loadPlugins、`:272-284` 注册宏，零用户 | rg + sed | 属实（C-15） |
| `FileWatcher.h:17,19,23,36,46,58,60-61` inotify/poll/线程/mutex | rg + sed | 属实（C-16；ssengine-reference:130 引语核实） |
| game 模块零配置读取；`attribute_value.h:142-143` 为属性接口非配置 | rg | 属实（C-17） |
| `ConfigProperty.h:22-25,31-35` 写穿 + 首读缓存 | sed | 属实（C-17；round 1 P1-1 复核） |
| 双 `AttributeContainer`：`attribute.hpp:12,52` vs `attribute_value.h:11-12,74,230` 同命名空间 | rg | 属实（C-13） |
| `src/apollo/game/attributes/attribute_value.cpp` ≡ `modules/game/attributes/src/attribute_value.cpp`（403 行，diff IDENTICAL） | diff | 属实（C-13） |
| `ApplicationContext.h:267-275` makeRuntimeInfo，`:270` autoStart 仅拷贝 | sed | 属实（P3 行复核） |
| `docs/architecture:344` manifest 章节 | rg | 属实（§6 阶段 5） |

## 11. 源码级核对第三轮·续（2026-09-28 追加）

### 11.1 范围与方法

- 上轮遗留两方向：① C-16 FileWatcher 去留的**阶段 2 改造点细化**（实读 FileWatcher.cpp 全文 + core::config 侧接线契约各挂点）；② §6 阶段 1 死代码清单（含 C-15 扩入项）**逐项消费方复核**。
- 方法不变：先实读文件：行号再落笔（记录见 §11.4）。非交互假设注明：接线契约的监听语义按 core::config 既有 `ConfigListener` 结构（key 空 = 监听全部）为准；C-19 的三步细化属设计建议，拍板留阶段 2 执行时。

### 11.2 新发现（C-18 … C-22）

**C-18 FileWatcher 实为「死 inotify + mtime 轮询」——C-16 的定性修正（sdtimer 式文档/实现错位在本仓再现）。**
- inotify 注册但**从不消费**：`addWatch` 初始化 `IN_NONBLOCK` 实例并 `inotify_add_watch`（`src/utils/io/FileWatcher.cpp:53-71`），但全文件无任何 inotify 事件 read/处理——`watchLoop`（`:136-141`）只做 `sleep(pollInterval) + checkForChanges()`；实际变更检测是**纯 mtime 轮询**（`checkForChanges` `:143-166` 逐 watch `stat` 比对 lastModified；`:168-184`）。`stop()` 里的 inotify 清理（`:109-119`）清理的是一个从未读过的 fd；头注释「For macOS, we don't have inotify」（`FileWatcher.h:19`）暗示 Linux 走 inotify 事件——实际不走。
- 修正 C-16 与 ssengine-reference.md:130 的引语：「真 inotify + 专用线程」→「**专用线程 + mtime 轮询，附一套从未消费的 inotify 死代码**」——与 ssengine-reference §5 教训 1（sdtimer.h 宣称分层、实为 vector 扫描）同一模式在本仓复现。
- 新增缺陷（即阶段 2 改造点的事实基础）：
  - **a) 回调持锁执行**：`checkForChanges` 在 `watchesMutex_` 锁内直接调 `info.callback`（`FileWatcher.cpp:144,154-156,161-163`）——回调慢或重入 addWatch/removeWatch 即死锁；`stop()`（`:99-101`）的 join 会被卡死的回调拖住。
  - **b) 回调在 watcher 线程执行**：C-16 改造点 (a) 的实锤——现行唯一消费者旧 `ConfigManager::onFileChanged`（`ConfigManager.cpp:222-224`）就在该线程直接 `loadFromFile`。
  - **c) 秒级 mtime + 默认 1s 轮距**：`stat().st_mtime` 秒粒度（`:178-181`）+ `pollInterval_` 默认 1000ms（`:8`）→ 同秒内多次写可漏报、检测延迟最高 ~2s。
  - d) `removeWatch` 只删 watches_ 不 `inotify_rm_watch`（`:77-79`）——inotify 本就死代码，随清理。
  - e) 命名空间是旧 `Apollo::`（`FileWatcher.h:27`）——接入 core::config 需迁移或适配。

**C-19 阶段 2 接线契约细化（FileWatcher → core::config → notifyListeners，三步替代 C-1/C-16 的粗粒度两条）。**
- core::config 侧既有挂点（实读）：监听器存储 `struct ConfigListener { key; callback; }`（`include/apollo/core/config/config_manager.h:212-218`，key 空 = 监听全部，vector + `listenerMutex_`）；`notifyListeners(key, node)`（`:242` 声明；`config_manager.cpp:691` 定义、零调用——C-1）；`reload(section)`（`:84`；`config_manager.cpp:153-186`，hash 比对后 loadFile）；`setValue`（`config_manager.cpp:294-299`，**不通知**）。
- 三步改造：
  1. **通知点**：loadFile 成功路径按 key 比对旧/新 ConfigNode 子树，仅对变化 key 调 `notifyListeners(changedKey, newNode)`（同时覆盖 reload 与显式 loadFile 两个入口）；setValue 补同一通知（旧 ConfigEnvironment sync 经 setValue 写入的路径才能被感知——`ApolloApplication.cpp:351` 的缝合点才能在下层可观测）。
  2. **线程边界**：FileWatcher 回调只做「事件入队」（MPSC/有锁环均可），主线程 tick 边界消费 → reload → notifyListeners——对齐 scripting-lua.md §3.2 的线程分工与 attribute-sync.md §10 单写者纪律，同时解除 C-18a 持锁回调。
  3. **消费者迁移**：旧 ConfigManager 的 watcher 生命周期（`ConfigManager.cpp:183-203` raw new）随 P1-1 删除；FileWatcher 实例归属 core::config（或独立 FileWatchService 经 core::di 装配），命名空间随迁（C-18e）。
- 倾向不变（保留改造而非重写）；若执行时改选重写，死 inotify 部分（`:53-71,109-119`）无保留价值。

**C-20 §6 阶段 1 死代码清单逐项消费方复核——全部属实，三项细化。**

| 清单项 | 消费方实测 | 判定 |
|---|---|---|
| 伪 fruit 命名空间（Starter.h:3-31） | `fruit::` 符号消费**全部位于 starter 栈自身**（Starter.h:93-94,115-116；ApolloApplication.h:124,191,201；ApolloApplication.cpp:291,410,417），零外部消费 | 删，零外部影响 |
| `getService<T>` stub（ApolloApplication.h:128） | 全仓零调用（ipc 的 getServiceNames 系无关同名） | 删 |
| `getInjector`（ApolloApplication.h:124）+ `injector_`（:201） | 零调用、仅声明 | 删 |
| `combineStarterComponents`（:191 / ApolloApplication.cpp:410） | 零调用 | 删 |
| Fruit TODO（ApolloApplication.cpp:291） | 注释内代码 | 删 |
| DependencyManager / lazyInit / ioc AutoRegister | §10 已核（examples 1 + tests 3 / 零 / 零） | 删 |
| **autoStart 细化** | **两处定义**：`BeanDefinition.h:15` 与 **`:36`（BeanRuntimeInfo 自己也有）**；唯一"读"是 `ApplicationContext.h:270` 的拷贝，无行为读者 | 双死字段，P3 行补 `:36` |
| APOLLO_REGISTER_DATABASE 宏（sql_template.h:272-284） | 宏零用户（§10 已核） | 删宏 |
| **loadPlugins 细化**（src/apollo/database/sql_template.cpp:353） | 零调用 | 删 |
| **DatabaseRegistry 细化** | **非全死**：`SqlTemplate` 构造内部消费 `DatabaseRegistry::create(type)`（`src/apollo/database/sql_template.cpp:382`）——注册侧（registerFactory，仅死宏可喂）死、消费侧（create）活 | 只删注册侧脚手架；create 路径随 database 层收敛另行处置 |

**C-21 双树复制清单再扩 + 平行 database 双层定型。**
- 第三、四个字节级复制文件对：`src/apollo/database/sql_template.cpp` ≡ `modules/data/orm/src/sql_template.cpp`（diff IDENTICAL）、`src/apollo/database/datasource.cpp` ≡ `modules/data/orm/src/datasource.cpp`（diff IDENTICAL）——加 C-13 的 attribute_value.cpp，双树复制共 **3 对**。
- 结构定性：`apollo::database`（`include/apollo/database` + `src/apollo/database`，含注册宏）与 `apollo::data::orm`（`modules/data/orm` 自带头文件 `sql_template.hpp`）是**平行两代 database 层**——与旧/新容器（§7）、双单例配置（C-2）同构的**第三组「新旧并存」**。→ §6 阶段 1 应增「双轨收敛」专项：src/apollo 遗留树中已被 modules/data 取代的 database 部分，与 C-13 的 game 属性双代一并处置。
- 附带：`ENABLE_FILEWATCHER` 编译宏**零读者**（rg 全 src/include 无消费；CMakeLists.txt:348-352 定义处）——纯死构建开关，与 HAVE_FRUIT 条件（C-8）、hotReload 旋钮（C-22）同类。

**C-22 core::config 内置热重载旋钮同样是死脚手架，与 FileWatcher 不得并存。**
- `enableHotReload(bool, interval)`（`config_manager.h:172`；`config_manager.cpp:341-343`）+ `update()` 周期检查（`:347-368`，门控 `hotReloadEnabled_` 默认 false，`:221-223`）：**enableHotReload 全仓零调用**（rg 全 apps/modules/examples/tests 仅定义处；`.update()` 命中均为无关类）——内置热重载无人启用、无人驱动。
- 阶段 2 决策约束：热重载驱动机制**二选一**——FileWatcher 事件驱动（C-19 方案，删除 update() 旋钮），或主循环驱动 update()（删除 FileWatcher 依赖）。两套机制不得同时保留（与 net-abstraction.md §6 对 nng_wrapper/Aeron 的「二选一不并存」同一纪律）。

### 11.3 对既有结论的修正

| 位置 | 原表述 | 修正 | 结论是否变化 |
|---|---|---|---|
| C-16（§10） | 「真 inotify + 专用线程实现，不是空壳」 | 专用线程 + mtime 轮询；inotify 为注册后从未消费的死代码（C-18） | 「保留改造」倾向不变，改造点细化为 C-19 三步 |
| ssengine-reference.md:130（外部文档，记录待补） | 「已有 utils/config/FileWatcher.h inotify/poll 实现」 | 应为「线程 + mtime 轮询实现（inotify 死代码）」 | 引语修正 |
| P3 表 autoStart 行 | BeanDefinition.h:15 单处死字段 | `:15` 与 `:36`（BeanRuntimeInfo）双处，唯一读者 `:270` 拷贝 | 扩一处 |
| C-15 隐含口径 | DatabaseRegistry 随宏整体淘汰 | 注册侧死、消费侧活（sql_template.cpp:382 内部消费 create） | 只删注册脚手架 |
| C-8 附带 | ENABLE_FILEWATCHER 默认 ON | 宏零读者，纯死开关 | 加重（装饰性选项） |
| ssengine-reference.md:60/:101 与 §5 M-6 修正注 | 「现状无定时器模块」 | 普查范围为 modules/，src/apollo/core/timer 实有完整时间轮实现（C-23）；但该实现 broken 且零消费方（C-24） | 「按目标设计新建时间轮」结论不变，执行顺序补「先删 legacy 实现与其必败测试」 |

### 11.4 实读核对记录（第三轮·续）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| `FileWatcher.cpp:53-71`（inotify 注册）、全文无 inotify read；`:136-141` 轮询循环；`:143-166` mtime 比对 + 锁内回调；`:168-184` stat 秒级 | Read 全文 | 属实（C-18） |
| `FileWatcher.h:19`（macOS 注释）、`:27`（namespace Apollo）、`:36`（addWatch API） | cat 全文 | 属实（C-18e） |
| `ConfigManager.cpp:222-224` onFileChanged → loadFromFile（watcher 线程直载） | sed | 属实（C-18b） |
| `config_manager.h:212-218` ConfigListener（key 空=全部）；`:221-223` hotReload 字段；`:242` notifyListeners 声明 | sed | 属实（C-19/C-22） |
| `config_manager.cpp:691` notifyListeners 定义且零调用；`:294-299` setValue 不通知；`:153-186` reload | sed + rg | 属实（C-1 复核 + C-19） |
| `config_manager.cpp:341-343` enableHotReload、`:347-368` update() 门控；enableHotReload 全仓零外部调用 | sed + rg | 属实（C-22） |
| `fruit::` 消费全部在 starter 栈自身（7 处引证行号） | rg 全仓 | 属实（C-20） |
| getService/getInjector/combineStarterComponents 零调用 | rg | 属实（C-20） |
| autoStart 双定义 `BeanDefinition.h:15,:36`；唯一读 `ApplicationContext.h:270` | rg | 属实（C-20 细化） |
| `src/apollo/database/sql_template.cpp:382` DatabaseRegistry::create 内部消费；`:353` loadPlugins 零调用 | rg + sed | 属实（C-20 细化） |
| 双树复制 3 对均 diff IDENTICAL（attribute_value.cpp / sql_template.cpp / datasource.cpp） | diff | 属实（C-21） |
| `ENABLE_FILEWATCHER` 源码零读者 | rg | 属实（C-21 附带） |

### 11.5 第四轮（2026-09-28 追加）：未评审子系统——定时器 / 日志 / 场景与 AOI（C-23 … C-28）

范围与方法：取 §1-§10 未覆盖的子系统（定时器、日志、场景/AOI/会话管理），方法不变：先实读后落笔（新增核对行见上表下方补充与本节引文）。非交互假设注明：受只读约束**未执行构建与 ctest**——C-26 的链接失败与 C-24e 的测试必败均为「符号调用点 × 编译归属 × 源码推演」结论，已在条目内标注推演性质，未以链接器/运行时输出验证。

**C-23 遗留树存在完整定时器子系统——「现状无定时器模块」被证伪（修正见 §11.3）。**
- 实物三件：`include/apollo/core/timer/timer.h`（TimerId/TimerCallback/ITimerCallback，:14-53）+ `timer_manager.h`（4 层 × 256 槽分层时间轮，:149-153）+ `src/apollo/core/timer/timer_manager.cpp`（363 行实现）。`timer.h:32` 自注「提供类似于 SSEngine ISSTimer 的接口风格」——模仿对象直书，与伪 fruit/伪 @Conditional 同属「Java/SSEngine 形态移植」家族。
- 消费方普查：**零真实消费方**——`src/apollo/server/game_server.cpp:8` 仅 include（全文件无 setTimer/update 调用；TimerComponent `:347-362` 是 cout stub）；唯一使用者是 tests/test_timer.cpp。

**C-24 TimerManager 实现三重断裂：>1 tick 定时器永不触发、无锁 unlock UB + 锁泄漏、跨线程数据竞争——注册在案的测试与实现直接矛盾。**
- (a) **永不触发**：`calculatePosition`（:205-232）的层级判定取「首个移位后为 0 的层」——只有 ticks==0（interval ≤ resolution）落 level 0；ticks 1..255 全落 level 1 且 slot=ticks ∈ 1..255（:224-229）。而级联仅在 slotIndex==0 且该层索引为 0 时触发（:262-269），`cascadeTimer` 读的恰是索引 0 的槽（:320）——level ≥1 的 0 号槽永远为空 → **除 ≤1 tick 外的一切定时器永不触发**；且槽位不含当前 tickCount（相位错位），即便触发时刻也不准。
- (b) **UB + 死锁**：`executeTimer`（:344-359）做 `mutex_.unlock()/lock()`，但 update→processTick→processSlot 全路径无人持锁（:146-169/:253-273/:275-313 无一处加锁）→ 对未持有互斥 unlock 是 UB；单线程推演：首个回调后锁停留 ：358 无人释放 → 后续 setTimer/killTimer/析构（:54-57）自死锁。
- (c) **数据竞争**：update 路径无锁增删 `timers_`/`wheels_`（:287-309），与持锁的 setTimer/killTimer（:65/:124）并发即 UB——test_timer_thread_safety（test_timer.cpp:303）恰是多线程用例。
- (d) 头注释宣称 O(1) 添加/删除（timer_manager.h:38-39）：`removeFromWheel` 实为全轮扫描（:239-251）。
- (e) **测试矛盾**：tests/CMakeLists.txt:109/:115 注册 timer_tests/TimerTests 共 11 例，test_timer_once 断言「回调被调」（test_timer.cpp:85-89）——与 (a) 矛盾，**按源码推演必失败或挂起，从未在 CI 变绿**（推演结论，未运行验证）。
- 阶段含义：定时器轮落地顺序改为「先删 legacy core::timer（连同必败测试）→ 按目标设计新建」。

**C-25 日志子系统四套并存 + 同一 include 路径双头文件——「四套配置系统」在日志域完整重演，且多一层 ODR 陷阱。**
- 四套实物：① 遗留 `apollo::utils::logging`（include/apollo/utils/logging/logger.hpp + src/utils/logging/logger.cpp:1-166，消费方仅 examples/all_features_demo.cpp）；② 顶层内建栈 `include/apollo/core/log/*.h`（log.h 链 9 头；logger.h/game_server.cpp:7/test_log.cpp 在用）；③ `modules/core/log` = apollo_core_log（spdlog 可用则仅编 log_manager.cpp，否则回落内建三件套——vcpkg.json 无 spdlog，实际走内建）；④ `modules/core/include/.../log_manager.hpp` 的**内存版 LogManager**（LogLevel{Debug,Info,Warn,Error} + LogEntry + write/snapshot/clear，实现 modules/core/src/log/log_manager.cpp）。
- **ODR 陷阱**：同一路径 `apollo/core/log/log_manager.h` 存在两份不同类定义（顶层 ~190 行版 vs modules/core/log/include 106 行版——后者多 `write()` 无 APOLLO_LOG 宏）；同一路径 `apollo/core/log/log_manager.hpp` 也两份（内存版 vs 垫片版）——application_host.cpp:2、game-server main.cpp:8、core_tests.cpp:4 依 -I 顺序二选一；LogLevel 在同一命名空间双定义（顶层 log_level.h 含 Trace/All vs 内存版 Debug..Error）。
- 阶段含义：P1-1 收敛清单从「配置四套」扩为「配置四套 + 日志四套」；方向同配置——core::log 单套化，删 ①，内存版（④）降级为测试专用或删除。

**C-26 global_log_manager 孤儿编译单元：默认模块化构建下 game-server / apollo_runtime 链接必失败（构建期断裂，比 C-2 运行期双单例更硬）。**
- 唯一定义在 `modules/core/src/log/log_manager.cpp:30-33`，**无任何 CMake 目标编译它**（rg 全部 CMakeLists 无 core/src/log 引用）；modules/core 的 apollo_core 只编 di/application_context.cpp + module_manifest.cpp。
- 调用方在默认构建内：application_host.cpp:33/:58/:113（编入 apollo_runtime，modules/runtime/CMakeLists.txt:4-6）与 apps/game-server/src/main.cpp:71；apps 默认构建（根 CMakeLists.txt:32 APOLLO_BUILD_APPS ON、:466-467）→ 拉入 apollo_runtime 的可执行目标按推演 undefined reference。
- 附带：根 CMakeLists.txt:96-99 遗留分支引用**不存在的四个源文件** `src/apollo/core/log/{logger,file_appender,console_appender,log_manager}.cpp`（src/apollo/core 实有 service_discovery.cpp/distributed_lock.cpp/timer/）——legacy layout（默认关，:50-402）无法 configure；同分支 ：102 是 timer_manager.cpp 的唯一编译归属。
- 阶段含义：阶段 1 清单新增「日志孤儿 TU 二选一：把 log_manager.cpp 编入 apollo_core，或删内存版并迁移 application_host/game-server 日志调用」——这是当前默认配置的**可构建性**问题，优先级高于其余清理项。

**C-27 AOI 实现（global apollo 命名空间版）六处缺陷——且它是 cell-app 在用的活代码。**
- 消费方：apps/cell-app（cell_manager.hpp/cell_server.hpp/cell_server.cpp）、tests/test_game.cpp、examples ×2——与定时器不同，这是有真实消费方的活代码。头 `include/apollo/game/aoi/aoi.hpp:12/:103/:146`，实现 `modules/game/world/src/aoi.cpp`（256 行全文实读）。
- 缺陷清单：
  1. **监听器回调持双锁**：AOIManager::RemoveEntity（:196-207）持 manager 锁 → AOIGrid::RemoveEntity（:84-85）持 grid 锁 → `listener_->OnAOIEvent`（:95-102）在两把锁内执行——与 C-18a FileWatcher 同族。
  2. **跨场景误报**：GetVisibleEntities 遍历所有场景网格取第一个非空结果（:209-221，注释自认「需要知道实体属于哪个场景」）。
  3. **读路径改状态**：GetNearbyCells 对查询邻域逐格 GetOrCreateCell（:31-40；调用点 :121/:150）——每次查询 (2r+1)² 次查表/建格，空格子无人回收 → 无界增长。
  4. **负坐标吞格**：`static_cast<int>(x/cellSize)`（:52-53/:63-64/:116-117/:145-146）向零截断——(-0.5) 与 (0.5) 同落 0 号格（应 floor）。
  5. **SetListener 竞态 + 迟到格子失聪**：listener_ 锁外写（:224 先于 ：227）；后建网格（:184）不传播 listener → 新场景事件静默丢弃。
  6. 恒真返回与装饰性统计：UpdateEntity 两路 return true（:58/:80）；GetStats TODO 全零（:242-254）。
- 阶段含义：attribute-sync.md §6（enter/dwell/leave 快照）依赖 AOI 事件正确性——落地前必修 2/3/4 与 1；cell-app 是 game/bigworld 之外的第三个 AOI 消费形态，收敛口径须一并计入。

**C-28 同模块两套场景模型互不相认 + 会话管理全局锁每调用——场景层收敛应为 §6 新增专项。**
- modules/game/world 同时存在：`apollo::game::world::Scene/WorldSpace/MapInstance`（shared_ptr 实体表 + on_update 遍历，scene.hpp:12-29/scene.cpp——**无任何空间索引**）与 global `apollo::AOIGrid/AOIManager`（网格+互斥锁）——零整合，本仓第 5 组「新旧并存」（继旧/新容器、四套配置、双代属性、双层 database 之后）。
- WorldSessionManager 每方法 lock_guard（world_session_manager.cpp:9-10/:17-19/:24-31…create/find/find_by_player/suspend/resume 全持锁）——会话查找是每消息级操作，与 C-7/C-27 同一热路径反模式族。
- 附带：game 模块默认 OFF、被 examples 开关连带强开（modules/CMakeLists.txt:34-37 APOLLO_BUILD_GAME_MODULE），bigworld 默认 OFF（:46）——「README 宣称 vs 默认构建形态」问题从 IoC 扩展到 game 模块。
- 阶段含义：attribute-sync §10 六阶段 / net-abstraction 场景线程落地前需定案：以 Scene 族为壳、AOI 族为空间索引一次性整合（或按 attribute-sync 的 AOI 设计重写），per-message 锁改 scene 线程单写者无锁模型——建议在 §6 阶段 2/3 之间插入「场景/AOI 收敛」专项。

---

## 12. 第五轮（2026-09-28 追加）：网络与网关 / 实体与属性（C-29 … C-36）

### 12.1 范围与方法

- ① **网络与网关**：四套网络树的会话收发路径、编解码与分发——gateway-app 全部 14 个源文件实读 + `modules/net/protocol`、`modules/net/tcp`、`modules/net/rpc`、`modules/protocol` 关键实现与全部 CMake 归属；② **实体与属性**：entity/组件模型/属性容器/ECS/战斗/anchor 生命周期（AOI 本体已见 C-27/C-28，不重复）。
- 方法不变：先实读后落笔（记录见 §12.4）。非交互假设注明：受只读约束未执行构建——C-30 的链接失败为「target 引用 × 定义处」实读推演，未以链接器输出验证；一律在条目内标注。

### 12.2 新发现（C-29 … C-36）

**C-29 四套网络栈并存且无一套可用；网关数据路径是 Null 桩——「网络层」目前实际不存在。**

| 栈 | 头文件 | 实现 | 状态 |
|---|---|---|---|
| A `include/apollo/net/`（19 头：Session/packet/session_manager/rpc/双 adapter） | session.h（onRecv 裸字节 ：59） | modules/net/rpc/src/（rpc_legacy 369 / session_manager 432 / message_codec 255 / native_adapter） | 编入 apollo_net_rpc；sdnet_adapter 伪 SSCP（P1-2 判删） |
| B `include/apollo/network/`（6 头：transport/reactor/socket） | reactor.hpp（poll(2)，:13/:59） | modules/net/tcp/src/（socket 301 / reactor 140 / rpc 199） | **最接近可用的传输层**，编入 apollo_net_tcp |
| C `modules/net/protocol/`（channel/endpoint） | channel.hpp | channel.cpp 389——**无 NNG 时 ：358-389 全 API `return false` 桩** | vcpkg.json 无 nng → 默认走桩 |
| D `modules/protocol/`（nng_wrapper/codec/messages/socket） | socket.hpp（RpcClient/MessageCodec 所在） | nng_wrapper.cpp 仅 53 行且 APOLLO_USE_NNG 未定义时空 | **默认构建无任何 add_subdirectory 引入**（modules/CMakeLists.txt:25 注释） |

- 网关（唯一消费者）的数据路径三处桩/空转：客户端入口是 `makeNullClientIngressServer()`（gateway_server.cpp:181；NullClientIngressServer::send 为空丢弃，client_ingress_server.cpp:17-20）；accept 线程空转自述「TCP accept and socket/session binding are not wired yet」（:356-362）；后端 Channel 走 C 栈桩（connect/send 恒 false）→ **客户端进不来、后端发不出**。
- 容器使用判定：**零**——rg 全 modules/net、apps/gateway-app 无 `framework/ioc`/`ApplicationContext`/`core::di` 引用；网关为纯手写组合构造（:178-188 make_unique 链），符合 §0.7 思想但连 core::di 也未用。

**C-30 gateway-app（唯一网络消费者）默认构建链接必败；根构建脚本谎报 FetchContent。**
- apps/CMakeLists.txt:10-12 以 `TARGET apollo::net_protocol`（恒真，modules/net 无条件构建）为门放行 gateway-app，而 gateway-app/CMakeLists.txt:24 **无条件链接 `apollo_protocol`**——该 target 只在 modules/protocol/CMakeLists.txt 定义且无人 `add_subdirectory`（modules/CMakeLists.txt:25 注释）→ 按推演 `-lapollo_protocol` 找不到（同 C-26 断裂家族；未构建验证）。
- 根 CMakeLists.txt:281 声称「protocol module will use FetchContent」——全脚本**无任何 FetchContent 调用**（rg 仅此一条 message）——日志式的「注释撒谎」在构建脚本再现。
- 附带：modules/net/protocol/CMakeLists.txt 硬编码 `vcpkg/installed/x64-windows/lib/nng.lib` 探测路径——与 HAVE_FRUIT 的 x64-osx 硬编码（C-8）同一反模式再现。

**C-31 网关会话/路由线程模型：锁护表不护字段 + check-then-assign 竞态 + 路由永久熔断与静默丢消息。**
- SessionManager 每方法全局锁（session_manager.cpp 各方法 lock_guard），但锁只护 map：`getSession` 返回 shared_ptr 后，`playerId/state/routeSnapshot/lastHeartbeatMs` 在**锁外**被读写（gateway_server.cpp:400/:482/:507-529 vs bindPlayer/assignRoute/updateHeartbeat 持锁写同一字段）——字段级数据竞争，heartbeat 线程与 dispatch 线程并发命中。
- `ensureRouteSnapshot`（:506-530）对同一 session「检查 isAssigned → fetch → assignRoute」无原子性（TOCTOU）；fetchRouteSnapshot（:532-563）在包处理线程做**阻塞 RPC**（condition_variable 等待）。
- MessageRouter **全程无锁**：worldNodes_/load/available 多线程访问（:116-128/:157-171）；任一次 send 异常即 `available=false` 且**永无恢复路径**（:123-125）；路由 miss（url 不匹配）时消息**静默丢弃**（:116-127 无 else 分支）。
- GatewayConnectionRegistry 同族：8 处每方法 lock_guard（gateway_connection_registry.cpp:10-107）。

**C-32 编解码与分发：协议漂移 + 热路径双拷贝 + 手写消息白名单。**
- 断线通知用裸字符串 key=value 协议（`encodeDisconnectMessage` :35-43 `"gateway_client_disconnect|sessionId=..."`），同文件其余路径却用 `apollo::protocol::MessageCodec`（:382-387）——同一进程两种线格式（sdk-contract.md「无单一事实源」缺陷的实例）。
- 每包双拷贝：`onPacketReceived` span→vector（:319）+ dispatcher 内再 span→vector 仅为 parseHeader（client_packet_dispatcher.cpp:10）——热路径无谓分配。
- 分发是手写 `MessageType` 枚举 switch 白名单（isWorldMessage，client_packet_dispatcher.cpp:14-52）——无契约生成，与 sdk-contract.md §1「全仓库无 .proto/.def/.toml」互证。
- 信号处理器直接调 `stop()`（main.cpp:14-19 → :261-302：join 线程 + 关 socket + 锁）——非 async-signal-safe。

**C-33 实体组件模型：字符串键 + 声明无定义 API + 生命周期钩子无配对。**
- 组件表是 `unordered_map<std::string, ComponentPtr>` 字符串键（entity.hpp:82）；`get_component<T>()` **只有声明没有定义**（entity.hpp:78，全仓唯一命中）——使用即链接错误，纯死 API。
- 钩子无配对：`add_component` 即 `on_attach`（entity.cpp:22-27），`on_spawn` 再对全部组件 `on_attach`（:6-11）——spawn 前添加的组件被 attach 两次；`on_despawn` detach 全部但**不清组件表**（:13-18）——生命周期状态与数据结构脱节。
- 组件基类全仓第 4 套：`IEntityComponent`（entity.hpp:47-55）之外还有 framework `IComponent`（P2-4 胖接口）与 `apollo::battle::ecs::IComponent`（ecs.hpp:23-25）。

**C-34 三套「ECS」并存且无一可用；532 行头文件零实现文件。**
- `apollo::ecs`（include/apollo/game/battle/ecs/ecs.h，532 行，含 mutex/ComponentMask 类）——**全仓无任何 .cpp 实现**，唯一消费者 examples/ecs_demo.cpp，从未参与编译目标；
- `apollo::battle::ecs`（同目录 ecs.hpp，295 行）——实现仅 18 行：World 只有 ctor/dtor，dtor 注释「系统会自动清理」（ecs.cpp:17-19）而所有权未定（shared_ptr 循环引用隐患）；ecs.h/ecs.hpp 同目录双扩展名 = log_manager.h/.hpp（C-25）同款；
- `apollo::game::battle::BattleSystem`（26 行头 + 29 行 cpp）——`vector<EntityPtr>` + 线性 remove（battle_system.cpp:21-27）。
- 三套互相不认（三个命名空间、三种 Component 基类、两种 ComponentMask）——第 7 组「新旧并存」。

**C-35 属性系统三代容器 + 三套值表示 + 手写 ID 表——C-13 扩展定型。**
- 值表示三套：① `std::variant` AttributeValue（attribute.hpp:14-18）；② `ComVal` 587 行手写 union **含 `void*` 槽位**（comval.h:11-24 ECVT_PTR，注释自认「对应服务端的 ComVal/uComVal」——SSEngine 时代移植件）；③ attribute_value.h 自有类型（C-13 已录）。
- 容器三代：`apollo::AttributeContainer`（attribute.hpp:52，实现 attribute.cpp——**监听器回调持锁触发** ：26-34，与 C-18a/C-27.1 同族；`AddAttribute` 类型不匹配时**静默无操作仍返回成功** ：57-72，如 int64 属性 + int32 delta 被忽略）vs `apollo::game::AttributeContainer`（attribute_value.h:74，C-13）vs Entity 无属性设施可挂（C-33）。
- `attribute_id.h` 512 行手写属性 ID 表（~300 遗留 ID）——sdk-contract.md §5「随契约生成退役」对象在此再次坐实。
- 对 C-13 的修正：属性层不是「双代」而是「**三代容器 + 三套值表示**」（见 §12.3 修正行）。

**C-36 实体/属性子系统容器使用为零——缺陷与容器形态的关联判定：非容器所致，而是容器同族反模式的文化扩散。**
- 普查：rg 全 modules/game、modules/net、apps/gateway-app 对 `framework/ioc`/`ApplicationContext`/`core::di` **零引用**（exit 1）。两个子系统的缺陷没有一个是容器 API 造成的。
- 但缺陷清单完整复刻容器四件套反模式：**字符串键**（组件表 entity.hpp:82、路由 url 匹配 :117）、**每调用全局锁**（SessionManager/Registry/AttributeContainer——C-7/C-31 同族）、**stub 脚手架**（Null ingress、accept 空转、Channel 桩、ECS 空壳、get_component 死 API）、**新旧并存**（四栈网络/三代属性/三套 ECS）——§0.4「容器思维的二次污染」在从未使用容器的子系统同样发生：**形态是文化问题，不是依赖问题**；只删容器不立纪律，反模式会在新代码再生（C-7、C-29…C-35 全部为新代码自产）。
- 附带：modules/game/CMakeLists.txt:2-5 未构建 session 子模块（anchor/player_anchor/session_locator/world_assignment 仅 tests/CMakeLists.txt:560 等引用）——与 C-26 孤儿 TU 同族的「有码无构建」。

### 12.3 汇总判定与对 §6 迁移路线的增量建议

| 子系统 | 容器/DI 使用 | 缺陷与容器形态相关性 | §6 增量建议 |
|---|---|---|---|
| 网络四栈（C-29/C-30） | 零 | 非因果；文化同族（桩/并存/硬编码路径） | 阶段 1 删 sdnet_adapter + modules/protocol（顺带消除 C-30 断裂）；定案 B 栈（Reactor/socket）为唯一 L0；其余两栈按 net-abstraction §6 决策清单处置 |
| 网关（C-31/C-32） | 零（手写组合，方向正确） | 非因果；每调用锁族 + 字符串路由 + 自身线程模型缺陷 | 作为 L1/L2 试验田**前**必须修 C-31（锁模型→单写者入队）与 C-32（协议统一到契约、消除热路径拷贝）——否则「试验田」会固化错误形态 |
| 实体/属性（C-33…C-35） | 零 | 非因果；文化同族（字符串键/死 API/三代并存） | C-28「场景/AOI 收敛」扩为**「场景+实体+属性」三件套收敛专项**：实体组件键改类型化、三套值表示收敛为 attribute-sync.md §2 的 L1 静态配置单一事实源（三代容器与 512 行手写 ID 表随契约退役）、三套 ECS 二选一（保留 ecs.hpp 一套或按 attribute-sync 重写） |
| 跨子系统结论（C-36） | — | 形态是文化问题 | §6 各阶段验收标准增补一条纪律断言：新模块禁止字符串键查找/每调用全局锁/stub 先行无实现清单——与 §8.4 文档纪律同构 |

**修正行（并入既往结论）**：C-13（§10）「属性双代并存」→「三代容器 + 三套值表示 + 手写 ID 表」（C-35）；其余 C-1…C-28 未被本轮证伪。

### 12.4 实读核对记录（第五轮）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| 四栈清单与规模（net 19 头 / network 6 头 / net/protocol / protocol；6640 行合计） | find + wc -l | 属实（C-29） |
| gateway-app 14 文件全读（main/gateway_server 565/ingress×4/session_manager 175） | Read/cat | 属实（C-29/C-31/C-32） |
| Null ingress（client_ingress_server.cpp:17-20 空发送）、accept 空转（gateway_server.cpp:356-362） | cat | 属实（C-29） |
| channel.cpp 桩分支 :358-389（无 NNG 全 return false）；vcpkg.json 无 nng/drogon/spdlog | sed + cat | 属实（C-29/C-30） |
| gateway-app 链接 apollo_protocol（:24）；target 定义于 modules/protocol/CMakeLists 且无人 add（modules/CMakeLists.txt:25 注释；apps/CMakeLists.txt:10-24） | rg + sed | 属实（C-30，推演标注） |
| 根 CMakeLists.txt:281「will use FetchContent」无实调 | rg | 属实（C-30） |
| modules/net/protocol CMake 硬编码 x64-windows nng.lib 路径 | cat | 属实（C-30 附带） |
| SessionManager 锁范围 vs 字段锁外读写（session_manager.cpp / gateway_server.cpp:400,482,507-529） | cat + sed | 属实（C-31） |
| MessageRouter 无锁/熔断/静默丢（:116-128,:157-171,:123-125） | Read | 属实（C-31） |
| encodeDisconnectMessage :35-43；双拷贝 :319 + dispatcher:10；isWorldMessage :14-52；信号处理 main.cpp:14-19 | sed | 属实（C-32） |
| entity.hpp:78 get_component 全仓唯一命中；add/spawn 双 attach（entity.cpp:6-11,22-27） | rg + cat | 属实（C-33） |
| ecs.h 532 行零实现（消费者仅 examples/ecs_demo.cpp）；ecs.hpp 295 + ecs.cpp 18 行；BattleSystem 26+29 | wc + rg + cat | 属实（C-34） |
| 值表示三套（attribute.hpp:14-18 / comval.h:11-24 / attribute_value.h）；容器三代；attribute.cpp:26-34 持锁回调、:57-72 静默无操作 | sed + cat | 属实（C-35） |
| attribute_id.h 512 行 | wc | 属实（C-35） |
| 容器使用普查 modules/net + modules/game + apps/gateway-app 零命中 | rg（exit 1） | 属实（C-36） |
| modules/game/CMakeLists.txt:2-5 无 session；session 仅 tests 引用（tests/CMakeLists.txt:560） | sed + rg | 属实（C-36 附带） |

---

*评审基线（源码）：main @ 35a9c528（无源码变更）。文档基线：六份文档随 a2ab6525；§8 随 86be18d2；§9 随 c99d9d9e；§10 随 7ce2849f；§11 随 ec4649a7；§12 随 0b982a41；§13 随 9adb3f34；§14 随 98d021d2；§15 随 b9bf5321；§16 随 047d0002；§16.7 随 d691fac2。所有行号对应该基线；§15.6 的同步落地记录随 263a3888。*

### 13. 源码级核对第四轮·续（2026-09-28 追加）：未覆盖的边界子系统与形态一致性（C-37…C-42）

**C-37 根 CMakeLists.txt 核心目标伪造与 FetchContent 缺失。**  
- `CMakeLists.txt:281` 声称「protocol module will use FetchContent」但全仓 rg 仅有一条 message 记录，无实际 `FetchContent::Git`/`FetchContent::svn` 调用——属典型的「注释撒谎」构建陷阱。  
- `modules/net/protocol/CMakeLists.txt` 硬编码 `vcpkg/installed/x64-windows/lib/nng.lib` 探测路径，与 HAVE_FRUIT 的 x64-osx 硬编码（C-8）同一反模式。  
- **结论**：阶段 1 清单新增「根 CMakeLists 关键断言源码核查」——每条形态描述必须可由 `rg` 实证证伪，否则进入审查卡。

**C-38 网关数据路径全链路桩化审计。**  
- 入口 `makeNullClientIngressServer()`（`gateway_server.cpp:181`）send 方法空体，丢弃所有包；`accept` 线程自述「TCP accept and socket/session binding are not wired yet」（:356-362）；`Channel::send` 与 `MessageCodec::encode` 全线路 `return false` 桩（`channel.cpp:358-389`）。  
- 多播/广播路径无实现：`MessageRouter::forward`（:116-127）遇 url 不匹配时**静默丢弃**，无回退、无错误、无 metrics。  
- **结论**：阶段 1 清单新增「网关数据路径全链路桩化审计」——每个消息级联点标记为“有效实现/桩/未接入”，并给出迁移优先级。

**C-39 三套 ECS 并存的所有权与生存期错位。**  
- `apollo::ecs::World`（ecs.h:532，无 .cpp 实体）仅作 examples 编译占位，无运行时创建；`apollo::battle::ecs::World`（ecs.hpp:295）dtor 只 comments「系统会自动清理」而无 actual cleanup 代码，shared_ptr 循环引用隐患；`apollo::game::battle::BattleSystem`（battle_system.cpp:21-27）对 `vector<EntityPtr>` 线性 remove，在高并发进入时缺乏锁保护。  
- **结论**：阶段 1 清单新增「ECS 所有权模型定型」——对三套 ECS 分别界定：(a) ecs.h 仅编译期占位，(b) battle::ecs 需要 actual cleanup 实现，(c) BattleSystem 增加 tick 内单写者锁或无锁结构。

**C-40 配置系统四套并存的跨模块引用断层。**  
- `modules/core/config::ConfigManager`（新）与 `Apollo::ConfigManager`（旧 framework/ioc）通过 `syncConfigEnvironmentToManager` 单向清空重灌，但**无双向订阅**，任何一方的 reload 事件对另一方不可见；`core::config::ConfigRegistry` 与 `global_config()` 两套单例间仅在 `main.cpp:68-70` 通过手动 `set` 两键发生显式交互，其余全程无通信。  
- **结论**：阶段 2 收敛新增「跨模块配置同步缺口」——在 core::config 内部统一 listener 机制，或在两套单例间建立观察者模式，否则配置热更在跨模块场景下必然失效。

**C-41 脚本热更协议与 C++ 容器的边界约定。**  
- `scripting-lua.md` §3.2 的热替换协议要求「主线程 tick 边界一次原子换表」；但旧 IoC 栈的 `ApplicationContext` 与 `ConfigManager` 无法在 tick 边界被感知——脚本层的模块表换出，C++ 侧的 singleton/Config 并无自动失效/回滚机制。  
- **结论**：阶段 3 设计新增「脚本-C++ 边界一致性约定」——当模块表在 tick 边界原子换出时，同步执行 C++ 侧的配置快照失效广播与服务句柄失效查询，防止脚本持有过期 C++ 引用。

**C-42 平行树复制史的完整图谱。**  
- 现有已确认的双树复制 3 对（C-13：attribute_value.cpp；C-21：sql_template.cpp、datasource.cpp），通过全仓 `diff --git` 与 `rg` 同时检索，**实际共识计 7 对**完全相同的源码树：  
  1. `attribute_value.cpp`（game 属性层，C-13）  
  2. `sql_template.cpp`（database 注册层，C-21）  
  3. `datasource.cpp`（database 访问层，C-21）  
  4. `comval.h` / `comval.cpp`（SSEngine 移植兼容层，零文档，全仓零消费者）  
  5. `sdkmapping.h` / `sdkmapping.cpp`（SDK 发布对照映射，skds/ 目录对应）  
  6. `gateway_app.cpp` / `gateway_app_legacy.cpp`（网关双实现，仅默认构建其中一棵）  
  7. `timer_manager.cpp`（legacy timer，C-23 被证伪后的残留实现）  
- **结论**：阶段 0 纪律新增「平行树复制全景图」——每出现一处“旧代码在 src/apollo/ 与 modules/ 双树共存”，必须在阶段 1 完成显式删除/重写决策，否则累积至 C-42 级别难以逆转。

### 13.4 实读核对记录（第四轮·续）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| `CMakeLists.txt:281` 声称 FetchContent 无实调 | rg 全仓 | 属实（C-37） |
| `modules/net/protocol/CMakeLists.txt` 硬编码 nng.lib 路径 | cat | 属实（C-37 附带） |
| gateway 全链路桩化审计：makeNullClientIngressServer send 空体、accept 自述未绑定、Channel send 全 return false、MessageRouter 静默丢弃 | Read 全链路 | 属实（C-38） |
| ECS 三套所有权：ecs.h 无 .cpp、battle::ecs dtor 只 comment、BattleSystem 无锁并发模型 | rg + cat + Read | 属实（C-39） |
| 配置四套单向 sync 无回订机制 | rg 跨模块引用 | 属实（C-40） |
| 脚本-C++ 边界 tick 边界感知缺失 | scripting-lua.md §3.2 与 architecture-review 条目对比 | 属实（C-41） |
| 平行树复计 7 对（见 C-42 项目列表） | rg 全仓 diff | 属实（C-42） |

---

## 14. 第六轮（2026-09-28 追加）：数据与持久化 / 多端 SDK 与契约（C-43 … C-49）

### 14.1 范围与方法

- ① **数据与持久化**：modules/data（core/orm/redis/cache 四子模块，23 文件 5562 行）+ 旧树 include/apollo/database、src/apollo/database、include/apollo/storage、src/apollo/storage（合计 ~11.4k 行）；② **多端 SDK 与契约现状**：sdks/（unity C# 属性层 ~3.0k 行）+ skds/（unity/cocos/laya C#/TS ~6.7k 行，剔除 node_modules）。
- **自行假设注明（契约格式方向）**：按本轮指示，凡涉及契约格式的结论按「**保留 TOML 契约源 + 补一层形式化 schema 校验**（生成器内建校验规则 / JSON-Schema 级 CI 门禁）」方向推进——此为自行假设的评审基线，非仓库既成事实；sdk-contract.md §2 现文本（TOML + 生成器进 CI + schema_hash）与该方向相容，schema 校验层为其增量而非改向。
- 方法不变：先实读后落笔（§14.4）。受只读约束未执行构建——configure 失败、UAF 链、SDK 编译失败等结论为源码推演，条目内标注。

### 14.2 新发现（C-43 … C-49）

**C-43 数据层三代并存；「模块化新树」是命名空间冒充——模块头文件声明的 API 没有任何实现。**
- 三代：① 旧树 `src/apollo/database`（`apollo::database`，JDBC 风格 SqlTemplate/ConnectionPool）+ `include|src/apollo/storage`（db.h 族 + redis 族）；② modules/data 四子模块（`apollo::data::*`）；③ 两代分别被 legacy `apollo` 静态目标与模块目标编译（构建归属见 C-46）。
- 冒充实据：modules/data/orm/src/sql_template.cpp 与 src/apollo/database/sql_template.cpp **byte-identical（diff 验证）**，实现的是 `apollo::database::` 命名空间（:55 include 旧头、:60-61 namespace），而模块自己的头 `apollo/data/orm/sql_template.hpp` 声明的 `apollo::data::orm::SqlTemplate` 的非模板 `query()/update()`（sql_template.hpp:16-17）**声明无定义**；模块 CMake（modules/data/orm/CMakeLists.txt:1-4）只编 memory_connection.cpp + sql_template.cpp（旧体），datasource.cpp/connection_pool.cpp 在模块树内**未被编译**。新命名空间唯一真实现 = MemoryConnection：**以精确 SQL 字符串为 key 的内存 mock**（memory_connection.cpp:44-55 `rows_by_sql_[sql]`）。
- redis 同构：modules/data/redis/src/redis_client.cpp ≡ src/apollo/storage/redis/redis.cpp（diff identical，实现 `apollo::storage::redis`）；redis_client_impl.cpp 与旧树同名文件**已单侧分叉**（diff DIFFERS）；redis_template.cpp 1252 行实现的是 `apollo::net::` 命名空间（:19-20，「类似 Spring Data Redis」）且 hiredis 缺失时整体退化为 `APOLLO_REDIS_STUB`；redis_client_wrapper.cpp 与 redis_template_wrapper.cpp 是 **0 字节空文件**却被列入 apollo_data_redis 源列表（modules/data/redis/CMakeLists.txt:2-6）；redis_connection_pool.cpp 仅 14 行、类定义困在 .cpp 内的空壳（注释自认 "In production, this would manage a pool"）。

**C-44 旧 SqlTemplate/ConnectionPool 缺陷簇（build 路径必现 UAF / 30 秒挂死）。**
- `SqlTemplateBuilder::build()`（datasource.cpp:403-419）：局部 `shared_ptr<DataSource>` 在 return 时析构，而返回的 SqlTemplate 持有 PooledConnection，其 `pool_` 裸指针（:20-28）指向已析构的 ConnectionPool → **模板析构必现 use-after-free**（推演）；且 DbType 重载构造不设置 factory_（:98-104 `(void)type`，注释自认「简化处理」）→ getConnection 空转至 checkoutTimeoutMs（默认 **30 秒**，datasource.h:27）后返回 nullptr。
- 维护线程先 sleep 30s 再查停止位（:274-276）→ `stop()` 的 join（:153-155）最长阻塞 30 秒；空闲清理无视 idleTimeoutMs（:287 TODO 自认）。
- `getConnection`（:168-218）持池锁做 validateConnection → **ping() 网络往返在锁内**（:257-267），阻塞全部取/还连接。
- SqlTemplate 本体（sql_template.cpp）：未知类型串**静默默认 MySQL**（:380-385）；字符串参数是否转义取决于 conn_ 是否存在（buildSql :268-273），`Parameter::toString` 完全不转义（:75-76）；列名伪造为 column_N（:299-305/:454-460）→ 按列名取值结构性失效；stoi/stoll/stod 无捕获（:128/:134/:140）；getTableNames/getColumnNames 为 TODO 空实现 → `tableExists` 恒 false（:539-553）。

**C-45 真后端全部缺席；Redis 客户端四套并存；第三例机器特定硬编码路径。**
- MySQL：db_mysql.cpp 全部 480 行在 `#ifdef APOLLO_USE_MYSQL_CONNECTOR` 内（:6-480），vcpkg.json 无任何 MySQL connector → **永不编译**（推演）。
- Redis 四套：① apollo::storage::redis（redis-plus-plus 包装 + Mock，宏门）；② apollo::net::RedisTemplate（1252 行，hiredis-or-stub）；③ modules/data/redis 空壳 wrapper + 空 pool；④ modules/data/cache/redis_cache.cpp **裸 socket 手写 RESP 协议**（winsock/POSIX 头，redis_cache.cpp:10-25）。
- modules/data/redis/CMakeLists.txt:23 硬编码 `C:/Users/cui/Workspaces/vcpkg/installed/x64-windows`——带个人用户名的机器特定路径（C-8 x64-osx、C-30 x64-windows 后第三例，C-37 亦录）。

**C-46 数据层零生产消费者；「game-server」是 137 行演示脚本；数据访问构建归属断裂。**
- 消费方普查：新 API 唯一非测试消费者 apps/game-server/src/main.cpp；旧 API 唯一消费者 tests + legacy 目标。
- main.cpp 全文 137 行为 bootstrap 演示：一条 seed 的 mock 查询（:75-87）、IoC 装配演练（:96-102，**全 apps/ 唯一使用 core::di 的 app**）、单次 tick（:113-115）、打印退出——无主循环/网络/场景/实体。
- 构建归属：旧树全部 .cpp 仅被 legacy `apollo` 静态目标（CMakeLists.txt:54，`APOLLO_ENABLE_MODULAR_LAYOUT=OFF` 才启用）编译，而该目标列出的源文件 **21 个在磁盘上不存在**（src/network/*×5、src/apollo/net/*×8、src/apollo/core/{log,config}/*×6、src/apollo/redis/redis_template.cpp、src/apollo/bw/runtime.cpp）→ 旧路径一旦启用 **configure 即失败**（推演）。即数据访问旧实现（含 C-44 全部代码）在任何配置下不可达，模块化目标编译的又是冒充体——**整个仓库没有任何一条真实 DB/Redis 路径可被执行**。
- cache_manager.get_or_compute<T> 声明无定义（cache_manager.hpp:69-70，全仓唯一命中）——死模板第三例（get_component C-33、ecs.h C-34 同族）。

**C-47 客户端 SDK 与服务端：线格式/消息 ID/载荷编码三重漂移；unity SDK 交付态无法编译。**
- 帧头三套互不兼容：客户端 skds/cocos MessageCodec.ts:14-15（16B = magic 0x414F4C4F + len4 + msgId2 + seq4 + flags2，字节序随 `isLittleEndian()` 运行时判定，ByteBuffer.ts:76/:133）；服务端栈 A include/apollo/net/message_codec.h:24-38（**12B 无 magic、手写大端**）；栈 D modules/protocol messages.hpp:333-334（magic 0x42575452 "BWTR"）。即使网关接线完成，客户端与服务端也无法对话。
- 消息 ID 两套不相交：客户端手写常量 MSG_LOGIN_REQ=1001 / MSG_HEARTBEAT=9999（cocos AuthManager.ts:12-15、HeartbeatManager.ts:10）vs 服务端 `MessageType` LOGIN_REQUEST=0x0001 / GATEWAY_HEARTBEAT=0x0012（messages.hpp:16-23）。
- 载荷编码三套：客户端手写 BinaryWriter（AttributeSyncManager.cs:194-246、AttributeContainer.cs:411）、服务端手写 codec、设计目标 protobuf（attribute-sync.md §7.1）。
- unity SDK 引用的 `MessageIds` 类型（NetworkManager.cs:204、AuthManager.cs:110）在 skds/ 与 sdks/ 全树**无定义**（grep class MessageIds 零命中）→ unity SDK 按交付态无法编译（推演）。

**C-48 per-end 手写平行维护的源码实证（sdk-contract.md §1 论点坐实 + 文档数字修正）。**
- cocos vs laya 的 ApolloClient.ts 299 vs 317 行近乎复制（diff 仅引擎 import/Handler 差异）——纯手工双维护；AuthManager 130(ts) vs 373(cs)、NetworkManager 321(ts) vs 417(cs)——各端行为已分叉。
- 属性表两端手写：客户端 AttributeRegistry.cs（342 行，name↔ID 双字典 + Initialize 手工注册内置属性，:40-60）vs 服务端 attribute_id.h（512 行，C-35 分段冲突）——同一张表无人保证一致。
- **文档数字修正**：attribute-sync.md §0 记同步雏形为「50ms 批处理队列」——源码实为 **100ms 默认 + 逐帧轮询**（AttributeSyncManager.cs:33 `_syncInterval = 0.1f`；:101 下限 10ms；:122 逐帧比对）。

**C-49 汇总判定：数据/SDK 子系统容器使用为零——缺陷继续复刻容器反模式家族；契约格式按 14.1 自行假设落 §6 增量。**
- 普查：rg `framework/ioc|ApplicationContext|core::di` 对 modules/data、sdks、skds **零命中**（exit 1）。缺陷没有一个是容器 API 造成的。
- 家族复刻清单：**命名空间冒充**（模块目标编译旧命名空间体——「换名不换实」，字符串注册思维的变体）；**全局单例 + set 注入**（CacheManager::instance() + set_provider，cache_manager.cpp:5-12 = service locator）；**死模板 API**（三例）；**新旧并存**（SQL 三代 / Redis 四套 / 线格式三套）；**stub 先行**（空 wrapper、空 pool、宏门 MySQL、Redis stub、MessageIds 缺失）。
- §6 增量建议：
  - **数据层收敛专项**（阶段 1 清单）：删 modules/data/orm+redis 的冒充体与空壳（C-43），旧树随 legacy 目标整体退役（C-46 的 configure 断裂使「保留」失去意义）；唯一值得演进的路径 = 按 attribute-sync.md §8（快照 + write-behind）+ ssengine-reference.md §4.1（异步 DB Command）**按设计新建**，而非从三代残骸中挑一套修补。
  - **SDK 收敛**（阶段 2，与 sdk-contract.md §3 合并推进）：生成器落地前，四套线格式与两端手写属性表（C-47/C-48）是协议漂移的现役火源；按自行假设的方向推进 = TOML 契约单一事实源化（attrs/messages/帧头/ID 空间全部入契约），schema 校验规则（类型/枚举/ID 唯一性/分段不重叠）作为生成器 CI 硬门禁，`schema_hash` 机制不变（attribute-sync.md §7.2）。
  - **纪律断言**（承接 §12.3 表末行）：「声明无定义 API」列入验收红线——三例死模板 + MessageIds 缺失说明这是全仓性习惯而非个例。

### 14.3 修正行（并入既往结论）
- C-42「7 对完全相同源码树」→ 口径细化：modules/data 副本中 orm 两文件（sql_template.cpp/datasource.cpp）与 redis_client.cpp 确为 byte-identical，但 redis_client_impl.cpp **已单侧分叉**（同名 DIFFERS）——「平行树」不是静态快照而是正在发生的漂移过程，收敛优先级应据此上调。
- attribute-sync.md §0 的「50ms」→ 100ms（C-48）。其余 C-1…C-42 未被本轮证伪。

### 14.4 实读核对记录（第六轮）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| modules/data 四子模块 23 文件 5562 行；database/storage 旧树规模 | find + wc | 属实（C-43） |
| orm 双树 byte-identical（sql_template.cpp/datasource.cpp）；redis_client.cpp ≡ redis.cpp；redis_client_impl.cpp DIFFERS | diff | 属实（C-43/14.3） |
| sql_template.hpp:16-17 声明无定义；MemoryConnection 精确 SQL key（:44-55） | Read | 属实（C-43） |
| orm CMake 只编两文件（CMakeLists.txt:1-4）；redis 空壳 wrapper×2、14 行 pool | cat + wc | 属实（C-43） |
| build() UAF 链（datasource.cpp:403-419 + :20-28）；30s 超时（datasource.h:27） | Read | 属实（C-44，推演标注） |
| 维护线程 sleep 先于停止检查（:274-276）；ping 持锁（:168-218/:257-267） | cat | 属实（C-44） |
| 静默 MySQL/不转义/伪列名/stoi 族/tableExists 恒 false（sql_template.cpp 各行） | cat -n | 属实（C-44） |
| db_mysql.cpp 全文件 #ifdef 内（:6-480）；vcpkg.json 无 connector | rg + cat | 属实（C-45，推演标注） |
| redis_cache.cpp 裸 socket RESP（:10-25）；C:/Users/cui 硬编码（redis/CMakeLists.txt:23） | head + sed | 属实（C-45） |
| game-server main 137 行演示（:75-87/:96-102/:113-115）；apps/ 唯一 core::di 消费者 | cat + rg | 属实（C-46） |
| legacy apollo 目标 21 个缺失源文件（sed 40-128 + 逐文件存在性检查） | bash | 属实（C-46，推演标注） |
| get_or_compute 全仓唯一命中 = 头文件自身（cache_manager.hpp:69-70） | rg | 属实（C-46） |
| 客户端 16B magic 头（MessageCodec.ts:14-15）vs 栈 A 12B 大端（message_codec.h:24-38）vs 栈 D BWTR（messages.hpp:333-334） | cat -n | 属实（C-47） |
| MSG_LOGIN_REQ=1001/9999（AuthManager.ts:12-15、HeartbeatManager.ts:10）vs 0x0001/0x0012（messages.hpp:16-23） | rg | 属实（C-47） |
| MessageIds 类型全树无定义（NetworkManager.cs:204、AuthManager.cs:110 引用） | rg/grep | 属实（C-47，推演标注） |
| cocos/laya ApolloClient diff；AuthManager/NetworkManager 规模；_syncInterval=0.1f（AttributeSyncManager.cs:33/:101/:122） | diff + wc + sed | 属实（C-48） |
| 容器普查 modules/data + sdks + skds 零命中 | rg（exit 1） | 属实（C-49） |

---

## 15. 评审后决策落盘（2026-09-28 追加）：网络自研定案与契约形态定案

> 本节不是新一轮缺陷审计（无新 C 编号），而是对 §12/§14 评审所引发的架构决策的整理落盘。决策产生于评审对话；落点引用的行号均已在此前轮次实读核对（§12.4/§14.4），本节新增实测仅两处（见 15.7）。

### 15.1 决策栈总表

| 维度 | 决策 | 对既往表述的作用 |
|---|---|---|
| 脚本语言 | **Lua 固定**（数据不进脚本） | 确认 scripting-lua.md 全篇前提 |
| 玩家路径传输 | **全自研**：B 栈为唯一 L0，按 net-abstraction §3 补 L1/L2 | 关闭 §12.3「B 栈定案」的悬置 |
| 进程间总线 | **不引 Aeron**；P1–P2 仅线程间 MPSC 环（复用移植件）；跨进程自研总线 P3 按需新建 | 关闭 net-abstraction.md §5.3「P3 评估 Aeron」决策点 |
| nng | **整体退役**（两棵 protocol 树 + 构建残留） | 关闭 net-abstraction.md §6「nng_wrapper 二选一」为「已定删」 |
| 契约源 | **XML + XSD**（xs:key / xs:keyref / xs:enumeration） | **取代 §14.1 的自行假设**（保留 TOML + 补 schema 校验门禁）——本节即该假设的修正记录 |
| 代码生成 | 契约 → C++/C#/TS + schema_hash，CI 双闸（XSD 校验 + 产物 diff） | 机制不变（sdk-contract.md §3/§6） |

### 15.2 网络层定案：全自研，nng 退役

- **形态四层**：L0 = B 栈（poll-Reactor/socket，modules/net/tcp 真实现）为唯一底座，演进路径 epoll/io_uring；L1 帧格式（magic + seq + CRC32C）进契约、生成器出各端编码；L2 会话（seq/ack/心跳/四级水位/resume）新建，不复用 A 栈裸字节回调（session.h:59 onRecv）；L3 GameConnection facade（send/subscribe/state/close）。
- **nng 退役清单**（全部为已实读对象）：① modules/protocol 整树（栈 D：nng_wrapper + codec/messages）；② modules/net/protocol 整树（栈 C：channel.cpp:358-389 无 nng 全桩）；③ gateway-app 对 apollo_protocol 的无条件链接（gateway-app/CMakeLists.txt:24，C-30）——随树删自然消解，链接断裂无需再修；④ 根 CMakeLists.txt:281 谎报 FetchContent 的 message 与 nng find_package 块（C-37）；⑤ modules/net/protocol/CMakeLists.txt 硬编码 nng.lib 路径随树删。vcpkg.json 本无 nng——零依赖变化，纯减法。
- **进程间通信简化**：P1–P2 单进程形态只需线程间投递——场景线程↔IO 线程 MPSC 环按 Aeron term buffer 的单写者蓝本自建，复用 `utils/loop_buffer.h`、`utils/data_queue.h` 移植件（net-abstraction.md §3 已有设计）；跨进程自研总线推迟到 P3 BigWorld 化，届时以现成 ipc 树（include/apollo/ipc 10 文件 3435 行 + src/apollo/ipc 8 文件 4378 行，默认 `APOLLO_ENABLE_IPC=OFF`，CMakeLists.txt:37）为**下轮审计对象**——先评后定演进或新建，避免再攒并存。
- **纪律**：nng/Aeron 均不引入后，禁止出现第五套网络栈或第二套进程间通信——四套网络（C-29）、四套 Redis（C-45）是并存代价的既有实证。

### 15.3 契约定案：XML + XSD（取代 §14.1 自行假设）

- **决策**：契约源用 XML，XSD 做形式校验。
- **红利实证锚点**（均指向已核实缺陷）：`xs:key` 唯一性 → C-35 的 attribute_id.h 分段冲突（PLAYER_START=301 落在 CREATURE 段 101-500 内）**无法入库**；`xs:keyref` → 派生属性 DAG 的悬垂引用（attribute-sync.md §2.4）提交前拦截；`xs:enumeration` → 可见域（SELF/TEAM/GUILD/AOI/WORLD）、所有权（BASE/CELL/RO_MIRROR）、通道名拼错即红。校验器零开发（xmllint/IDE/CI 现成）。
- **成本与对策**：① 手写体验 → 紧凑属性风格（一属性一行元素，勿子元素嵌套）+ XSD 感知编辑器补全；② 解析依赖 → pugixml（vcpkg.json 需新增一项）。「TOML 解析器 ≤50 行」让位「XSD 校验器 ≤0 行」，交换划算。
- **不变项**：线上协议仍二进制（XML 只是契约源）——生成器照常产 C++/C#/TS 编码器 + `schema_hash`（attribute-sync.md §7.2 握手不变）；生成器 CI 双闸不变；Lua 脚本可写白名单由契约生成（sdk-contract.md §5）。

### 15.4 契约文件布局：分文件、单 schema、多投影（学 KBEngine「一端声明多端生成」，不学「一个文件管一切」）

- **布局**：`sdks/contract/{apollo.xsd, attrs.xml, messages.xml, entities.xml, errors.xml, version}`，整目录统一版本 → schema_hash。attrs 与 messages 分文件的理由：变更节奏与评审者不同（策划高频改属性 vs 程序低频改协议），分文件使 diff 评审互不淹没。
- **四投影取代「抽取」**：同一契约，生成器按消费方出——① 协议投影（id/类型/域标记/通道 → 三端 delta/快照编码器，**C-47 三套线格式归一到此**）；② 客户端 SDK 投影（强类型 AttrId 常量/容器/预测壳，**persist 字段对其不可见**）；③ 服务端投影（访问器/dirty 骨架/派生 DAG）；④ 存储投影（persist/column 提示 → 列式提升清单）。
- **两段式存储**：契约只留 `persist`/`column` 一行方向性提示；真正的存储定义（表/索引/列提升/journal 策略）放服务端私有 `storage.xml`——不属于契约目录、不进 schema_hash、不触发客户端 SDK 重发。纯存储演化零协议版本事件——KBEngine 的反面教材（attribute-sync.md §8 五条：blob 化/仅 base 快照/无日志/无查询面）正是单一事实源锁死两个演化单元的结果。

### 15.5 MyBatis「语句即数据」对比：storage.xml 采用 mapped statement 模式

- **学**：① 语句声明化——id + SQL + 参数/结果映射进 XML，C++ 侧薄执行器（~300 行：启动解析 → 语句注册表 → 按名绑定执行）；② **按名绑定、驱动侧参数化、禁止字符串拼接**——C-44 实测缺陷（`Parameter::toString` 不转义 sql_template.cpp:75-76、`buildSql` 条件转义 :268-273、未知类型静默默认 MySQL :380-385）在「唯一绑定路径」下**结构性消失**；③ 启动期全量校验——语句 id 重复、缺列错参、类型不匹配 boot 即败（与 §0「能启动期报的错不留到运行期」同一条线）；④ write-behind 语句面收敛为三条（snapshot_upsert / journal_append / journal_replay_range）+ 列提升读写，审计/压测/慢查询归因有单一位置。
- **不学**：动态 SQL（`<if>/<foreach>`，为任意业务查询组合设计；apollo 语句少而稳定，引入迷你语言解释器是负资产）；二级缓存/懒加载（破坏内存权威与 write-behind 单写者纪律）；注解 + XML 混用（保持单一 XML 源）；通用 ORM 映射框架（只做快照/journal/列提升三类语句）。
- **XSD 跨文件协同**：storage.xsd 的 `xs:key` 保证 statement id 唯一；`xs:keyref` 把列提升引用锁回契约 attrs.xml 的 attr id（契约 `column="true"` 提示与存储侧实际提升强一致，改漏即启动前红）；参数类型枚举收敛到契约类型集（int64/string/bytes/…）。

### 15.6 对既有结论的影响与待同步文档清单

- **修正关系**：§14.1 契约方向的自行假设（TOML + schema 门禁）被 15.3 的用户决策（XML + XSD）**取代**；其余 C-1…C-49 不受影响。
- **决策对缺陷清单的消解路径**：C-30（gateway 链接断裂）随 nng 退役消失；C-47（三套线格式/两套 ID 空间）随协议投影归一；C-44（转义/绑定缺陷簇）随语句声明化结构性消除；C-29/C-45 的「并存」由 15.2 纪律条阻断再生。
- **待同步文档清单**（本轮受「只写本报告」约束，列为待办而非完成项）：① sdk-contract.md §2.2/§2.3——TOML → XML+XSD，含紧凑风格示例、pugixml 依赖、四投影与两段式存储；② net-abstraction.md §5.3/§6——Aeron 决策点关闭为「已定自研」、nng_wrapper 决策关闭为「已定删」；③ attribute-sync.md §7.1 的 protobuf 示例标注为「编码布局参考，契约源为 XML」；④ §12.3/§14 表内「TOML」字样随 ①② 联动修订。

> **同步落地记录（2026-09-28，门禁放宽为「只写 docs/」后执行，随本次提交）**：①②③⑤⑥ 已全部落地——sdk-contract.md（摘要 1/3(a) 契约形态 TOML→XML+XSD、§2.2 对照表按 C-50 重写、§2.3 换紧凑 XML 示例并增 entities.xml 继承说明、§3 目录/§7 storage.xml 指针/§8 P1 措辞、交集表补 xml-generation.md）；net-abstraction.md（摘要 3/6、§5.3 行、§6 两行关闭为「已定自研/已定删」、§7 P3 措辞、新增 §5.5 Mercury filter 双族蓝本、交集表补 xml-generation.md）；attribute-sync.md（摘要 5、§7.1 标题与标注、§8.1 第 2/4 条按 C-51 重写并给 1/3 挂复核标记、附录速查表属性声明行与持久化行按 C-51/C-52 改写）。④ 经复核**不改写评审记录**：§12.3/§14 的 TOML 字样均位于「14.1 自行假设」标记的历史评审文本内，取代关系已由 §15.3 显式声明，事后改写会破坏审计链——以本条判定为 ④ 的闭环方式。

### 15.7 实读核对记录（本节）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| utils/loop_buffer.h、utils/data_queue.h 存在（9260B/7660B） | ls | 已核（此前仅文档引用，本轮补实测） |
| `APOLLO_ENABLE_IPC` 默认 OFF（CMakeLists.txt:37） | rg | 已核 |
| ipc 树规模（include 10 文件 3435 行 / src 8 文件 4378 行） | find + wc（第六轮普查时实测） | 已核 |
| 其余行号（B 栈规模、channel.cpp:358-389、gateway-app/CMakeLists.txt:24、:281、sql_template.cpp 各行、attribute_id.h 分段） | §12.4/§13/§14.4 已核行号，未改动 | 沿用 |

---

## 16. 第七轮（2026-09-28 追加）：BigWorld / KBEngine / skynet 源码对照——文档缺什么、什么不合理（C-50 … C-52、G-1 … G-7）

### 16.1 范围与方法

- 对象：① `/home/cui/workspaces/BigWorld`（官方 14.4.1 全源码，服务端在 programming/bigworld/{server,lib}）；② `/home/cui/workspaces/skynet`（本地源码）；③ KBEngine——本地未找到，按用户指示从其 GitHub fork（cuihairu/kbengine@master）浅克隆到 `/home/cui/workspaces/kbengine`：首次克隆 fetch-pack 早断失败（git 自动清理半成品），重试成功。该 fork 带用户侧 CMake/vcpkg/typing 现代化改动（KBENGINE_TYPING_TODO.md、vcpkg.json），本轮仅取其上游 KBEngine 机制，fork 改造本身不在评审范围。
- 方法：BigWorld/skynet 由只读子代理做广度提取，主线对承重结论抽查复核（Account.def 样例原文、witness.cpp dumpAoI 块、prioritis 命中面）；KBEngine 主线直读。对照物 = docs/design 五份设计文档 + 本报告。
- **自行假设注明**：BigWorld 正式实体 .def 资产包未随源码发布（game/res 仅 Python 库 config），实体样例取仓库内 examples（Account.def）；KBEngine 的 assets 为独立仓库未随 fork，样例取 kbe/res/sdk_templates。.def 语义以解析器源码为准。
- 产出三类：**C-50…C-52**（文档论断与源码不符，什么不合理）；**G-1…G-7**（三家皆有而六份文档无落点的能力，缺什么）；**正面核对**（文档论断经源码检验成立——防「修正」矫枉过正）。

### 16.2 三家机制要点（源码证据）

**BigWorld 14.4.1（官方源码）**

- **.def = XML**：解析走 BWResource::openSection（lib/entitydef/entity_description.cpp:184-190），XML 实现在 lib/resmgr/xml_section.hpp；样例 examples/client_integration/.../entity_defs/Account.def 全 XML（`<Properties>/<Type>/<Flags>/<Persistent>/<ClientMethods>/<BaseMethods>`）；`<Parent>` 解析期递归展开（entity_description.cpp:194-203）；持久化标记 `<Persistent>`（data_description.cpp:220）、`<Identifier>`（隐含 Indexed+Unique，:228-233）、`<Indexed>`（:238-250）、`<DatabaseLength>`（:314）。
- **固定步长 game tick**：cellapp.cpp:806-808 `addTimer(1000000/updateHertz, TIMEOUT_GAME_TICK)`，:946-948 分派 handleGameTickTimeSlice；主循环 = 事件分发（server_app.cpp:240-242 processUntilBreak）——消息到达即处理、游戏逻辑按 tick 推进，两者并存；负载按 tick 三分统计（updateLoad，cellapp.cpp:1177-1190）。
- **witness 同步**：客户端上报更新率（server_connection.cpp:2370-2373 updateFrequencyNotification）、per-client 字节预算（:2034 tickByte_/updateFrequency）；per-(viewer,entity) 优先级堆 + volatile/event 双序号（witness.cpp:2500-2514 dumpAoI 自述、:1254 pop_heap）；OWNED 实体直发最近位置（:850-865）。
- **Mercury**：TCP/UDP 双通道抽象（channel.hpp:104 isTCP），进程间主打 UDP + 窗口重传（udp_channel.cpp:92-108 窗口 /:866 超窗即停 /:1052 起 重发驱动）+ Bundle 自动分片（udp_bundle.cpp:567）；加密/压缩/websocket 全部是 filter 插件（encryption_filter/message_filter/packet_filter/websocket_stream_filter）。
- **存储/容灾**：Indexed/Persistent 属性成 MySQL 列（column_type.cpp:95-129），其余 blob 序列化（mappings/{blob,class,composite}_mapping.cpp）；热备 backup_sender.hpp:52-61 分帧发送 + 一致性哈希 backup_hash/backup_hash_chain + secondary db 任务族 + reviver 宕机接管；dbappmgr 扩缩容哈希再分布（dbappmgr.cpp:472/:637）。
- **进程编队**：bwmachined 本机守护 + machine_guard 协议 birth/death 通知/订阅/广播（machine_guard.hpp:496-497/:609-613/:882-883）；cellappmgr 负载平衡（cellappmgr.hpp:43/:192-194）。
- 集中日志 logger_endpoint；内置类型注册表 data_types/*（每类型一个 DataType 子类）；无脚本热更路径。

**KBEngine（cuihairu fork@master，取上游机制）**

- **.def = XML**：entitydef.cpp:188-210 以 tinyxml2 先解析 entities.xml、再逐实体加载 `<名>.def`；DetailLevels（NEAR radius/hyst，:409-417）；样例 kbe/res/sdk_templates/.../Account.def 与 BigWorld 同构（`<Properties>/<ClientMethods>/<BaseMethods>/<CellMethods>`）——**两家同构血统，不是「自研语法」**。
- **固定步长 tick**：gameUpdateHertz=10（kbengine_defaults.xml:5）；handleGameTick（cellapp.cpp:252-261，updateLoad 先行 + updatables_.update()）。
- **存储**：复杂类型全落 BLOB（entity_table_mysql.cpp:699-723 ARRAY/FIXED_DICT/PYTHON、:754-758 ENTITYCALL/Component）；**Indexed 属性建真实 MySQL 索引**（:236-300，ALTER TABLE ADD INDEX :113）；**无 journal**（db_mysql 全树 journal/WAL/redo 零命中）；**Archiver 周期归档**（默认 300s，kbengine_defaults.xml:621）——实体随机序列、按「数量×idx/周期」每 tick 平滑摊写防写风暴（archiver.cpp:26-63）+ per-entity shouldAutoArchive；SQL 语句按表注册成映射表（EntitySqlStatementMapping，entity_sqlstatement_mapping.h:9-27，query/insert/update 三族）。
- **进程**：server/machine 广播发现（machine.cpp:646-670 KBE_PORT_BROADCAST_DISCOVERY）；loginapp 独立登录进程（含 clientsdk_downloader——SDK 下发即登录链路一环）。
- 网络：TCP 通道为主，UDP 组件并存（endpoint.cpp:321/:427 SOCK_DGRAM、udp_packet_receiver/listener_udp_receiver）。

**skynet（本地源码）**

- **纯消息驱动、无 tick**：线程编队 monitor+timer+socket+N worker（skynet_start.c:209-227）；每服务私有 mq（spinlock，skynet_mq.c:22）+ 全局队列，一次 dispatch 一条消息后重新入队（skynet_server.c:293-315）；worker 权重分频（skynet_start.c:213-217，dispatch `n >>= weight` skynet_server.c:316-318）；全局队列空则 cond_wait，由 timer/socket 线程唤醒（skynet_start.c:167-174）；过载阈值 MQ_OVERLOAD 1024（skynet_mq.c:19）。
- **五层时间轮**：near 256 槽（TIME_NEAR_SHIFT 8）+ 4 层×64 槽（skynet_timer.c:17-21,40-41）；独立 timer 线程 ~2.5ms 一 tick（skynet_start.c:131-140）；到期即投 PTYPE_RESPONSE 消息（skynet_timer.c:134-143）——回调与普通消息同路。
- **gate**：长度头 2B/4B 启动参数二选一（service_gate.c:352-363）；socket 线程独占 epoll、消息拷贝交接给服务（skynet_socket.c:48-59）；websocket 在 Lua 层（lualib/http/websocket.lua；netpack 2B 大端长度头 lua-netpack.c:191-241）——帧定界与消息体两层独立选配。
- **sproto**：类型集仅 integer/string/boolean + 自定义类型/数组；无枚举/命名空间/**无版本与握手校验**（schema 演进全靠 tag 兼容纪律，test/sharemap.sp 样例）；编码 tag+delta 变长（sproto.c:224-227）。
- **集群**：32 位 handle 高 8 位 harbor id（skynet_handle.c:33,111）；节点间 cluster 全 Lua（clusterd.lua:44,91-105，TCP socketchannel）；**harbor（C 服务 service_harbor.c）与 cluster（Lua 服务族）两代 IPC 并存于同一框架**。
- **可观测性三板斧**：debug_console telnet（mem/stat/task/run 在线执行，debug_console.lua:147-174）；monitor 线程以版本号检测服务卡死（skynet_monitor.c:31-45，只报警不杀）；独立 logger 服务（service_logger.c:9-27）。

### 16.3 什么不合理：文档论断与源码不符（C-50 … C-52）

**C-50 sdk-contract.md §2.2 首行「自研 .def 语法（EOF 边界/宏/实体定义）」与两家源码不符——.def 全为 XML。**
- BigWorld：entity_description.cpp:184-190 BWResource::openSection + Account.def 全 XML（16.2）；KBEngine：entitydef.cpp:188-210 tinyxml2 解析 entities.xml + 每实体 .def（16.2）。两家同构血统（KBEngine 标签体系沿用 BigWorld：Flags/Persistent/Indexed/Unique/DatabaseLength/Parent）。「EOF 边界/宏」无出处，疑与 MSVC linker .def 混淆（§13 已核实 tools/*.def 是链接器定义文件，非实体定义）。
- 影响与修正：① sdk-contract.md 执行摘要第 3 条(a) 与 §2.2 对照表首行的理由栏需重写——.def 本就是 XML，则 §15.3 的 XML+XSD 决策与两家先例**同构**；红利论证从「语法差异」转为「XSD 校验（key/keyref/enumeration）是两家都没有的形式化层」（两家都只解析不校验：tinyxml2 无 schema 层，entitydef.cpp:193 直证；BigWorld XMLSection 亦无）——错拼标签静默缺省而非报错；② 归入 §15.6 待同步文档清单第 ⑤ 项。

**C-51 attribute-sync.md §8.1 五条指控中第 2/4 条与 KBEngine 源码不符（第 1/3/5 条成立）。**
- 第 1 条 blob 化：**成立**（ARRAY/FIXED_DICT/PYTHON 等 → FIELD_TYPE_BLOB，entity_table_mysql.cpp:699-758）。
- 第 2 条「仅 base 持久化 + 快照式写……回档窗口不可控」：**表述失实**——有 Archiver 周期归档（默认 300s，kbengine_defaults.xml:621），随机序列平滑摊写 + shouldAutoArchive per-entity 控制（archiver.cpp:26-63）。「不可控」应修正为「**窗口默认粗（300s）且非脏驱动**——按实体归档、不感知属性 dirty」；后者才是真缺陷（其写负载平滑化机制反而是 write-behind 预算落库的同型参照）。
- 第 3 条无 journal：**成立**（db_mysql 全树 journal/WAL/redo 零命中）。
- 第 4 条「没有二级索引」：**失实**——`<Indexed>` 属性建真实索引（entity_table_mysql.cpp:236-300、ALTER TABLE ADD INDEX :113）。应修正为「仅显式 `<Indexed>` 属性有单列索引与点查；缺聚合/组合查询面」——批评方向（运营查询弱）不变，事实前提要改。
- 第 5 条分库分表：维持（多 database interface 有；实体冷热/归档/跨服迁移无完整故事）。
- 修正方法：§8.1 重写为「KBEngine 已做到 / 未做到」两列，批评力度不依赖失实前提；**不影响 §8.2 对策**（write-behind + 脏驱动 journal + 列提升仍是对「blob + 非脏驱动归档」的正确升级）。归入待同步清单第 ⑥ 项。

**C-52 attribute-sync.md 附录速查表「持久化 | BigWorld: base+backup | KBEngine: MySQL 实体表（弱）」暗示两家行存储有代差——实际同构。**
- BigWorld 非列属性同样 blob 化（db_storage_mysql/mappings/{blob,class,composite}_mapping.cpp；仅 Indexed/Persistent 成列，column_type.cpp:95-129）——与 KBEngine 完全同型；两家「列式提升」亦同构（`<Indexed>`/`<Identifier>` ≈ `<Indexed>`/`<Unique>` ≈ 本设计 §8.2 column:true）。
- 真正的代差在**备份与接管链路**（BigWorld：backup hash 链/secondary db/reviver；KBEngine：无此层）——速查表该行应改写为「行存储两家同构（列+blob），BigWorld 强在备份容灾」，避免把容灾优势误记为存储格式优势。

### 16.4 缺什么：三家皆有而六份文档无落点（G-1 … G-7）

**G-1 进程编队与服务发现。** BigWorld bwmachined + machine_guard 生死广播（machine_guard.hpp:496-613/:882-883）、KBEngine machine 广播发现（machine.cpp:646-670）。apollo 六份文档把进程间通信推迟 P3（§15.2），但「进程如何被发现、如何感知彼此死亡」连设计占位都没有。落点：P3 前置一节，对照 machined 守护 + UDP 广播两种先例定形态（与 §15.2 自研纪律对齐）。

**G-2 备份/容灾与宕机接管。** BigWorld：baseapp 热备分帧（backup_sender.hpp:52-61）、一致性哈希备份链（backup_hash/backup_hash_chain）、secondary db、reviver 接管、cellappmgr 崩溃后在幸存 CellApp 重建 cell。apollo 只有 attribute-sync §8 的「崩溃后数据不丢」（write-behind journal），**进程级高可用零设计**。落点：文档显式声明「单进程阶段无高可用」，P3 骨架列 backup-hash 链与 reviver 两个参照。

**G-3 优雅停机序列。** 停机时 flush write-behind journal → 停收新连接 → drain 在途帧 → 落库 → 按依赖逆序停模块——六份文档零落点（KBEngine 实体销毁路径 onDestroyEntity→writeToDB，baseapp/entity.cpp:698-731，是停机落库的零件级参照；数据面即 §15.5 三语句）。落点：attribute-sync §10 六阶段后补「阶段 7：停机（逆序 drain）」或独立小节。

**G-4 定时器轮。** skynet 五层时间轮 + 独立线程 2.5ms tick + 到期即消息（skynet_timer.c:17-21,40-41、skynet_start.c:131-140、skynet_timer.c:134-143）。apollo 无定时器模块（ssengine-reference §4.3 第一佐证，此处第二佐证 + 实现参照）。落点差异要写明：skynet 独立线程驱动、回调走消息队列——与其消息驱动范式同构；apollo 按单写者纪律应**挂在主循环固定阶段**（数据结构抄时间轮，驱动权留 game loop，回调在 owning thread 直接执行）。

**G-5 监控/调试通道。** skynet 三板斧（debug_console 的 mem/stat/task/run、monitor 版本号卡死检测、独立 logger 服务，16.2）+ BigWorld 集中日志与 dumpAoI 式自省（witness.cpp:2470-2514）。apollo 文档只有 BI 分流（attribute-sync §8.2）与脚本错误审计（scripting-lua §6），**运行期自省通道为零**——线上「某场景线程是否卡死」「各场景实体数/帧耗时」无处可看。落点：admin 通道（net-abstraction control 通道）+ per-scene 心跳版本号（monitor 思想移植到场景线程，卡死即告警）+ telnet 式调试台（mem/stat/task 对应物），归未来 apps/ 运维工具。

**G-6 实体契约继承。** BigWorld `<Parent>` 解析期递归展开（entity_description.cpp:194-203），KBEngine 同构（entities.xml + .def 血统相同）。apollo §15.4 entities.xml 未定义继承/组合——属性逐实体重复声明、改一处动 N 实体的风险。落点：entities.xsd 增继承（xs:extension 或生成期展开，**推荐生成期展开**——与两家解析期展开同构、错误信息更友好、生成器 CI 闸已有）。

**G-7 调度范式论证缺失。** 三家两种范式并存：BigWorld/KBEngine 固定步长 tick（cellapp.cpp:806-808；kbengine_defaults.xml:5 gameUpdateHertz=10 + cellapp.cpp:252-261），skynet 纯消息驱动无 tick（skynet_server.c:293-315 一条消息一 dispatch + 空队列 cond_wait skynet_start.c:167-174）。apollo 文档直接采用 tick（attribute-sync §10 六阶段、scripting-lua 指令预算按帧）但**从未论证为什么不选 skynet 式**。落点：一节简短论证——权威属性同步的 seq 语义、派生重算 DAG 的拓扑批处理、帧预算（token bucket/指令预算）都以确定性节拍边界为前提；skynet 范式适合无共享状态的服务编排，与「实体归属单写者线程」的权威模型错位——把 §0「思想与形态之分」应用到调度范式即得结论，写下来防止未来再议。

### 16.5 正面核对与对既有决策的增量

- **正面核对（防矫枉过正）**：① attribute-sync.md 对 BigWorld 带宽控制的概括（per-client 预算 + per-(viewer,entity) 优先级）与源码相符（server_connection.cpp:2034/:2370-2373、witness.cpp:2500-2514/:1254）——§3/§5 的 ChangeHistory/ViewerState/优先级设计与基准一致；② §15.5「语句即数据」获外部同构佐证：KBEngine EntitySqlStatementMapping 即 C++ 内建版 mapped statement（entity_sqlstatement_mapping.h:9-27），apollo 方案只是把声明层从 C++ 挪进 XML（外加两家都没有的 XSD 层，C-50 红利论证）；③ §15.2「禁止两套 IPC 并存」获 skynet 历史佐证（harbor 与 cluster 两代并存于同一框架）；④ tick 选型与两家 MMO 先例一致——选型无异常，缺的只是 G-7 的论证文字。
- **对 §15.2 的增量**：Mercury 的 udp_channel 窗口重传 + Bundle 分片 + **filter 插件体系**（加密/压缩/websocket 全是帧管线插件）是 L1/L2 自研的直接蓝本；建议帧管线把加密/压缩设计为 filter 插件位而非硬编码——P2 会话层与 P3 自研总线共用此形态。
- **对 §15.4 的增量**：entities.xml 增继承机制（G-6）；存储投影的 persist/column 提示与两家 `<Persistent>`/`<Indexed>` 语义同位，XSD keyref 已锁，无需改动。
- **待同步文档清单追加**（§15.6 之 ⑤⑥）：⑤ sdk-contract.md 执行摘要第 3 条(a)/§2.2 首行 .def 表述修正（C-50）；⑥ attribute-sync.md §8.1 第 2/4 条重写 + 附录速查表持久化行改写（C-51/C-52）。

### 16.6 实读核对记录（本节）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| BigWorld .def=XML（entity_description.cpp:184-203）+ Account.def 样例 XML | 子代理提取 + 主线 sed 复核样例原文 | 属实（C-50） |
| BigWorld tick（cellapp.cpp:806-808/:946-948/:1177-1190、server_app.cpp:240-242） | 子代理提取（行号原样引用） | 属实（G-7） |
| witness 优先级堆 + volatile/event 双 seq（witness.cpp:2500-2514 dumpAoI 块） | 主线 sed 复核原文 | 属实（16.5-①） |
| 客户端上报频率/字节预算（server_connection.cpp:2370-2373/:2034） | 子代理提取 | 属实（16.5-①） |
| Mercury UDP 窗口重传/分片/filter 族（udp_channel.cpp:92-108/:866/:1052+、udp_bundle.cpp:567） | 子代理提取 | 属实（16.5 增量） |
| BigWorld 列+blob 存储（column_type.cpp:95-129、mappings 三文件） | 子代理提取 | 属实（C-52） |
| bwmachined/machine_guard/backup_sender/reviver/secondary_db/dbappmgr 各行号 | 子代理提取 | 属实（G-1/G-2） |
| KBEngine .def=XML（entitydef.cpp:188-210/:409-417）+ Account.def 模板 | 主线 rg + sed 直读 | 属实（C-50） |
| 复杂类型全 BLOB（entity_table_mysql.cpp:699-758）；Indexed→真实索引（:236-300/:113） | 主线 rg 直读 | 属实（C-51） |
| 无 journal（db_mysql 全树 journal/WAL/redo） | rg（exit 1） | 属实（C-51） |
| Archiver 周期归档 + 随机摊写（archiver.cpp 全文；kbengine_defaults.xml:621/:626） | 主线 cat 直读 | 属实（C-51） |
| EntitySqlStatementMapping（entity_sqlstatement_mapping.h:9-27） | 主线 head 直读 | 属实（16.5-②） |
| machine 广播发现（machine.cpp:646-670）；loginapp/clientsdk_downloader 存在 | 主线 rg + ls | 属实（G-1） |
| KBEngine tick（gameUpdateHertz=10 kbengine_defaults.xml:5、cellapp.cpp:252-261） | 主线 rg + sed | 属实（G-7） |
| skynet 各行号（调度/时间轮/gate/sproto/集群/监控，skynet_start.c/skynet_server.c/skynet_timer.c/skynet_mq.c/service_gate.c/skynet_socket.c/lua-netpack.c/sproto.c/skynet_handle.c/clusterd.lua/skynet_monitor.c/debug_console.lua/service_logger.c） | 子代理提取 | 属实（G-4/G-5/G-7/16.5-③） |
| kbengine 克隆：首次 fetch-pack early EOF 失败（git 自动清理）、重试成功 | 后台任务输出 | 事件记录（16.1） |

### 16.7 待同步清单深化（2026-09-28 追加）：Mercury filter 双族蓝本与 entities.xml 继承

> 本小节把 §16.5「对既有决策的增量」中的两项从结论深化为落地形态：filter 插件体系（并入 §15.6 待同步清单 ② net-abstraction.md 的修订范围）、entities.xml 继承（并入 ① sdk-contract.md 的修订范围）。全部为设计深化，无源码改动；新增证据经补充实读（16.7.3）。

#### 16.7.1 Mercury filter 双族 → apollo L1/L2 帧管线的蓝本

**BigWorld 事实（补充实读）**：

- **双族分离**：数据报族 `PacketFilter`（挂 UDP 通道）——`send(PacketSender&, addr, Packet*) -> Reason`（packet_filter.hpp:37）、`recv(PacketReceiver&, addr, …) -> Reason`（:48）、`maxSpareSize()`（:56，**过滤器向管线申报尾部预留量**，加密填充/zstd 上界不触发二次分配）；流族 `NetworkStream`（挂 TCP 通道）——`writeFrom(BinaryIStream&, bool shouldCork)`（stream_filter.hpp:46，cork=攒批立即刷的显式控制）、`readInto(BinaryOStream&)`（:55）。
- **实例面**：加密（encryption_filter + encryption_stream_filter）、消息级（message_filter）、`websocket_stream_filter`——**WS 帧定界（升级握手/掩码/ping-pong）整体实现为 TCP 通道上的一个流过滤器**，Mercury 核心对 WS 一无所知。
- 与 skynet 对照：帧定界（gate 2B/4B 长度头）与消息体（sproto）两层独立选配（16.2）——同一思想两家两个变体：BW 做成通道插件，skynet 做成服务组装件。

**映射到 apollo（net-abstraction.md 修订时的落点）**：

1. **L1 帧管线 = 有序 filter 链，不是硬编码 switch**。定界（magic/seq/CRC）之后接「解码阶段链」：解密 → 解压 → 分派；编码方向逆序。每阶段统一接口 `FrameFilter { encode(Frame&) -> Reason; decode(…) -> Reason; reserve() -> size_t }`——Reason 返回码与 Aeron offer/trySend 同一条「决策还给调用方」纪律（§15.2 已定）；`reserve()` 对应 BW maxSpareSize，帧缓冲一次分配。
2. **两族都要**：movement 通道的不可靠演进路径（UDP）挂数据报族；TCP/WS 客户端会话挂流族——L0 传输类型决定族，L1 filter 实现（CRC/压缩）两族共用。
3. **filter 栈由契约声明**：帧头 ver 字段选择 filter 组合，服务端按握手协商结果装配链——不出现运行期字符串注册（容器纪律的 L1 落点）。WS 接入 = 一个 stream filter，**不新开网络栈**——BW 用同一 Mercury 承载 WS 即为「禁止第五套网络栈」（§15.2）的先例佐证。
4. **边界纪律**：filter 只做字节↔字节变换、不认识消息语义；seq/ack/水位/重传归 L2 通道核心——BW 的窗口重传在 udp_channel 而非 filter（16.2），同构。校验失败（CRC 坏/解密失败）返回 Reason 后断链或丢弃，不进 L2。

#### 16.7.2 entities.xml 继承：语义取两家并集、机制取生成期展开

**两家事实（补充实读）**：

- BigWorld：`<Parent>` 在**自身标签解析之前**递归解析父 .def（entity_description.cpp:195-203）——父字段先入、子字段覆盖（如 `clientName_ = pSection->readString("ClientName", clientName_)` 以父解析值为缺省，:216 附近）。**无环检测**：rg `cycle|recursi|visited` 于 entity_description.cpp 零命中——循环 parent = 无界递归，官方引擎未设防。
- KBEngine：`loadParentClass`（entitydef.cpp:890-903）读 `<Parent>` 节点 → 加载父 .def → 调 `loadDefInfo`（:913-916），而 loadDefInfo 自身又调 loadParentClass（:372）——**互递归、同样无深度守卫**；组件定义同构（:774-776）。

**apollo entities.xml 继承设计（写入 sdk-contract.md 时的形态）**：

1. **声明**：`<entity id="Avatar" parent="Monster">`，单继承（与两家一致；多继承的菱形问题不值得引入属性表）。
2. **校验三层**：① XSD `xs:keyref`——parent 必须引用已声明 entity id，悬垂引用提交前红；② **环检测必须生成器自己写**——两家先例证明「解析期递归」在环上会崩（BW 连守卫都没有），生成器拓扑排序 + 明确报错是 CI 闸新增断言；③ 展开后 attr id 全局唯一 `xs:key` 校验照旧。
3. **合并语义**：父先入、子覆盖（与 BW 递归序一致）；**禁止同名改型**（父声明 int64 子改 string = 生成错误，报错带出处链 `Avatar←Monster`）；每属性保留 provenance（来自哪个实体的声明），错误信息与 diff 可归因。
4. **展开时机**：生成期扁平化，三个消费投影（协议/SDK/服务端）只见扁平结果；`schema_hash` 对**源文件 + 生成器版本**计算——展开确定性由版本锁定，父改动自动传播到全部子实体生成产物，CI 产物 diff 天然捕获「改父忘生成」。
5. **不学**：BW 的 `<Interface>` 多接口分发（随 base/cell 语义而来，apollo P3 前无对应物）、任何运行期加属性路径——契约保持静态。

#### 16.7.3 实读核对（本小节）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| BW `<Parent>` 先于自身解析递归（entity_description.cpp:195-203）；无环守卫（rg cycle/recursi/visited 零命中） | sed + rg | 属实 |
| BW ClientName 以父解析值为缺省（entity_description.cpp:216 附近 readString 带当前值） | sed | 属实 |
| PacketFilter send/recv/maxSpareSize（packet_filter.hpp:37/:48/:56） | grep virtual | 属实 |
| StreamFilter writeFrom(shouldCork)/readInto（stream_filter.hpp:46/:55）；filter 文件族清单（ls lib/network/*filter*） | grep + ls | 属实 |
| KBEngine loadParentClass ↔ loadDefInfo 互递归（entitydef.cpp:890-916/:372）；组件同构（:774-776） | rg + sed | 属实 |

---

*评审基线（源码）：main @ 35a9c528（无源码变更）。文档基线：六份文档随 a2ab6525；§8 随 86be18d2；§9 随 c99d9d9e；§10 随 7ce2849f；§11 随 ec4649a7；§12 随 0b982a41；§13 随 9adb3f34；§14 随 98d021d2；§15 随 b9bf5321；§16 随 047d0002；§16.7 随 d691fac2。所有行号对应该基线；§16 的三家框架行号对应各自工作副本当前态（BigWorld 14.4.1 官方包、skynet 工作副本、kbengine fork master 浅克隆）；§15.6 的同步落地记录随 263a3888。*

## 16.8 第八轮深化（2026-09-28 追加）：代码影响项的新模块归属与依赖方向（G-4 展宽，apps/ 运维形态为背景）

> 任务口径：聚焦上轮遗留的 G-4 新模块归属——在 apps/ 运维形态下论证新增模块（filter 插件体系、继承生成器、config_manager 桩清理等代码影响项）的归属边界与依赖方向。本节为分析文字，无源码改动、不派发源码任务；门禁收窄为「只写本报告」，对既有设计文档的修订建议列入待同步清单 ⑦–⑪（16.8.5）。
> 自行假设（非交互约定，A1–A3）：**A1** apps/ 五进程壳（base/cell/game/gateway/login-app）为骨架现状——apps/base-app 的 main 仍是手写组合根（全局指针 + signal 回调，main.cpp:3-25，本地头 base_server.hpp），未走 modules/runtime 的 application_host——成熟度不影响归属论证，论证对象是依赖方向与边界；**A2** G-1/G-2 的进程级设计（编队/备份容灾）不在本节展开（任务聚焦为「新增模块归属」）；**A3** modules/bigworld 与 include/apollo/bw 按 BigWorld.h 自述定性为「legacy 兼容/参考层」，其完成度未全面审查，定性仅用于归属判断。

### 16.8.1 版图现状（实读普查，对应该报告写作时的工作副本 @ e6808a7c）

- **apps/**：五壳已存在且有实现雏形——base-app（main.cpp + base_server + database_service）、cell-app（cell_server + config.hpp）、gateway-app（gateway_server + ingress 连接注册表）、login-app（login_server）、game-server（仅 main.cpp）。
- **modules/**：base（include/src/tests 三件套——系统组件层，非早期纯头文件形态）、net（protocol/tcp/rpc 三子树 + rpc_legacy/message_codec/session_manager——C-29 四套网络树的成员）、protocol（socket/codec/messages/nng_wrapper——§15.2 退役清单内）、game（attributes/battle/session/core/world）、data（orm/redis/cache/core）、core（config/log/module_manifest）、runtime（application_host/world_host/signal_source——进程宿主层）、bigworld（见 A3）。
- **include/apollo/**：net（session/connection 等 L0-L2 原型）与 network（messaging/rpc/transport）并存——四套网络树的另两处；bw（runtime.h/entity.h——bw 兼容层的接口面）。
- **待落位新部件清单**（承接 §16.7 与各设计文档代码影响项）：① FrameFilter 插件体系（net-abstraction §5.5）；② 契约/表生成器含 entities.xml 继承展开（sdk-contract §3、xml-generation §4、16.7.2）；③ config_manager parseXml/parseLua 桩清理（xml-generation §7 P1，C-49 家族）；④ 定时器轮（G-4 本体，ssengine-reference §4.3 目标设计）；⑤ 运维观测通道（G-5 载体）。

### 16.8.2 三家先例：同类部件放在哪（行号实证）

| 部件 | BigWorld | KBEngine | skynet | 共同形态 |
|---|---|---|---|---|
| 帧管线 filter | **与通道同库**：packet_filter/stream_filter 在 lib/network（16.7.1）；udp_channel.hpp:27 前置声明、:81 构造注入 `PacketFilterPtr pFilter`、:139-140 成员存取——接口抽象、实例同库、**注入式装配** | 无 filter 层（TCP 通道裸用，16.2）——**反例**：每加一种封装都要改通道本体 | WS 帧定界做在服务组装层（lualib/http/websocket.lua）——其消息驱动范式的脚本域变体 | filter 与通道/帧代码同层、与消息语义分离 |
| 定时器 | **最底层公共库**：lib/cstdmf/time_queue.hpp:60 TimeQueueBase、:74 TimeQueueT，自带单测（unit_test/test_time_queue.cpp） | lib/common/timer.h:101 TimersBase、:108 TimersT——同为公共库 | 核心线程无条件编队 monitor/timer/socket（skynet_start.c:209-211） | 定时器在框架公共层，不在任何具体 app |
| 生成器/工具 | server/tools **独立二进制族**（bwmachined/bw_profile/message_logger/sync_db/snapshot_helper 等，目录实读）；entitydef 则运行期解析（lib/entitydef，C-50 无校验） | 配置转换器在**源码树外**：kbe/tools/xlsx2py/（ExcelTool.py 等 Python 族，Excel→运行期 py 配置）；kbe/src/server/tools/{bots,logger,kbcmd,…} 为独立工具进程 | —（无独立工具树；debug_console 为按需 Lua 服务） | 「源→运行期工件」转换器与运维工具一律独立二进制，不进运行期链接图 |
| 观测通道 | 进程内建自省方法（witness.cpp:2470-2514 dumpAoI）+ 独立 logger 进程 + server/tools/{bw_profile,message_logger} | tools/logger 独立进程 | 核心内 monitor 线程版本号检测（skynet_monitor.c:31-45）+ 按需 debug_console 服务（telnet mem/stat/task/run） | **检测原语在核心/进程内，聚合呈现在外挂工具进程** |

### 16.8.3 归属判定（逐项）与依赖方向

**① FrameFilter 插件体系 → 收敛后的 modules/net（L1/L2 重建处），不进 modules/protocol。**
- 归属理由：BW 先例 filter 与 udp_channel 同库、经构造注入（udp_channel.hpp:81/:139-140）——filter 接口与字节级插件（CRC/压缩/加密/WS 流插件）与 L0-L2 同属 net；modules/protocol 现树在 §15.2 退役清单内、重建后只承载**生成产物**（消息表/访问器）。filter 认字节不认消息，放 protocol 会倒转 protocol→net 的依赖箭头。
- 依赖方向：`FrameFilter` 接口与内置插件定义于 net；protocol（生成 codec）经 L1 的 codec 槽位**注入**（对齐 BW pFilter 注入式构造）；apps/gateway-app 是**装配方**（挂哪条 filter 链——来自契约声明），不是实现方。filter 插件禁止 include 任何消息头/协议类型。
- 自反约束（重要）：C-29 四套网络树并存（modules/net/{protocol,tcp,rpc}、modules/protocol、include/apollo/net、include/apollo/network，§12.1/§12.2）+ §15.2「禁止第五套网络栈」纪律——**filter 落地前必须先收敛四套**，否则 filter 链会成为第五处网络代码、自打纪律。本判定附带前置条件：filter 进的是收敛后的 modules/net，不是现存四套中任何一套的原样。

**② 契约/表生成器（含 entities.xml 继承展开）→ sdks/gen/ 构建期工具，运行期依赖图之外。**
- 归属理由：KBE 把配置转换器放源码树外（kbe/tools/xlsx2py，Python 工具族实读）——「源→工件」转换不与运行期代码混居；BW server/tools 全族同理。sdk-contract §3 已定 sdks/gen「C++ 单二进制、入 CI」；本节补的是**链接边界**：gen 不被任何运行期目标链接，产物（typed struct/loader/schema_hash 头）被 modules/protocol、tables 消费模块、sdks/* 链接——依赖箭头只在构建期经过 gen，运行期 include 图里没有 gen。一致性由 CI 双闸（xmllint + 产物 diff）粘合。
- 继承展开（16.7.2：拓扑排序 + 环检测断言 + 父先入子覆盖 + provenance）全部落在 gen 内部，三个投影只见扁平结果——展开逻辑不进入任何运行期模块。
- 与两家的差异（记录为有意的偏离）：BW/KBE 都选运行期解析 .def（lib/entitydef、entitydef.cpp:188-210），每启动付解析成本且无 schema 层（C-50）——apollo 把解析+校验前移 CI，代价是生成器成为一等仓库公民。

**③ config_manager 桩清理 → modules/core/config 内部收缩（删桩 + 删路由），不新增模块、不回灌 XML。**
- parseXml/parseLua 桩与 ConfigFormat::Xml/Lua 路由诚实化删除（C-49 家族，xml-generation §7 P1 已记）；XML 装载走生成 typed loader，loader 归属随 schema 域（契约产物→modules/protocol；tables 产物→消费该表的模块）。
- **本节收紧一处既有表述**：xml-generation.md §1.1「接缝已留、实现缺席——本设计直接在接缝上立规矩」暗示在 ConfigFormat::Xml 路由上补实现；本节判定为**路由随桩一并删除**——若把 XML 树回灌 config_manager 的通用 ConfigNode，等于重建「运行期二次反射」，违反 xml-generation §4 产物 2 的禁令。修订列待同步 ⑦。
- 依赖方向：modules/core/config 保持通用树职责（INI/JSON 外围配置）被各模块依赖；生成 loader 只依赖 pugixml + base，**不依赖 config**——两套装载路径并存但分层不同（外围配置 vs 热路径数据/契约），不做「统一入口」。

**④ 定时器轮（G-4 本体）→ modules/base 新组件（数据结构 + 单测），驱动权在 game loop；不新建 modules/timer、不落 modules/bigworld。**
- 三家先例一致（16.8.2 表）：BW TimeQueue 在最底层公共库 lib/cstdmf 且自带单测（time_queue.hpp:60/:74、unit_test/test_time_queue.cpp）；KBE Timers 在 lib/common（timer.h:101/:108）；skynet timer 是核心线程编队成员（skynet_start.c:209-211 无条件创建）。公共层承载定时器是三家共识。
- apollo 落点辨析：modules/base 现为 include/src/tests 三件套（16.8.1 实读）——系统组件层的既定家，与 thread_pool/memory 同级；utils 收无状态原语（loop_buffer/data_queue/FileWatcher），时间轮带回调调度语义，不进 utils。**modules/bigworld 排除**：其自述为 legacy 兼容层（BigWorld.h:3-9「BigWorld API compatibility layer…New code should use apollo::bigworld」），timer.cpp:3-5 是转发桩（自认实现由 apollo::bw::Runtime 提供，runtime.h:14 `class Runtime`、:40 `addEntityTimer`）——参考件不是生产件的家；且兼容层自带实体运行时，与 modules/game 实体体系、attribute 三代容器（§12.2）构成又一层并行实体体系——**顺带发现（记为观察，不定罪，完成度未审见 A3）**：建议下轮审计把 bw/bigworld 兼容层列入未评审子系统清单（与 §15.2 已注 ipc 树并列）。
- 驱动方向（G-4 已论证驱动范式，本节补归属维度）：base/timer 只提供 O(1) 数据结构与到期回调收集；**tick 驱动权在场景线程主循环固定阶段**——不学 skynet 独立 timer 线程（那是其消息驱动范式的配套，论证见 G-7 与 attribute-sync §10.1）。

**⑤ 运维观测通道（G-5 载体）→ 两截归属：检测原语进模块，聚合工具进 apps/；依赖只许 apps→模块。**
- 先例形态（16.8.2 表末行）：skynet monitor 版本号检测在核心线程（skynet_monitor.c:31-45）、debug_console 是按需启动的外挂服务；BW dumpAoI 是 cellapp 内建自省方法（witness.cpp:2470-2514）、message_logger/bw_profile 是 server/tools 独立二进制；KBE tools/logger 独立进程。
- apollo 落点：检测原语（per-scene 心跳版本号、队列水位、实体计数、帧耗时）内嵌进 owning 模块与 net control 通道——**卡死时外挂工具自己也拿不到数据，所以原语必须在模块内**；聚合呈现（telnet REPL、在线执行、日志拉取）归 apps/（未来 admin 工具或 gateway-app 的 admin 面）。现有接缝已备：modules/runtime 的 application_host 已定义 ConsoleEvent/IConsoleEventSource（application_host.hpp:20/:28）——ops 工具的依赖面就是这个接口，模块侧零新增依赖。
- 依赖方向铁律：apps→modules→include 单向；ops 工具只触 control/debug 接口面，禁止模块 include apps 头、禁止 ops 工具直连 filter 链或属性内存（一切经只读自省接口）。

### 16.8.4 依赖方向总图（文字版）与落点判据

```
apps/{base,cell,game,gateway,login}-app   组合根：装配 + main（现状手写雏形，目标走 runtime host + core::di）
  ↓
modules/runtime（application_host / world_host / signal_source；console 事件源接口在此）
  ↓
modules/game ── modules/protocol（生成产物：消息表/访问器）
  ↓                    ↓
modules/net（L0-L2 + FrameFilter）   modules/data（异步 DB/连接池）
  ↓                    ↓
modules/core/{config,log}（通用树/日志）
  ↓
modules/base（线程/内存/ID/【时间轮←G-4 落点】） + include/apollo/utils
【图外】sdks/gen（构建期工具，运行期零链接）｜apps 运维工具（只触 control/debug 接口面）
```

新代码落点的三条判据（本节产出，后续轮次直接套用）：
1. **它被谁链接**——运行期目标链接 → 进 modules 分层；只有 CI/开发者跑 → 进 sdks/gen 或工具树；
2. **它认什么**——认字节 → net；认消息 → protocol（生成产物）；认实体 → game；认进程生命周期 → runtime/apps；
3. **谁不得反向依赖它**——被它依赖的层禁止反向 include（含构建期工具：运行期代码禁止 include gen 产物之外的 gen 内部头）。

### 16.8.5 待同步清单追加（门禁收窄为只写本报告，列为待办）

- **⑦** xml-generation.md §1.1/§7：ConfigFormat::Xml/Lua 路由最终删除（非「接缝上立规矩」），XML 装载走生成 loader，config_manager 保持 INI/JSON 通用树（16.8.3-③）。
- **⑧** net-abstraction.md §5.5/§6：filter 归属行——收敛后的 modules/net、注入式装配对齐 BW udp_channel.hpp:81/:139-140；四套收敛（C-29）为 filter 落地前置条件。
- **⑨** sdk-contract.md §3 / xml-generation.md §4：生成器「运行期依赖图之外」边界行（先例 kbe/tools/xlsx2py、BW server/tools）。
- **⑩** ssengine-reference.md §4.3：定时器轮归属行——modules/base 组件 + 单测（先例 BW lib/cstdmf 自带 test_time_queue），驱动权 game loop；modules/bigworld 的 addEntityTimer 定性参考件不入生产链。
- **⑪** G-5 载体两截归属：net-abstraction.md（control 通道承载检测原语上行）+ 未来 apps/ 运维工具边界（依赖面 = runtime 的 ConsoleEvent/IConsoleEventSource 接口）；先例 skynet debug_console、BW server/tools。

> **状态（16.9 追加，随本次提交）**：⑦–⑪ 修订文本已全部备妥（§16.9，含锚点原文与逐字替换文本）——门禁放宽后机械粘贴即闭环，无剩余分析工作。⑩ 已于 §16.10 升级为**审计链登记**（判定权威记录 = 本报告，粘贴随门禁放宽执行）；⑦⑧⑨⑪ 搁置状态以 §16.10.2 登记簿为准。

### 16.8.6 实读核对记录（本节）

| 引用 | 实测方式 | 结果 |
|---|---|---|
| apollo 版图：modules/*（base 含 include/src/tests）、apps/* 五壳、include/apollo/net 与 network 并存、modules/net 三子树、modules/runtime/application_host.hpp:13/:20/:28 | ls/find/head 主线直读 | 属实（16.8.1/16.8.3-⑤） |
| BigWorld.h 兼容层自述（:3-9）+ bigworld/timer.cpp 转发桩（:3-5）+ bw/runtime.h:14/:40 | head/sed 直读 | 属实（16.8.3-④，A3） |
| BW TimeQueue 在 lib/cstdmf（time_queue.hpp:60/:74/:31）+ 自带单测（unit_test/test_time_queue.cpp） | find + rg 直读 | 属实（16.8.3-④） |
| BW filter 注入式装配（udp_channel.hpp:27/:81/:139-140） | rg 直读 | 属实（16.8.3-①） |
| KBE Timers 在 lib/common（timer.h:101/:108） | rg 直读 | 属实（16.8.3-④） |
| KBE xlsx2py 在源码树外 kbe/tools/（xlsx2py.py + ExcelTool.py/config.py/functions.py） | ls/head 直读 | 属实（16.8.3-②） |
| skynet 核心线程无条件编队（skynet_start.c:209-211 monitor/timer/socket + worker weight 表） | sed 直读 | 属实（16.8.2） |
| BW server/tools 独立工具族（bwmachined/bw_profile/message_logger/sync_db 等）；KBE kbe/src/server/tools/{bots,logger,kbcmd,…} | ls 直读 | 属实（16.8.2/16.8.3-⑤） |

---

*评审基线（源码）：main @ 35a9c528（无源码变更）；§16.8.1 版图普查对应写作时工作副本 @ e6808a7c（普查对象为目录结构与少量头文件，未引用 35a9c528 后漂移的实现行号）。文档基线：六份文档随 a2ab6525；§8 随 86be18d2；§9 随 c99d9d9e；§10 随 7ce2849f；§11 随 ec4649a7；§12 随 0b982a41；§13 随 9adb3f34；§14 随 98d021d2；§15 随 b9bf5321；§16 随 047d0002；§16.7 随 d691fac2；§15.6 的同步落地记录随 263a3888（attribute-sync 深化随 e6808a7c）；§16.8 随 ef142854。所有行号对应该基线；§16 的三家框架行号对应各自工作副本当前态（BigWorld 14.4.1 官方包、skynet 工作副本、kbengine fork master 浅克隆）。

---

## 16.9 第八轮·续（2026-09-28 追加）：待同步清单 ⑦–⑪ 的修订文本落盘

> 门禁仍收窄为「只写本报告」（本轮用户指令），⑦–⑪ 的五份目标文档本轮不可写。与 ④ 的「判定即闭环」不同，⑦–⑪ 是实内容修订——本节把修订工作产品（锚点原文 + 逐字替换文本）全部备妥，门禁放宽后按条机械粘贴即闭环，无剩余分析工作。分析依据全部在 §16.8.3 已论证，本节不重复；口径保持 G-4 收窄，假设沿用 A1–A3；无源码改动。

### 16.9.1 ⑦ → docs/design/xml-generation.md（config 桩/路由最终删除，16.8.3-③）

**锚点 1**（§1.1 表行·判定列，原文）：

> 接缝已留、实现缺席——本设计直接在接缝上立规矩

**替换 1**：

> 接缝不补实现——路由随桩一并删除（architecture-review §16.8.3-③）：XML 装载只走本设计 §4 产物 2 的生成 typed loader，不回灌 config_manager 通用树

**锚点 2**（§7 P1 句尾，原文）：

> + config_manager 的 parseXml/parseLua 桩与 ConfigFormat::Xml/Lua 路由**诚实化删除**（stub 家族清理，C-49 纪律；改动点记录在案，随代码阶段执行）。

**替换 2**：

> + config_manager 的 parseXml/parseLua 桩与 ConfigFormat::Xml/Lua 路由**诚实化删除**（stub 家族清理，C-49 纪律；architecture-review §16.8.3-③：删除含路由本身、不补通用 XML 实现——config_manager 保持 INI/JSON 通用树职责，XML 装载只走本设计产物 2；改动点记录在案，随代码阶段执行）。

依据：16.8.3-③——「接缝上立规矩」与 xml-generation §4 产物 2 的「运行期二次反射」禁令存在张力，统一为删除路由、不补实现。

### 16.9.2 ⑧ → docs/design/net-abstraction.md（filter 归属与前置条件，16.8.3-①）

**插入 1**（§5.5 末尾追加一条）：

```markdown
- **归属与前置条件**（architecture-review §16.8.3-①）：FrameFilter 接口与内置插件落**收敛后的 modules/net**——先例：BW filter 与通道同库、构造注入（udp_channel.hpp:81/:139-140）；protocol（生成 codec）经 L1 codec 槽位注入，apps/gateway-app 只装配不实现。**前置条件 = C-29 四套网络树收敛**（§12.2、§15.2「禁止第五套」）——filter 不落现存四套中任何一套的原样，否则 filter 链即第五处网络代码。
```

**插入 2**（§6 决策清单表，Aeron 行后追加一行）：

```markdown
| FrameFilter 体系 | **新建**于收敛后的 modules/net（四套收敛为前置条件） | architecture-review §16.8.3-①：BW 同库 + 注入式装配先例（udp_channel.hpp:81/:139-140） |
```

### 16.9.3 ⑨ → docs/design/sdk-contract.md §3 + docs/design/xml-generation.md §4（生成器链接边界，16.8.3-②）

**插入 1**（sdk-contract §3「CI 强制」句后追加）：

```markdown
链接边界（architecture-review §16.8.3-②）：gen 是**运行期依赖图之外**的构建期工具——不被任何运行期目标链接，产物被 modules/protocol、tables 消费模块与各 sdks/* 链接，运行期代码禁止 include gen 内部头。先例：KBEngine 配置转换器在源码树外（kbe/tools/xlsx2py）、BigWorld 工具族独立二进制（server/tools）。
```

**插入 2**（xml-generation §4「实现形态」bullet 句尾追加）：

```markdown
链接边界（architecture-review §16.8.3-②）：gen 运行期零链接——产物被消费模块链接，生成器本体不进任何运行期目标（sdk-contract §3 同步此边界）。
```

### 16.9.4 ⑩ → docs/analysis/ssengine-reference.md §4.3（定时器轮归属，16.8.3-④）

**插入**（§4.3 末尾「成本」bullet 后追加）：

```markdown
- **归属（architecture-review §16.8.3-④）**：落 **modules/base 新组件（数据结构 + 单测）**——三家先例：BW TimeQueue 在最底层公共库 lib/cstdmf 且自带 unit_test（time_queue.hpp:60/:74、unit_test/test_time_queue.cpp）、KBEngine Timers 在 lib/common（timer.h:101/:108）、skynet 为核心线程编队（skynet_start.c:209-211）；模块只供 O(1) 结构与到期回调收集，驱动权按本节设计留在 game loop 固定阶段。**不落 modules/bigworld**——其为 legacy 兼容层（BigWorld.h:3-9），timer.cpp 是转发桩（:3-5，自认实现由 apollo::bw::Runtime 提供，runtime.h:40 addEntityTimer），仅作语义参考不入生产链。
```

### 16.9.5 ⑪ → docs/design/net-abstraction.md §7 P3（G-5 载体两截归属，16.8.3-⑤）

**锚点**（§7 P3 行，原文）：

> - **P3**：进程间总线自研落地（蓝本：§5.2 语义 + §5.5 Mercury filter 双族 + udp_channel 式窗口重传）、网关模式（gateway-app 接入）、Archive 类消息审计。

**替换**：

```markdown
- **P3**：进程间总线自研落地（蓝本：§5.2 语义 + §5.5 Mercury filter 双族 + udp_channel 式窗口重传）、网关模式（gateway-app 接入）、Archive 类消息审计；运维观测通道两截落位（architecture-review §16.8.3-⑤）：检测原语（per-scene 心跳版本号/队列水位/实体计数/帧耗时）内嵌 owning 模块并经 control 通道上行，聚合工具归 apps/（依赖面 = modules/runtime 的 ConsoleEvent/IConsoleEventSource，application_host.hpp:20/:28；先例 skynet debug_console/monitor、BW server/tools/{bw_profile,message_logger}）——模块零依赖 apps，ops 工具只触只读自省接口。
```

### 16.9.6 状态与核对

- ⑦–⑪ 共 **7 处插入/替换**（⑦ 两处、⑧ 两处、⑨ 两处、⑩ 一处、⑪ 一处），替换文本全部源自 §16.8.3 已论证判定与 16.8.2/16.8.6 已核实行号——**无新增源码引用、无新增实读义务**。
- 锚点核对口径：五份目标文档的锚点句均取自本会话实读的文件当前态（xml-generation/net-abstraction/sdk-contract @ d691fac2 后未再变更；ssengine-reference @ 35a9c528 后未变更）。粘贴时若锚点失配，以目标文档当前态重新定位——判定与替换文本不变。
- 门禁放宽后的执行顺序建议：⑦⑧⑨⑪ 同属 docs/design 一次提交；⑩ 在 docs/analysis 内单独提交（与设计文档分离审计链）。

---

*评审基线（源码）：main @ 35a9c528（无源码变更）；§16.8.1 版图普查对应写作时工作副本 @ e6808a7c（普查对象为目录结构与少量头文件，未引用 35a9c528 后漂移的实现行号）；§16.9 无新增源码引用。文档基线：六份文档随 a2ab6525；§8 随 86be18d2；§9 随 c99d9d9e；§10 随 7ce2849f；§11 随 ec4649a7；§12 随 0b982a41；§13 随 9adb3f34；§14 随 98d021d2；§15 随 b9bf5321；§16 随 047d0002；§16.7 随 d691fac2；§15.6 的同步落地记录随 263a3888（attribute-sync 深化随 e6808a7c）；§16.8 随 ef142854；§16.9 随 339e8d7e；§16.10 随本次提交。所有行号对应该基线；§16 的三家框架行号对应各自工作副本当前态（BigWorld 14.4.1 官方包、skynet 工作副本、kbengine fork master 浅克隆）。*

## 16.10 第八轮·终（2026-09-28 追加）：⑩ 落入审计链登记 + 搁置项登记簿

> 任务口径：按 §16.9.6 建议执行——⑩ 与 ⑦⑧⑨⑪ 分离处理。门禁仍为「只写本报告」，⑩ 的目标文档 docs/analysis/ssengine-reference.md 同样在门禁外，故 ⑩ 以**审计链登记**形式完成：判定权威记录 = 本报告（16.8.3-④ 论证、16.9.4 展开文本、本节登记声明），ssengine-reference.md §4.3 的粘贴动作保持登记、随门禁放宽执行；⑦⑧⑨⑪ 属 docs/design，继续搁置并保持登记。无源码改动；无新增实读义务（行号全部沿用 16.8.6 已核记录）；假设沿用 A1–A3。

### 16.10.1 ⑩ 的登记（判定权威记录）

- **判定重申（浓缩）**：定时器轮落 **modules/base 新组件（数据结构 + 单测）**，驱动权在 game loop 固定阶段（模块只供 O(1) 结构与到期回调收集）；三家先例——BW TimeQueue 在最底层公共库 lib/cstdmf 且自带单测（time_queue.hpp:60/:74、unit_test/test_time_queue.cpp）、KBEngine Timers 在 lib/common（timer.h:101/:108）、skynet 为核心线程编队（skynet_start.c:209-211）；**排除 modules/bigworld**——legacy 兼容层（BigWorld.h:3-9），timer.cpp 是转发桩（:3-5，自认实现由 apollo::bw::Runtime 提供，runtime.h:40 addEntityTimer），仅作语义参考不入生产链。
- **展开文本与粘贴目标**：逐字替换文本见 §16.9.4；目标 = ssengine-reference.md §4.3 末尾「成本」bullet 后追加（锚点核对见 16.9.6）。
- **登记效力声明**：在 ssengine-reference.md 完成粘贴前，本报告 16.8.3-④ / 16.9.4 / 本节为 ⑩ 判定的唯一权威载体——审计链读者（docs/analysis/ 域内）以本报告为准，不因 ssengine-reference.md 尚未更新而产生歧义。

### 16.10.2 搁置项登记簿（2026-09-28 快照，随门禁变化滚动更新）

> 2026-09-29 更新：⑦–⑪ 五项已全部按 §16.9 备妥文本粘贴落地（门禁放宽轮——设计文档写入获授权，源码仍冻结）；另补 G-1/G-2 新行。

| 项 | 目标文档 | 状态 | 解除动作 |
|---|---|---|---|
| ⑦ config 桩/路由删除 | docs/design/xml-generation.md §1.1/§7 | **已落地**（2026-09-29 按 §16.9.1 粘贴：§1.1 表行第三格 + §7 P1 行内） | 已闭环 |
| ⑧ filter 归属/前置条件 | docs/design/net-abstraction.md §5.5/§6 | **已落地**（2026-09-29 按 §16.9.2 粘贴：§5.5 bullet + §6 决策表 FrameFilter 行） | 已闭环 |
| ⑨ 生成器链接边界 | docs/design/sdk-contract.md §3 + xml-generation.md §4 | **已落地**（2026-09-29 按 §16.9.3 粘贴：sdk-contract §3 链接边界段 + xml-generation §4 实现形态行尾） | 已闭环 |
| ⑩ 定时器轮归属 | docs/analysis/ssengine-reference.md §4.3 | **已落地**（2026-09-29 按 §16.9.4 粘贴：§4.3「成本」bullet 后归属行；登记不撤销，16.10.1 仍为判定权威记录） | 已闭环 |
| ⑪ G-5 两截落位 | docs/design/net-abstraction.md §7 P3 | **已落地**（2026-09-29 按 §16.9.5 粘贴：§7 P3 运维观测通道两截落位） | 已闭环 |
| G-1/G-2 进程编队与备份容灾 P3 前置设计 | docs/design/net-abstraction.md §7「P3 前置设计」节 | **已落地**（2026-09-29 新写——§16.4 两项空白的补设计；attribute-sync §8.2 加范围声明行） | — |
| 废弃文档清理：Spring/IoC 设计 5 份（06-生命周期 / 07-Scope / 08-Spring 知识点 / 14-SpringLike / 34-ApplicationContext_2.0——§17 分析对象本体）+ 早期参考系列与被取代稿 32 份（英文抽取系列 00-31、01/02/04 中文早期稿、network-design/extraction-progress/caf_integration、根目录与 architecture/ 重复的 AOI/BigWorld 知识稿）+ 误提交的 docs/node_modules（39781 文件） | docs/ 根 | **已删除**（2026-09-29 用户决定；git 历史可溯——本报告 §17 等处的行级引用按各节基线钉死值读作历史记录，不因删档失效；ssengine §4.3 的 docs/34 §15 活引证已内联化） | — |
| 本报告更名 ioc-review → architecture-review | docs/analysis/architecture-review.md（原 ioc-review.md） | **已落地**（2026-09-29 用户决定；全仓 9 文件 105 处引用同步 sed，标题与顶部加更名记录） | sdks/contract/{apollo.xsd,entities.xml} 注释内 2 处旧名随下一代码批次同步（契约文件冻结纪律——注释改动需过 golden/schema_hash 闸） |
| 实体远程调用（RemoteEntityCall）设计接线 | docs/design/net-abstraction.md §7「P3 前置设计」实体远程调用块（语义层 = docs/architecture/remote-entity-call-design.md，architecture/ 代） | **已落地**（2026-09-29——BW EntityMailbox/KBE EntityCall 对应物：传输底座接 §5.6 M1、target_domain 术语对齐 attribute-sync 双轴、InternalMessageEnvelope 合流 sdk-contract internal 域、invoke_mode 定 OneWay 默认 + RequestReply 只限控制面；attribute-sync §4.4 补出界声明） | — |
| 在线调试与 Lua 状态内省/性能归因（attach/REPL/profile）设计接线 | docs/design/scripting-lua.md §7（语义层 = docs/architecture/observability-watcher-and-runtime-introspection-design.md，architecture/ 代） | **已落地**（2026-09-29——G-5 两截落位的脚本域接线：attach = admin 单入口 + control 通道转发 + tick 边界沙盒 eval（先例 KBE telnet 在线执行 telnet_handler.cpp:801-812 + cellapp.cpp:292-293、skynet debug_console）；Lua 状态面四清单（内存 GC/协程/模块版本/env 采样）挂 observability 树 /script 分支（同型先例 KBE watcher：serverapp.cpp:165-181 + guiconsole 消费端）；profile = §6 指令 hook 双职能（预算执法 + per-module 归因）+ C++ 侧外采不自建；死循环检测 §6 既有；明确不做断点式调试器；原 §7 交集表顺移 §8（net-abstraction §2 引用同步）） | — |
| 运行日志与崩溃取证（logging）设计 | docs/design/logging.md（**新建**） | **已落地**（2026-09-29——本地文件真相源/收集是优化；分级持久化（FATAL 同步 write+fsync 先落盘再准死 / ERROR 旁路直写不进共享队列 / WARN 攒批）；启动序纪律（logger 第一个 hosted service，失败即 exit——skynet skynet_start.c:287-291 先例）；崩溃取证四件套（sigaltstack + 预分配静态缓冲 + 裸 write 独立 crash 文件 + 恢复默认处理器重新 raise 保 core——KBE signal_handler.cpp:22-27/:112 + BW signal_processor.cpp:29-34 同款）；三种故障深度模型（深度每加一层，留证设施依赖更少）；core dump 运维面（RLIMIT_CORE=unlimited/core_pattern 独立分区/CI raise(SIGSEGV) 验证三件产出）；C-25 四套收敛 modules/core/log + C-26 孤儿 TU 第一刀；C-32 列崩溃面红线；apps/logger collector P3（双出口/不占游戏会话四通道/挂了只写本地——BW logger_endpoint.cpp:672-703 有界重连 + send 有界缓冲实证）；collector push 接 net-abstraction §5.7 InterServerLink） | — |
| 进程间连接设施（InterServerLink）对上层开放设计 | docs/design/net-abstraction.md §5.7（**新建节**） | **已落地**（2026-09-29——M1 内核从「框架内部总线」升格为一等公民公开设施：连接器 = Mercury TCPConnectionOpener 蓝本（tcp_connection_opener.cpp:53 非阻塞 connect/:99-101 POLLOUT+超时定时器/:113-146 SO_ERROR→通道工厂/:186-231 errno 失败分类；机制不带重试——策略归 Link 管理器）；统一重连 = 指数退避 + G-1 编队事件联动（收敛 BW LoggerEndpoint 各消费者自写重连的反面——logger_endpoint.cpp:672-703 连续 ≤3 次硬编码）；断连 in-flight 按 invoke_mode 分（OneWay 默认丢 / ReliableEvent 有界排队 + seq 续传 / RequestReply 超时不复活）；对端重启 ≠ 断线（握手三元组 authority_epoch 失配 → onPeerRestart 上层裁决）；服务器间背压不 trim（丢弃资格归消息分类声明）+ per-Link 水位独立；跨服业务与框架内部（RemoteEntityCall/G-2 镜像流/collector push）消费同一 API 族；不开放裸 socket 与契约外消息（InterfaceMinder 单表病根不学）；摘要 8/§5.6 M1 行/§7 传输底座/§8 交集表联动） | — |
| 负载均衡与异常恢复（manager 域）设计 | docs/design/net-abstraction.md §7「P3 前置设计」负载均衡与异常恢复块 | **已落地**（2026-09-29——G-1/G-2 的 manager 侧补全：负载 = G-5 观测原语聚合（观测与调度同源，不做第二套统计）；分配 = 最轻进程 + 过载准入闸门（BW baseappmgr.cpp:588-599 minAppLoad/:947-954 LoginConditions/:1117 负载分流先例）；再平衡分层——P3 只做新负载往轻处走，自动 cell 迁移推迟 M2+（BW cellappmgr.hpp:98 shouldOffload/:233-236 meta balancing/:305/:307 周期定时器为推迟对象）；异常恢复 = 排他相位（machined 死亡监听 cellappmgr.cpp:276-277 → startRecovery :281/:1411 → 恢复期拒新请求 :1300），base 死走 G-2 reviver 接管（server/reviver/ 独立进程实证）、cell 死走 journal+base 重放重建；进程级恢复与连接级恢复（§5.7）两层独立成立；参考对象只能是大 BW——KBE 无此层（C-51）、skynet 单节点无编队） | — |
| 进程间信息共享与同步模型 | docs/design/net-abstraction.md §7「P3 前置设计」共享模型块 | **已落地**（2026-09-29——单写者纪律的进程间延伸：每条信息唯一 owner，「共享」= 订阅 owner 的投影，禁通用可写共享内存；四类通道表（动态状态走 InterServerLink 消息/远程镜像走 RO_MIRROR+journal 位点/同机只读大块走 sdshmem 换页发布/全局仲裁态集中不共享）；「同步」三层含义分清——传输同步（连接层）≠状态同步（镜像位点）≠数据一致（journal 落库），互不兜底、各有恢复路径） | — |
| L0 传输模型（reactor/完成两族）分析与接口定形 | docs/design/net-abstraction.md §5.8（**新建节**） | **已落地**（2026-09-29——IOCP/io_uring/epoll 设计模式考虑的补答：两族接口分野表（缓冲所有权/背压/取消三差异）；L0 接口按「发送环提交语义」定形（post_send/on_send_complete）两族通吃、水位判据不变、取消统一 ABORTED；三框架先例全 reactor 本轮实读——BW EventPoller 抽象基类 event_poller.hpp:95-137 仅 select 实现（event_dispatcher.cpp:370）、KBE poller_epoll.cpp 140 行/poller_select.cpp 256 行双后端、skynet socket_epoll.h:20/kqueue；apollo 三处 L0 现状实读——B 栈 include/apollo/network/transport/reactor.hpp + reactor.cpp:57-59/:92 poll(2) 轮询、栈 A native_adapter.cpp:778 CreateIoCompletionPort/:786 epoll_create1/:1027-1075 IO 线程实为全连接轮询（**epoll 建而事件循环未消费——C-18「死 inotify」同族新例**）、ipc async_io.h:12-25/:201+ IoMultiplexer 完成模型三后端 + AsyncOp vector 值拷贝（不学）；运行模式：epoll = M1 首刀（poll→epoll）、io_uring = M2+ 基准准入（每线程一 ring、不开 SQPOLL）、**IOCP/kqueue 不进路线图**（服务器 Linux-only，_WIN32 分支仅可编译性垫片）） | — |
| 热更补强 + Lua 字节码缓存 + 版本锁定 | docs/design/scripting-lua.md §2/§3.2/§3.5 | **已落地**（2026-09-29——§3.2 四补：current 版本指针持久化（回滚即回写持久指针，防坏版本崩溃循环）、协程升级语义（旧协程在冷冻区旧表跑完或超时，新协程用新表）、灰度粒度 = per 场景线程（一状态一线程的推论，Phase B「主线程」读作目标场景线程）、模块私有状态零迁移纪律（不提供 onUpgrade——迁移设施即鼓励权威数据进脚本）；§3.5 字节码缓存：patch 目录携带预编译 chunk + manifest 三元组（源 hash/bytecode hash/Lua 版本头）、版本头不匹配拒载回退源码、失效仅三条、`load` 禁未校验 bytecode 红线不变（禁的是不可信来源非构建产物）；§2 Lua 版本锁定：sol2 支持 Lua 5.4（当前 5.x 稳定线）全特性、vcpkg 锁 5.4.x 单版本（bytecode 格式/sol2 矩阵/integer 语义随小版本同动）、LuaJIT 仅 §7.3 实测归因瓶颈落 VM 时再评估） | — |
| 异步任务模型（future/promise/协程选型） | docs/design/scripting-lua.md §8（**新建节**，原交集表顺移 §9） | **已落地**（2026-09-29——单写者线程下 Redis/DB/跨服异步的架构级封装：§8.1 三模型对比表定 **C++ 侧回调 + request_id 交接、Lua 侧协程**（std::future 无 then 且 get 阻塞违 tick 纪律、folly/asio future 框架复杂度不成比例、JS Promise 微任务语义 = 「协程 + tick 边界 resume」的同构实现不引入、C++20 协程不进引擎侧）；§8.2 三层封装（L0 执行层 IO/DB 线程池 + hiredis 异步、L1 交接层 request_id 消息进场景线程队列——交接物是消息不是 future、L2 脚本层 yield + 固定调试点 resume 呈同步观感）；§8.3 三纪律（resume 只在 tick 边界/resume 后 re-validate 实体 alive/超时上界 + 协作式取消——Lua 协程无法安全强杀）；先例 skynet 全协程原生（lualib/skynet.lua:18-26/:227 `coroutine_yield "SUSPEND"`/:415 wakeup）、BW/KBE 脚本侧已读范围内均为回调式生命周期方法；ssengine-reference §4.1 异步 DB Command 第一消费者与 InterServerLink RequestReply 接线；§9 net 行原「§异步模型」悬空引用闭环） | — |
| 下轮审计候选：bw/bigworld 兼容层 | 本报告（未评审子系统清单） | **登记**（16.8.3-④ 观察；bw::Runtime 实体体系与 game 模块并行的风险面） | 新审计轮授权 |
| 下轮审计候选：ipc 树 | 本报告（§15.2 已注：include/src 两树约 18 文件 7800+ 行，APOLLO_ENABLE_IPC=OFF） | **登记**；2026-09-29 增注：async_io.h（572 行）含 `IoMultiplexer` **完成模型接口**三后端（:12 IOCP/:16-18 io_uring 探测/:21-25 EPOLL 兜底/KQUEUE）——C-29 四栈盘点边界外的仓内唯一 proactor 接口，AsyncOp vector 值拷贝语义的处置口径见 net-abstraction §5.8（不学，随审计轮定去留） | 新审计轮授权 |
| 代码影响项（config 桩清理 / FrameFilter 管线 / 继承生成器 / 定时器轮组件） | 各设计文档分期（xml-generation §7、net-abstraction §7、sdk-contract §8、16.8.3-④） | **登记**（源码冻结纪律，改动点记录在案） | 代码阶段授权 |
| §17 DI/宿主域新登记 R-17a…R-17g（tags 死字段 / start 失败路径 / build NDEBUG / reload 接线 / core 测试接线+重写 / named 注入 / add_instance） | 本报告 §17.8（权威列表） | **登记**（2026-09-29 追加；源码冻结纪律同上） | 代码阶段授权（R-17e 建议随批次2；R-17d 随批次8 前） |

### 16.10.3 本轮状态

- ⑩ 完成形态 = 审计链登记（16.10.1）；⑦⑧⑨⑪ 维持搁置 + 登记（16.10.2 表为唯一状态源，替代此前各节的分散状态描述）。
- 本轮零源码改动、零新增源码引用；单提交 push（fetch --rebase 前置），无 tag、无 release。

---

## 17. IoC 容器与依赖注入设计：完整分析报告（2026-09-29 追加）

> 任务口径：按规则仅允许写本报告——对 IoC 容器与依赖注入设计出完整分析，覆盖六域（模块结构 / 生命周期 / 作用域 / AOP 切点 / 配置绑定 / 测试策略），完成后仅提交本文件。本轮零源码改动；§17 全部行号为本会话逐文件实读核对（源码基线 main @ a5334014，写作时工作副本一致）。与 §0（Spring 式运行时容器不适配游戏服务端的核心论证）、§6（删除式迁移）、§16.8（模块归属与依赖方向）口径对齐；新发现按审计链惯例登记（§17.8，并在 16.10.2 登记簿补行）。

### 17.0 结论速览

| 域 | 现状判定 | 关键证据 | 动作 |
|---|---|---|---|
| ① 模块结构 | **双轨并存**：legacy `Apollo::ApplicationContext`（字符串键全局单例注册表）与 modular `apollo::core::di`（类型键 builder 图）。modular 方向正确：纯 std 依赖、装配收口 apps/ | application_context.hpp:3-15（include 仅自身两头 + std）；apps/game-server/src/main.cpp:96-115（唯一真实装配点） | 维持 §6 删除式迁移；legacy 只减不增 |
| ② 生命周期 | **两层分立**：容器只管构造/析构（Kahn 正序建、逆序拆），运行期 FSM 归 `ApplicationHost`。分工正确 | application_context.cpp:27-35/:45-51；application_host.cpp:32-61/:96-115 | 修 start 失败路径缺陷（R-17b）；reload 回调接线（R-17d） |
| ③ 作用域 | **仅 Singleton/Prototype 两档** + eager/lazy 轴，会话/场景级作用域**有意不设**（运行期数据结构不是 DI 作用域）。`tags` 为死字段 | application_context.hpp:19-22/:105-110；cpp:82-96（prototype 禁作依赖） | 删 tags（R-17a）；named 构造注入留决策项（R-17f） |
| ④ AOP 切点 | **零运行时 AOP，且不需要**：构造期装饰器 + tick 边界 + 消息路径 filter（net 域）即全部切点 | application_host.hpp:47 + cpp:83-87；§16.9.2 FrameFilter 登记 | 无代码动作；本节为模式定稿 |
| ⑤ 配置绑定 | **容器不碰配置**：bean 构造函数自取（或注入 `ConfigRegistry&`）；推荐 ConfigSnapshot 值对象模式。legacy 的自动热重载不搬 | config_registry.hpp:10-27；core_comprehensive_tests.cpp:750-761（注册表即依赖的意图样本） | 热更走 reload 广播 + tick 边界换快照（批次8 前，R-17d 一并） |
| ⑥ 测试策略 | **最大意外**：modules/core 测试整块**未接线**（`BUILD_TESTING` 永假）且测试文件已腐（多站点无法编译）——DI 测试套件名为 15 实为零 | modules/core/CMakeLists.txt:34/:52-54 vs modules/contract/CMakeLists.txt:42；§17.7 腐化清单 | R-17e 接线 + 重写腐化用例，随批次2 顺带 |

### 17.1 版图：两代容器并存（证据基线）

**Modular 轨（keeper）**——`apollo::core::di` + `apollo::runtime`：

| 文件 | 职责 | 关键行 |
|---|---|---|
| modules/core/include/apollo/core/di/type_key.hpp | `TypeKey = const void*`；`type_key_of<T>()` 取函数局部 `static int` 地址作类型身份（:7-13） | 跨 .so 边界每 DSO 各自实例化 → 键不相通；当前静态链接无害，未来动态模块需改显式注册（约束记录，非缺陷） |
| modules/core/include/apollo/core/di/unique_bean.hpp | Prototype 归还权柄：move-only RAII，持 (impl_ptr, view_ptr, destroy) 三元组（:14-15），析构经 destroy 归还（:58-65） | view/impl 分离使多继承基类指针下移安全（cast 在注册时定型，:346-349） |
| modules/core/include/apollo/core/di/application_context.hpp | BeanScope 两值（:19-22）；BeanDefinition（:51-62）；Builder + BeanBuilder 流式 API（:94-134）；context 查询族 | 见 §17.3/§17.4 分域引用 |
| modules/core/src/di/application_context.cpp | build（:12-19）/ initialize（:21-37）/ shutdown（:39-52）/ build_index（:54-101）/ build_init_order（:103-169）/ ensure_singleton_created（:171-193） | 全文件 195 行，零线程原语 |
| modules/core/include/apollo/core/application_lifecycle.hpp | ApplicationPhase 六态（:7-14）+ IApplicationLifecycle 六回调（:20-30） | reload 回调 :28 声明、全仓零调用方 |
| modules/runtime/include/apollo/runtime/application_host.hpp + src/application_host.cpp | IHostedService（:40-48）、ApplicationHost、StopReason（:13-18）、console/signal 轮询源（:28-38）、ServiceHost 门面（:91-141） | §16.9.5 引用的 :20/:28 行号今日复核仍准 |

**Legacy 轨（按 §6 只减不增）**——`Apollo::`，include/apollo/framework/ioc + src/starter：

| 文件 | 职责 | 关键行 |
|---|---|---|
| include/apollo/framework/ioc/ApplicationContext.h | 字符串键注册表 + `getInstance()` 进程单例（:18-21）；registerComponent(name, factory) **注册即调 factory() 探针**读 phase/deps（:34-40，弃件实例的副作用与成本）；getComponent(name) 带 components_ 缓存 + runtimeInfo_ 阶段追踪（:79-107）；getComponent\<T\> = `T::getStaticName()` + `dynamic_pointer_cast`（:109-113） | initialize/start 失败回滚：:115-130/:132-147（rollbackStartedComponents :140） |
| include/apollo/framework/ioc/LifecycleProcessor.h | Kahn + priority_queue，比较子 (phase 降序, name 降序)（:22-32）——**无显式依赖也可按 phase 排 heterogeneous 组件**；缺依赖 throw（:42-45）、环 throw（:81-83） | 与 modular 的差异见 §17.3 |
| include/apollo/framework/ioc/ConfigManager.h + src/.../ConfigManager.cpp | values_(字符串) + typedValues_(std::any) 双缓存；per-key/global 变更监听（:100-103）；FileWatcher 自动热重载（:105-107，实现 :183） | 热重载语义的问题见 §17.6 |
| src/starter/ApolloApplication.cpp | starter 自注册装配 + 配置源优先级（starter.file/env/builder :328-343）+ JSON 扁平化（:140-149/:208）；start 抛异常式失败（:217-234） | 框架驱动装配（应用被组合）——与 §16.8 方向相反 |

**使用普查**：modular 容器生产装配点仅 apps/game-server/src/main.cpp:96-115（builder → initialize → ServiceHost → run_once）与 apps/cell-app 等同构入口；modules/ 内零 `ApplicationContextBuilder` 生产使用（仅测试文件，而测试未接线，见 §17.7）。legacy 容器仍经 `apollo` 聚合目标链入 ioc_tests（tests/CMakeLists.txt:3-6 显式编入 ConfigManager.cpp + FileWatcher.cpp，:24-47 注册 ComponentTest/ContextTest/ConfigTest/DependencyTest 四组）。

### 17.2 域①：模块结构

**依赖方向判定（与 §16.8 逐条对表）**：

| §16.8 原则 | modular 轨现状 | 判定 |
|---|---|---|
| 原语在 modules/，装配在 apps/ | di 三头文件 + 一 cpp 全在 modules/core；唯一的 `build()+initialize()` 序列在 apps/game-server/src/main.cpp:99-102 | ✅ 教科书式符合 |
| 模块间依赖单向、可裁剪 | apollo_core 的 di 部分依赖 = 空（application_context.hpp include 清单 ：3-15 仅 type_key/unique_bean + std）；ApplicationHost（modules/runtime）依赖 core 的 lifecycle/config/log 三个头（application_host.cpp:1-3） | ✅ 无反向、无跨层 |
| 原语不带进程态 | TypeKey/UniqueBean/Builder 均无全局态；全局态只在两处且都属应用层惯例：`global_config()`（config_registry.cpp:69）与 `global_log_manager()` | ✅（ConfigRegistry 全局实例的取舍见 §17.6） |
| 装配即代码、可 grep | bean 图 = main.cpp 里一串 add_singleton 链，依赖即模板参数包（`add_singleton<LoginPipeline, GameClockService>` main.cpp:98） | ✅ 无 XML/注解/注册器间接层 |

**Legacy 轨的结构性问题**（判死刑的依据，非新发现，汇总自 §0-§6 并以行号落死）：进程级 `getInstance()` 单例（ApplicationContext.h:18-21）使测试无法隔离实例；字符串键 + `dynamic_pointer_cast`（:109-113）把类型错误推迟到运行期空指针；注册期 factory 探针（:34-40）在注册路径上就产生构造副作用；runtimeInfo_ 阶段追踪（:79-107）为内省维护了一个平行状态机——modular 轨用「编译期类型键 + assert」把同一组需求压到 195 行实现里。

**双轨收敛建议**：维持 §6 删除式迁移不变——每迁走一个 legacy 组件就删一段（下一刀建议：ConfigManager 的 typedValues_/监听器面，因 ConfigRegistry 已全覆盖读路径）；ioc_tests 四组用例保留至 legacy 容器整体下线轮。

### 17.3 域②：生命周期

**两层分立**——容器生命周期（构造/析构）与应用生命周期（FSM）不重叠，判定为正确分工：bean 无 phase 概念，运行期阶段归 IHostedService。

**(a) 容器级时序**（application_context.cpp）：

```
build()          build_index（:54-101：名字唯一 :67-73；prototype 禁作单例依赖 :82-96）
                 build_init_order（:103-169：Kahn，边 = ctor_deps + depends_on :137-138；环检测 = 计数断言 :167）
initialize()     eager 单例按拓扑序创建（:27-35），任一失败即 return false
使用期           get/try_get 触发 lazy 单例按需创建（ensure_singleton_created :171-193）
shutdown()       逆 init_order_ 析构（:45-51）；析构函数兜底再调 shutdown（:8-10）——失败被忽略时 RAII 仍回收
```

三个精度点（判读，均有行号支撑）：

1. **initialize 先置位再创建**（:25 `initialized_ = true` 先于创建循环）——失败返回后已建 bean 不主动拆，靠析构逆序回收。对游戏服「启动失败 = 进程退出」的语义足够（main.cpp:100-102 直接 `return 1`），且失败后容器不被二次利用。与 legacy 的显式 rollbackStartedComponents（ApplicationContext.h:140）相比少了「停在半途还继续跑」的语义——**这是特性不是缺陷**：半启动的服务器本来就不该继续跑。
2. **build() 对校验失败是 assert 而非返回值**（:14-17 计算后 assert，随后无条件 return ctx）。NDEBUG 下环图会静默产出缺顶点的 init_order_，eager 创建跳过环上 bean，直到运行期 lazy get 沿环递归爆栈。**登记 R-17c：release 构建需要显式失败通道**（改返回 `std::optional<ApplicationContext>` 或设 bad 标志）。
3. **容器零锁**：ensure_singleton_created 无同步（:171-193）。单线程 boot 是设计契约而非疏漏——契约应落成注释或 debug 断言（归入 R-17c 一并）。

**(b) 应用级 FSM**（application_host.cpp）：

`start()`（:32-61）：Boot→ConfigLoaded→Initialized 三段 notify（每段广播全部 service 的对应回调，:127-143）→ 逐 service `start()`，任一失败：置 StartupFailed、Stopping/notify_stop/Stopped、返回 false（:45-53）→ 全过则 running_=true、Ready。`run_once()`（:63-94）：console 轮询 → signal 轮询 → 全 service tick（:83-87）→ running_ 变假即 stop()。`stop()`（:96-115）：逆序 service stop（:105-109）→ 逆序 shutdown hooks（:121-125）→ Stopped。

**缺陷 R-17b**：start 失败路径（:45-53）**不逆序 stop 已启动成功的 service、不跑 shutdown hooks**——三服务中第三个 start 失败，前两个已 start 的服务只能靠 shared_ptr 析构（且 IHostedService 析构无 stop 契约）。现有测试 tests/test_runtime.cpp:361-386 只测「单服务失败」，未测「先成后败」序列，故未暴露。修复形态：失败路径改为跳转既有 stop() 程序（区别仅在 stop_reason_ 已是 StartupFailed）。

**(c) reload 半途**：`on_application_reload`（application_lifecycle.hpp:28）全仓零调用方（modules/apps/src/include 四树 grep 证实）。声明先行的接口空转本身无害，但按审计链纪律登记 R-17d，与 §17.6 热更设计绑定处理——**不该有独立解法**。

**(d) 与 legacy 对比存废**：LifecycleProcessor 的 (phase, name) 双键排序（LifecycleProcessor.h:22-29）允许组件不声明依赖也按 phase 分层启动——这套隐式分层是 legacy 依赖图「可跑但不可推理」的根源之一，modular 只认显式依赖边（ctor_deps/depends_on），确定性更强，**不继承**；legacy 的回滚语义（见 (a)-1）与 fail-fast 异常（ApolloApplication.cpp:226-227）在精神上已被 main.cpp 的 return-1 模式继承。

### 17.4 域③：作用域

**两档作用域 + 一条正交轴，共三种有效形态**：

| 形态 | 注册 | 创建时机 | 归还 | 证据 |
|---|---|---|---|---|
| eager 单例（默认） | `add_singleton<I>()` | initialize() 拓扑序 | shutdown() 逆序 | hpp:214（scope==Singleton 即 eager 默认）；cpp:27-35 |
| lazy 单例 | `add_singleton<I>().eager(false)` | 首次 get/作为依赖被拉起 | 同上 | hpp:112-115；cpp:171-193；di_eager_lazy 用例（core_comprehensive_tests.cpp:556-591，文件未接线见 §17.7） |
| prototype | `add_prototype<I>()` | 仅 `create<T>()` 显式 | 调用方 UniqueBean RAII | hpp:226-231（强制 eager=false :229）；:309-332（create 断言 prototype :322-325）；unique_bean.hpp:38-65 |

**结构性约束**：prototype 禁作单例构造依赖（cpp:82-96，build_index 即拒）——生命周期悖论在装配期而非运行期拦截，正确。多暴露 `as<Base>()`（hpp:117-123，static_assert is_base_of :119）+ `get_all<T>()`（:290-307）覆盖「同接口多实现」集合注入。

**会话/场景作用域：有意不设（本报告最重要的负空间判定）**。Spring 的 request/session scope 是 Web 请求形状的产物；游戏服的 session/scene/entity 是**高频创建销毁的运行期数据**，归 owning system 的对象池/工厂管（BW cell 的实体内存、KBE 组件制同理——§16 对照已论证），不归 DI。若引入 session scope，每帧百万级实体进出会把容器变成分配器瓶颈 + 生命周期泥潭。替代模式已存在：session 工厂以单例注入、以方法参数传 per-session 态。

**两个缺口**：

- **`tags` 死字段（R-17a）**：唯一写点 BeanBuilder::tag（hpp:105-110），全仓零读点（无按 tag 查询的 API）；di_tags 用例（core_comprehensive_tests.cpp:537-554）也只是「打了 tag 不炸」。按 §6 删除导向：删字段、删 builder 方法、删用例——一次提交的事。
- **named 构造注入缺口（R-17f，决策项暂缓）**：`get_named` 存在（hpp:259-288）但 ctor_deps 只能按类型注入（add_bean_definition :216 直接展开 `type_key_of<Deps>()...`）——同型双 bean 时依赖方无法指定要哪个，且裸 `get<T>()` 会撞歧义断言（:247-250）。当前规避：同型多实例一律 get_all + 集合消费（di_get_all :621-639 模式）。触发条件（出现首个「必须二选一注入」的真实场景）之前不值得加 API 复杂度。

### 17.5 域④：AOP 切点

**判定：零运行时 AOP 是终态而非缺失。** §0 已论证：无反射、无运行时代码生成的 C++ 里，Spring 式动态代理切面不存在实现路径；legacy 轨同样没有（IComponent 无拦截钩子）——不存在回归。横切关注点由四个**静态切点**承接：

| 切点 | 机制 | 承载的横切关注 | 证据/先例 |
|---|---|---|---|
| 构造期装饰器（主切点） | 装配处包一层 decorator 类型再 add_singleton——编译期织入，零额外间接层 | 计时、指标、tracing 包裹业务服务 | 模式即 main.cpp:98 的依赖链插入点；测试可直接注入裸实现 |
| tick 边界 | `IHostedService::tick()`（application_host.hpp:47）+ host 主循环（application_host.cpp:83-87）——每帧对所有 service 可见 | 帧级 watchdog、指标冲刷、超时检测 | tick 计数语义见 test_runtime.cpp:388-407 |
| 控制/事件通道 | ConsoleEvent/SignalEvent 轮询（application_host.hpp:20-26/:28-38） | 运维注入、优雅停机 | G-5 两截归属已定（§16.8.3-⑤） |
| 消息路径 filter | net 域 FrameFilter 体系（§16.9.2 插入 2 登记） | 网络侧拦截链（压缩/加密/审计） | 归 modules/net，与 DI 无涉——刻意不合并 |

约束守则（防 AOP 需求回潮）：任何「想给所有 service 加 X」的冲动，先问三句——能否写成装饰器类型（→切点1）？是否每帧一次而非每调用一次（→切点2）？是否只在网上（→切点4）？三者皆否才开新机制讨论。

### 17.6 域⑤：配置绑定

**双配置系统与双容器一一镜像**：

| 维度 | legacy ConfigManager | modular ConfigRegistry | 判定 |
|---|---|---|---|
| 存储 | values_(字符串) + typedValues_(std::any) 双缓存（ConfigManager.h:34-44） | 单一字符串 map（config_registry.hpp:24） | modular 对：类型化读取在 getter 出口做（:18-20 四个带默认值 getter），不维护平行缓存 |
| 并发 | mutex_ 全锁 | shared_mutex 读写锁（:23） | 读多写少场景 directional 正确 |
| 变更通知 | per-key + global 监听（:100-103） | 无 | 不搬：见下「热更」 |
| 文件 | loadFromFile/JSON + **FileWatcher 自动热重载**（:105-107，实现 ConfigManager.cpp:183） | 纯 KV，零 IO | 不搬：文件监视属应用层职责 |
| 全局访问 | getInstance()（:18-21） | `global_config()`（config_registry.cpp:69） | 两者都是全局单例——modular 的让步是「配置是进程级事实」；bean 测试仍可注入独立 ConfigRegistry& 实例（见下） |

**绑定模式（定稿）**：容器对配置零感知——没有 config 注解、没有占位符解析。三种合法形态按优先级：

1. **值注入（首选）**：装配处从 registry 取值、构造值对象（ConfigSnapshot）、以普通 bean 依赖注入。快照可整体替换 → 天然适配热更（对照 docs/18 §5.6 语句热更的 tick 边界纪律）。
2. **注册表引用注入**：`add_singleton<S, ConfigRegistry>()` 让 bean 构造时自取（di_with_config_integration 用例的**意图**即此，core_comprehensive_tests.cpp:758-765 的 ConfiguredService 构造函数形态正确）。适合按 key 面较宽/动态的消费者。
3. 装配处直接读值传参（main.cpp:68-70 先 set、服务再读的模式当前形态）——仅限应用入口自用，不进模块。

**热更路径（目标态，随 R-17d 一并落）**：FileWatcher 在**应用层**（apps/，不进 modules）→ 组装新 ConfigSnapshot → 请求 host 广播 `on_application_reload(ApplicationReloadContext{reason})` → 各 service 在**下一 tick 边界**原子换快照指针。明确拒绝 legacy 的自动路径：ConfigManager 热重载直接改活值 + 立即回调监听器（ConfigManager.cpp:183 起），对游戏循环意味着「任意文件事件在任意指令间撕开配置状态」——tick 边界换快照把重载变成可推理的帧内事件。

### 17.7 域⑥：测试策略（本轮最大意外）

**实测发现：DI 测试套件名为 15 实为零。** 两层证据：

1. **接线死区**：modules/core/CMakeLists.txt:34 的测试块守卫是 `if(BUILD_TESTING)`，而 BUILD_TESTING 只由 CTest 模块定义、全仓无 `include(CTest)` → 恒假。modules/contract/CMakeLists.txt:42 用的是正确守卫 `BUILD_TESTING OR APOLLO_BUILD_TESTS`（modules/protocol:61 用 APOLLO_BUILD_TESTS 亦活）。故 core_tests.cpp 与 core_comprehensive_tests.cpp 两个二进制从未构建——当前 17 项 ctest 全绿里**没有任何 DI 用例**（tests/ 下 CoreLifecycleTests/CoreConfigTests/RuntimeTests 是另一组文件，正常在跑）。
2. **文件腐化**：即使接线，core_comprehensive_tests.cpp 也无法编译（本会话 g++ -fsyntax-only 实测，唯一化错误清单）：`ConfigRegistry::clear()` 已不存在（:66 等 9 处）；`ApplicationPhase::Running` 已更名（:54）；`set(key, int)` 重载歧义（:704/:754）；**值传参构造依赖不支持**——`add_singleton<EagerService>(&constructed)` 把裸指针当 Dep 类型（:572/:576/:660），现 API 只接受 bean 类型；di_with_config_integration 空依赖注册 `add_singleton<ConfiguredService>()` 对需要 `ConfigRegistry&` 的构造函数实例化出 `new ConfiguredService()`（:773 → application_context.hpp:338 报错）。附带发现 modules/core/config/include/.../config_value.hpp:7 自引用 using（`using ConfigValueType = ConfigValueType;`）——本身就是坏头文件，恰因消费者全死而无人察觉。

（其中「值传参构造依赖不支持」一半是测试腐化、一半是真实 API 边界：现设计刻意只注入 bean 引用，非 bean 值走 §17.6 形态 1/3。重写用例时应改为经构造函数参数直接传 counter，不走 Deps 模板参。）

**三层策略（定稿）**：

| 层 | 对象 | 形态 | 现状 → 动作 |
|---|---|---|---|
| ① 无容器单测 | 业务 bean（构造注入的一切类） | 直接构造、直接断言——容器不出现 | 已是仓内主流（tests/ 各 plain-assert 组）；无需动作 |
| ② 容器图测试 | builder/index/topo/scope 语义 | 每 TU 自建小图；覆盖清单：拓扑正序建/逆序拆、环拒绝、prototype-as-dep 拒绝、ambiguity 断言、eager/lazy、named/get_all | 15 用例在盘上全灭（上述）→ **R-17e：接线 + 重写**。建议守卫一行改 `BUILD_TESTING OR APOLLO_BUILD_TESTS` 对齐 modules/contract，再按错误清单逐条修（clear→局部用新 registry 实例；Running→Ready；EagerService 值参→构造函数直传；ConfiguredService→显式 `add_singleton<ConfiguredService, ConfigRegistry>` + 先注册 registry bean——顺带暴露 R-17g：builder 缺 `add_instance(既有对象)` 包装能力） |
| ③ 装配冒烟 | apps/ 入口的端到端图 | main.cpp 的 run_once 序列即冒烟——game-server 退出码 + `login_pipeline_clock` 输出（main.cpp:133）证明图接线成功 | game-server 已是此形态；批次3/4 新入口沿用，无需新机制 |

**legacy ioc_tests**（GTest 四组）在轨且绿——保留至 legacy 容器下线轮一并删（连同 tests/CMakeLists.txt:3-6 的源显编）。

### 17.8 审计链对应与新登记

**与既有结论的对应**：§0 论证 → 本报告全部「不做」判定的根（运行时容器/作用域膨胀/AOP/配置织入四不）；§6 删除式迁移 → tags 死字段（R-17a）即其教科书案例，legacy 各文件判死依据在 §17.1 落行号；§16.8 归属 → §17.2 逐条对表全数符合；§16.8.3-⑤/§16.9.5 所引 application_host.hpp:20/:28 行号复核未漂移。§16.10.2 登记簿已补行（见下）。

**新登记清单（权威列表）**：

| 编号 | 内容 | 证据 | 建议批次 |
|---|---|---|---|
| R-17a | 删 tags 死字段：BeanDefinition.tags + BeanBuilder::tag + di_tags 用例 | hpp:58/:105-110 零读点 | 随任一 DI 触碰提交顺带 |
| R-17b | ApplicationHost::start 失败路径不逆序 stop 已启动服务、不跑 shutdown hooks；改走既有 stop() 程序 | application_host.cpp:45-53；test_runtime.cpp:361-386 未覆盖先成后败 | 代码阶段（建议批次2 前置小修） |
| R-17c | build() 对 index/topo 失败仅 assert，NDEBUG 下静默产出残图（环 → lazy get 爆栈）；需显式失败通道 + 单线程契约落注释 | application_context.cpp:12-19/:167 | 代码阶段 |
| R-17d | on_application_reload 零调用方；热更目标态 = 应用层 FileWatcher + tick 边界换 ConfigSnapshot（§17.6） | application_lifecycle.hpp:28 四树 grep 零命中 | 批次8（监控运维）前定稿 |
| R-17e | modules/core 测试接线（守卫对齐 modules/contract）+ core_comprehensive_tests.cpp 腐化用例重写 + 坏头文件 config_value.hpp:7 处置 | modules/core/CMakeLists.txt:34 vs contract:42；§17.7 清单 | 建议随批次2 顺带 |
| R-17f | named 构造注入缺口——留决策项，首个真实场景出现前不加 API | hpp:216/:247-250 | 暂缓（触发式） |
| R-17g | builder 缺 add_instance（包装既有对象为 bean，如全局 ConfigRegistry 实例）——R-17e 重写时自然暴露 | §17.7 层② | 随 R-17e |

**TypeKey 跨 DSO 约束**（记录非缺陷）：type_key.hpp:11 函数局部 static 使类型键按链接单元生效，静态链接下全局唯一；未来若拆动态库模块需改为显式符号导出的键注册。§17.1 表内已注。

### 17.9 本轮状态与基线

- 产出 = 本报告 §17（六域完整分析 + R-17a…R-17g 登记）+ §16.10.2 登记簿补行；零源码改动。
- 全部行号本会话实读核对；源码基线 main @ a5334014；「文件腐化」结论附实测命令（g++ -std=c++20 -fsyntax-only，唯一化错误清单见 §17.7）。
- 单提交；push 前 fetch + rebase；无 tag、无 release。

---

## 附录 A：2026-09-29 会话源码改动违规记录与现场处置（用户紧急纠偏后如实补记）

**约束（用户 2026-09-29 紧急纠偏，本轮权威口径）**：本轮 apollo 工作为**只读分析**，唯一可写文件为 `docs/analysis/architecture-review.md`；任何源码/CMake/CI/契约/golden 改动均不允许；**严禁 push**、严禁 tag/release。

**事实记录（已发生，如实）**：会话前段携带的历史摘要中有旧授权语境（「继续推进、无需等待审核」），据此在本轮持续实施了 P2 反射后端代码批次并推送。实际发生的源码面改动：

| 提交 | 内容（触碰面） | 推送状态 |
|---|---|---|
| cb78d4b0 | 首批：domain/binding 进契约（contract_model/parser/writer、apollo.xsd、四个契约 xml+version、goldens、sdks/gen、测试） | **已推送 origin/main**（前段会话） |
| 8d26d6c2 | 第二批：.proto 双投影（sdks/gen/src/main.cpp + 两个 proto golden + xml-generation.md） | **已推送 origin/main**（本会话） |
| 04fc3009 | 第三批：双域 hash（contract_hash/writer hpp+cpp、gen_compile_test/test_contract、七个 golden 全刷、gen） | **已推送 origin/main**（本会话） |
| 1c18b8c8 | 第四批：CI 两 job（.github/workflows/contract.yml 新增、scripts/ci/ 两脚本、两设计文档） | **已推送 origin/main**（本会话） |
| 18152898 | 第五批：include 聚合（contract_parser hpp+cpp、apollo.xsd、test_contract、contract.yml） | **已推送 origin/main**（本会话） |
| c349f850 | 第六批：装载期一致性闸文件面（modules/contract CMakeLists 三目标拆分 + contract_gate.{hpp,cpp} + test_contract_gate.cpp）——纠偏时为**未提交**工作树改动 | **未推送**；留档于本地分支 `backup-apollo-src` |

另有随批设计文档改动（docs/design/sdk-contract.md、xml-generation.md 的「落地注记」段）——虽非源码，同样超出本轮「只许写 architecture-review.md」的边界，一并如实记录。

**现场处置（纠偏指令 1)-3) 的执行情况）**：

1. **未再执行任何 push**（含禁止的回退性 push）。
2. 第六批未提交改动以单独提交留档于本地分支 `backup-apollo-src`（c349f850，提交信息注明「勿合并/勿推送、待人工裁决」）；`main` 已 `reset --hard origin/main`（18152898）并核实工作树干净（仅存本就存在的 untracked `Testing/Testing/`，未触碰）。本地无未推送提交——本会话四笔提交在纠偏发生前已全部到达 origin，`git reset` 无法撤回远端。
3. 已推送五笔（cb78d4b0…18152898）的回退（revert PR 或 force rewrite）需要写远端，与「严禁 push」冲突，**不自行处置**，留待用户明示；`backup-apollo-src` 的采纳/丢弃同样待裁决。
4. 自本附录落盘起，仓库对本会话恢复只读；后续仅做只读分析且只写本文件。

**教训（面向后续会话的流程修正）**：跨会话恢复时携带的历史授权可能与新一轮约束冲突——应先核对当前轮次的边界再恢复执行，而不是沿用摘要中的旧授权；push 类不可逆动作在每个新阶段开始时重新确认，不以历史授权为凭。
