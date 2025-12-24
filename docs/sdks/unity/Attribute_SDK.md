# Unity 属性同步 SDK

## 概述

为 Unity 客户端提供属性同步 SDK，与服务端 `ComVal`/`ProperValue` 属性系统对应，支持：
- 多类型属性值存储
- 属性变更检测与增量同步
- 计算属性支持
- 属性变更事件通知

## 目录结构

```
sdks/unity/ApolloSDK/Attributes/
├── AttributeValue.cs       # 属性值类型（对应服务端 ComVal）
├── AttributeDefinition.cs  # 属性定义
├── AttributeRegistry.cs    # 属性注册表
├── AttributeContainer.cs   # 属性容器（对应服务端 ProperValue）
├── AttributeSyncManager.cs # 属性同步管理器
├── ComputedAttribute.cs    # 计算属性支持
├── AttributeEvents.cs      # 属性变更事件
└── AttributeExtensions.cs  # 扩展方法
```

## 服务端对应关系

| 服务端 | 客户端 |
|--------|--------|
| `BaseClass/ComVal.h` - uComVal | `AttributeValue.cs` |
| `Adapter/CppLua/ObjProperty_enum.h` - 属性ID定义 | `AttributeDefinition.cs` |
| `Game/GlobeObj.h` - ProperValue | `AttributeContainer.cs` |
| `protocol/PacketBuilders.cpp` - 同步逻辑 | `AttributeSyncManager.cs` |

## 属性 ID 范围

```csharp
// 公共对象属性 (1-12)
ATTR_PointId = 1,      // 对象指针ID
ATTR_ObjType = 2,      // 对象类型
ATTR_Direction = 4,    // 方向
ATTR_DisplayName = 5,  // 显示名称
ATTR_NameColor = 6,    // 名字颜色
ATTR_Status = 7,       // 状态 (0=活着 1=死亡)
ATTR_Appr = 8,         // 外观
ATTR_MasterID = 11,    // 主人ID

// 生物属性 (101-223)
ATTR_Sex = 101,        // 性别
ATTR_Level = 105,      // 等级
ATTR_DcMax = 110,      // 最大攻击
ATTR_AcMax = 111,      // 最大防御
ATTR_CurHp = 112,      // 当前血量
ATTR_HpMax = 113,      // 最大血量
ATTR_AddAtkPer = 114,  // 攻击万分比
ATTR_AddHpMaxPer = 115,// 生命万分比
ATTR_Hit = 128,        // 命中(万分比)
ATTR_Dodge = 129,      // 闪避(万分比)
ATTR_CritRatio = 130,  // 暴击概率(万分比)
ATTR_TotalHpMax = 153, // 总最大血量
ATTR_FinalHit = 196,   // 命中率
ATTR_FinalDodge = 197, // 闪避率
ATTR_FinalCritRatio = 198, // 暴击率

// 玩家专属属性 (301-378)
ATTR_ExpMax = 303,        // 升级所需经验
ATTR_BindYuanBao = 305,   // 绑金
ATTR_YuanBao = 306,       // 元宝
ATTR_Diamond = 307,       // 钻石
ATTR_JinBi = 308,         // 金币
ATTR_MaxPower = 313,      // 体力上限
ATTR_PlayerID = 327,      // 角色全服唯一ID
ATTR_TeamId = 372,        // 队伍ID

// 宠物属性 (3001-3100)
PetAttr_Start = 3001,
ATTR_PetIdx = 3002,
ATTR_PetState = 3003,
PetAttr_End = 3100,

// 怪物属性 (5001-6000)
ATTR_MoveInterval = 5001,
ATTR_AttackDistance = 5003,
ATTR_AttackInterval = 5004,

// 物品属性 (8000-9000)
ATTR_CollectionLevel = 8050,
ATTR_ItemType = 8052,
ATTR_ItemIdx = 8053,
ATTR_Dura = 8054,
ATTR_Quality = 8055,
ATTR_Bound = 8061,
ATTR_ItemId = 8063,
```

## 使用示例

```csharp
// 1. 初始化属性系统
AttributeRegistry.Instance.Initialize();

// 2. 创建属性容器
var container = new AttributeContainer(playerId);

// 3. 设置属性
container.SetAttribute(AttributeIDs.ATTR_Level, 50);
container.SetAttribute(AttributeIDs.ATTR_CurHp, 1000);
container.SetAttribute(AttributeIDs.ATTR_HpMax, 5000);

// 4. 获取属性
int level = container.GetInt(AttributeIDs.ATTR_Level);
long hp = container.GetLong(AttributeIDs.ATTR_CurHp);

// 5. 监听属性变化
container.OnAttributeChanged += (evt) => {
    Debug.Log($"属性 {evt.AttributeId} 从 {evt.OldValue} 变为 {evt.NewValue}");
};

// 6. 同步到服务端
byte[] deltaData = AttributeSyncManager.Instance.BuildDeltaPacket(container);
NetworkManager.Send(deltaData);

// 7. 应用服务端更新
AttributeSyncManager.Instance.ApplyServerUpdate(receivedData, container);
```

## 开发任务

### 阶段一：基础类型
- [x] AttributeValue.cs - 属性值包装结构体
- [ ] AttributeDefinition.cs - 属性定义类

### 阶段二：注册与容器
- [ ] AttributeRegistry.cs - 属性注册表
- [ ] AttributeContainer.cs - 属性容器

### 阶段三：同步系统
- [ ] AttributeSyncManager.cs - 属性同步管理器

### 阶段四：计算属性
- [ ] ComputedAttribute.cs - 计算属性支持

### 阶段五：事件系统
- [ ] AttributeEvents.cs - 属性变更事件

### 阶段六：扩展与工具
- [ ] AttributeExtensions.cs - 扩展方法
