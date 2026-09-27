# Apollo IoC/DI 设计评审（ioc-review）

> 分析性文档：只评审，不改动任何源码。评审对象为 `include/apollo/framework/ioc`、`include/apollo/starter`、`src/framework/ioc`、`src/starter` 及其全部调用方与设计文档（docs/03、06、08、14、34、architecture/starter-and-module-assembly-design）。
> 结论立场：**逐项分析可取之处，不预设保留**——值得留的给出落地形态，不值得留的明确建议删除。
> 复核追加（2026-09-27）：docs/design 四份设计文档与本评审的交叉一致性复核见 §8；源码级核对第二轮（承接 C-1/C-2 的消费方普查与承重断言复核）见 §9；第三轮（迁移路线调用点/收敛清单/快照验收口径/条件装配实证）见 §10。

---

## 执行摘要（最严重 5 条）

1. **运行时字符串键 Service Locator（P0，类型安全）**：`ApplicationContext` 是全局单例 + 字符串注册/查找 + 每次查找全局互斥锁 + `dynamic_pointer_cast`（`ApplicationContext.h:18-21,79-113`）。字符串拼错、类型不匹配全部推迟到运行时才暴露——这正是 Java 反射式 BeanFactory 在无反射语言里的错位模仿。**建议：整体删除**，收敛到仓库内已有的正确答案 `apollo::core::di`（类型键、构造期注入、零锁读，`modules/core/include/apollo/core/di/application_context.hpp`）。
2. **注册期"探针构造"副作用 bug（P0，正确性）**：`registerComponent` 为了读 phase/dependencies 把工厂**真的执行一次并丢弃实例**（`ApplicationContext.h:34-39`）——每个组件被构造两遍，GUID/随机状态不一致；工厂若申请连接、起线程即产生幽灵资源。**建议：删除探针**，依赖/phase 用编译期 trait 或注册参数显式声明。
3. **四套配置系统并存（P1，双份真相）**：`Apollo::ConfigManager`（framework/ioc）+ `ConfigEnvironment` + `apollo::core::config::ConfigManager` + `DefaultConditionContext.properties_`，靠 `syncConfigEnvironmentToManager()` 单向清空重灌来对齐（`ConfigEnvironment.h:178-194`、`ApolloApplication.cpp:351,365`）；且旧 ConfigManager.cpp 被编进新模块（`modules/core/config/CMakeLists.txt:6`）。**建议：删除旧 `Apollo::ConfigManager` 与 ConfigProperty**，配置收敛到 core::config 一套。
4. **Starter 层的三处死代码/假实现（P1，启动复杂度）**：`!HAVE_FRUIT` 时**伪造 `fruit` 命名空间**（`Starter.h:12-30`，ODR 隐患）；`getService<T>()` 无条件返回空 `shared_ptr`（`ApolloApplication.h:127-131`）；Fruit 组合与 Injector 全是 TODO（`ApolloApplication.cpp:284-292,409-419`）。**建议：删除整个 Fruit 维度**，`ApolloStarter` 收敛为纯装配模板接口。
5. **条件装配在 C++ 里半数无意义 + 求值时序缺陷（P2）**：`ConditionalOnClass` 用运行时字符串集合模拟链接期事实（`Conditional.h:67-80`）；`matches()` 先于 `registerBeans()` 求值，`ConditionalOnBean` 看不到任何 Starter 注册的 Bean（`ApolloApplication.cpp:261 vs 275-277`）。**建议：删除 OnClass/OnBean，保留 OnProperty（配置驱动开关）**，条件求值改为注册时单趟。

**关键背景**：真正跑起来的 game-server（`apps/game-server/src/main.cpp`）用的已经是 `apollo::core::di`；旧 IoC/Starter 栈的全部生产代码调用方为零，只有 examples/tests 在用。**README 把"IoC 容器系统"列为第一核心特性，而实际服务器并不使用它**——迁移成本因此非常低，主要工作是"删"而不是"改"。

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

---

*评审基线（源码）：main @ 35a9c528（无源码变更）。文档基线：六份文档随 a2ab6525；§8 随 86be18d2；§9 随 c99d9d9e。所有行号对应该基线。*
