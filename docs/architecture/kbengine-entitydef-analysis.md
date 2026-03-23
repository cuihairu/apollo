# KBEngine EntityDef 与属性自动化分析

## 1. 结论

KBEngine 让人觉得“开发量一下子少了很多”，核心原因不是单纯用了 XML，而是它把实体定义抽成了一套统一的运行时元数据模型。  
一份定义会同时驱动：

- 实体属性注册
- Base/Cell/Client 可见性划分
- 网络同步
- 持久化
- 默认值初始化
- 类型校验
- 客户端实体定义导出
- 远程方法分发

所以它本质上是 schema 驱动，而不是配置文件驱动这么简单。

## 2. 关键文件分工

### 2.1 `entities.xml`

示例文件：

- `C:\Users\cui\Workspaces\kbengine\kbe\res\sdk_templates\server\python_assets\scripts\entities.xml`

作用不是详细定义属性，而是声明有哪些实体，以及实体是否存在 `hasBase`、`hasCell`、`hasClient` 这些部分。

例如：

```xml
<root>
    <Account hasClient="true"></Account>
</root>
```

它更像“实体清单 + 组件存在性声明”。

### 2.2 `entity_defs/*.def`

示例文件：

- `C:\Users\cui\Workspaces\kbengine\kbe\res\sdk_templates\server\python_assets\scripts\entity_defs\Account.def`

真正的实体定义在这里，包括：

- `Properties`
- `ClientMethods`
- `BaseMethods`
- `CellMethods`

这才是属性和方法元数据的主来源。

### 2.3 `entity_defs/types.xml`

示例文件：

- `C:\Users\cui\Workspaces\kbengine\kbe\res\sdk_templates\server\python_assets\scripts\entity_defs\types.xml`

这里定义的是类型系统扩展，例如：

- 基础类型别名
- `FIXED_DICT`
- `ARRAY`

它相当于实体属性系统的类型注册中心。

## 3. 初始化主链路

入口在：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\entitydef.cpp`

关键函数：

- `EntityDef::initialize`

初始化顺序大致是：

1. 确定脚本路径
2. 初始化内建 flags 映射
3. 加载 `entity_defs/types.xml`
4. 加载 `entities.xml`
5. 遍历每个实体
6. 读取对应的 `.def`
7. 解析属性和方法
8. 构建 `ScriptDefModule`
9. 加载脚本模块
10. 初始化 watcher 和客户端导出等后续能力

可以理解为：

```text
types.xml
    -> DataTypes

entities.xml
    -> 实体名单

entity_defs/*.def
    -> PropertyDescription / MethodDescription

这些描述对象
    -> ScriptDefModule

ScriptDefModule
    -> 同步 / 持久化 / 客户端导出 / 方法调用
```

## 4. 最核心的抽象：`PropertyDescription`

定义在：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\property.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\property.cpp`

这个类是整套自动化的核心。它不只是“属性描述结构体”，而是“属性元数据 + 行为”的统一对象。

它保存的信息包括：

- 属性名
- 数据类型 `DataType`
- flags
- 是否持久化
- 是否是 identifier
- 索引类型
- 默认值字符串
- `utype`
- `detailLevel`
- `aliasID`

更关键的是，它直接提供行为：

- `newDefaultVal()`
- `parseDefaultStr()`
- `addToStream()`
- `createFromStream()`
- `addPersistentToStream()`
- `createFromPersistentStream()`
- `isSameType()`

这意味着：

- 属性怎么序列化，不再由调用方决定
- 属性怎么反序列化，不再由调用方决定
- 属性默认值怎么构造，不再由业务层决定
- 属性持久化格式怎么写，也复用同一套定义

这就是 KBEngine 自动化能力很强的根本原因。

## 5. 属性定义是如何解析出来的

解析逻辑在：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\entitydef.cpp`

关键函数：

- `EntityDef::loadDefPropertys`

每个属性节点会解析出以下信息：

- `Flags`
- `Persistent`
- `Type`
- `Index`
- `Identifier`
- `DatabaseLength`
- `Default`
- `DetailLevel`
- `Utype`

然后通过：

- `PropertyDescription::createDescription(...)`

自动创建描述对象。

对于复杂类型，还会自动切换到不同子类：

- `FixedDictDescription`
- `ArrayDescription`
- `VectorDescription`
- `EntityComponentDescription`

这一步完成后，属性已经不是 XML 节点了，而是运行时对象。

## 6. `ScriptDefModule` 是实体的运行时总表

定义在：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\scriptdef_module.h`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\entitydef\scriptdef_module.cpp`

它可以理解成“某个实体类型的完整 schema 对象”。

里面维护了多套索引：

- `cellPropertyDescr_`
- `basePropertyDescr_`
- `clientPropertyDescr_`
- `persistentPropertyDescr_`
- 对应的 `uidmap`
- `methodCellDescr_`
- `methodBaseDescr_`
- `methodClientDescr_`

也就是说，一份属性定义在加载时会自动分发到多个视图：

- 面向 Cell 的视图
- 面向 Base 的视图
- 面向 Client 的视图
- 面向 DB 的视图

调用方不需要重新写过滤逻辑，只需要查对应表。

### 6.1 自动别名分配

在 `ScriptDefModule::onLoaded()` 里，还会自动给客户端属性和方法分配 `aliasID`。

这样做的意义是：

- 内部仍然用完整 `utype`
- 对客户端发包时，如果数量足够小，可以压缩成 1 字节或更小的别名

这是一种很典型的“schema 驱动协议压缩”。

## 7. 自动化到底体现在哪些地方

### 7.1 网络同步自动化

相关代码：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseapp\entity.cpp`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\entity.cpp`

属性同步时，核心逻辑并不关心某个属性叫 HP 还是 MP，而是：

1. 找到 `PropertyDescription`
2. 根据 `flags` 判断该发给谁
3. 根据是否启用 alias 决定发 `aliasID` 还是 `utype`
4. 调 `propertyDescription->getDataType()->addToStream(...)`
5. 直接写入包体

所以新增属性后，通常不需要去同步系统里再补一份分支逻辑。

### 7.2 持久化自动化

相关代码：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\db_interface\entity_table.cpp`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\lib\db_redis\entity_table_redis.cpp`

数据库层初始化时，直接遍历：

- `getPersistentPropertyDescriptions()`

然后为每个 persistent 属性创建对应的表项处理器。

写库时：

- 按 `utype` 找 `EntityTableItem`
- 按属性类型写入

读库时：

- 还是按同一套定义反序列化

这意味着“要不要入库”不是 DB 层配置，而是实体定义的一部分。

### 7.3 客户端导出自动化

相关代码：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseapp\baseapp.cpp`

服务端会把以下内容打包导给客户端：

- datatype 列表
- client properties
- client methods
- exposed base methods
- exposed cell methods

客户端拿到的不是一堆手写协议，而是一份运行时 schema 导出结果。

这就是为什么它很像一个轻量级 IDL 系统。

### 7.4 远程方法分发自动化

相关代码：

- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\baseapp\entity.cpp`
- `C:\Users\cui\Workspaces\kbengine\kbe\src\server\cellapp\entity.cpp`

收到远程调用时，并不是手写 switch 分发，而是：

1. 根据 `utype` 找 `MethodDescription`
2. 通过方法描述解析参数
3. 再调用对应脚本方法

方法定义也进入了统一元模型体系。

## 8. 为什么它能显著减少开发工作

传统做法里，一个属性往往要重复定义多次：

- 代码字段
- 网络协议
- DB 字段
- 客户端 ID
- 默认值
- 校验逻辑
- 可见性规则

KBEngine 的做法是只维护一份实体 schema，然后让多个子系统共享这份 schema。

因此新增一个属性，开发者主要做的是：

1. 在 `.def` 中声明它
2. 在脚本逻辑中使用它

而不需要把同步、DB、导出、类型检查都手写一遍。

这就是它“极大简化开发”的真正来源。

## 9. 它的代价

这套设计也有明显代价：

- 编译期约束减少，错误更多暴露在启动期或运行期
- XML + Python + C++ 三层联动，调试成本高
- schema 修改会影响客户端、DB、旧数据兼容
- 自动化能力强，但 runtime 复杂度很高

所以它本质上是：

- 用一套复杂的元数据 runtime
- 换掉大量重复的业务样板代码

## 10. 和 Apollo 当前状态的对比

当前 Apollo 仓库里已存在的，是：

- 属性容器
- 属性同步文档
- BigWorld 兼容层
- `EntitySchema` 方向的设计文档

例如：

- `C:\Users\cui\Workspaces\apollo\include\apollo\game\attributes\attribute.hpp`
- `C:\Users\cui\Workspaces\apollo\modules\game\attributes\src\attribute_value.cpp`
- `C:\Users\cui\Workspaces\apollo\docs\architecture\entity-schema-design.md`
- `C:\Users\cui\Workspaces\apollo\docs\33-BigWorld_Compatibility.md`

但和 KBEngine 的 `entitydef` 子系统相比，还缺少一个真正落地的统一运行时 schema 层，用来打通：

- 实体定义加载
- 属性描述对象
- 方法描述对象
- Base/Cell/Client 视图
- 持久化映射
- 客户端导出
- 远程方法分发

换句话说，Apollo 现在更像：

- 有属性系统
- 有兼容层
- 有设计蓝图

但还没有完整复刻 KBEngine 这条“实体定义驱动全链路自动化”的骨架。

## 11. 对 Apollo 的直接启发

如果 Apollo 后续要吸收 KBEngine 这套优势，最值得抽取的不是 XML 本身，而是下面这几个抽象：

- `EntitySchema`
- `PropertySchema`
- `MethodSchema`
- `TypeRegistry`
- `ReplicationDescriptor`
- `PersistenceDescriptor`

建议目标不是照搬 KBEngine 的 XML 格式，而是保留它的核心思想：

- 单一实体定义来源
- 多子系统复用同一份元数据
- 运行时描述对象驱动同步、持久化、导出与分发

## 12. 一句话总结

KBEngine 真正厉害的不是“实体用 XML 定义”，而是“把实体定义做成可执行的运行时 schema”，然后让属性、同步、持久化、客户端导出和远程调用都围绕这份 schema 自动运转。
