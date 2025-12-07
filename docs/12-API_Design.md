# Apollo MMORPG API 设计文档

> **版本**: 1.0
> **更新日期**: 2024-12-06
> **协议**: Protocol Buffers v3
> **通信方式**: gRPC / TCP Socket

## 1. API 设计原则

### 1.1 设计理念

1. **RESTful风格**: 使用资源导向的API设计
2. **版本管理**: 所有API支持版本控制
3. **向后兼容**: 新版本保持对旧版本的兼容
4. **幂等性**: 关键操作支持幂等调用
5. **错误处理**: 统一的错误码和错误信息

### 1.2 命名规范

```protobuf
// 服务命名: PascalCase
service PlayerService {}
service GuildService {}

// 方法命名: 动词 + 名词
rpc CreatePlayer(CreatePlayerRequest) returns (CreatePlayerResponse);
rpc GetPlayerInfo(GetPlayerInfoRequest) returns (GetPlayerInfoResponse);

// 消息命名: PascalCase
message PlayerInfo {}
message CreatePlayerRequest {}

// 字段命名: snake_case
message Player {
  uint64 player_id = 1;
  string player_name = 2;
  int32 level = 3;
}
```

## 2. 通用数据结构

### 2.1 基础类型定义

```protobuf
// base.proto
syntax = "proto3";

package apollo.base;

// 通用响应状态
enum StatusCode {
  SUCCESS = 0;
  INVALID_REQUEST = 1000;
  UNAUTHORIZED = 1001;
  PERMISSION_DENIED = 1002;
  NOT_FOUND = 1003;
  ALREADY_EXISTS = 1004;
  INTERNAL_ERROR = 2000;
  SERVICE_UNAVAILABLE = 2001;
  TIMEOUT = 2002;
}

// 通用响应头
message ResponseHeader {
  StatusCode code = 1;
  string message = 2;
  string request_id = 3;
  int64 timestamp = 4;
}

// 分页信息
message Pagination {
  int32 page = 1;
  int32 page_size = 2;
  int32 total = 3;
  int32 total_pages = 4;
}

// 坐标信息
message Position {
  float x = 1;
  float y = 2;
  float z = 3;
  uint32 map_id = 4;
}

// 属性值
message Attribute {
  int32 attr_id = 1;
  int64 value = 2;
  int64 base_value = 3;
}

// 物品信息
message Item {
  uint64 item_id = 1;
  int32 config_id = 2;
  int32 count = 3;
  int64 create_time = 4;
  map<string, string> extra_data = 5;
}
```

### 2.2 通用请求响应

```protobuf
// common.proto
syntax = "proto3";

package apollo.common;

import "base.proto";

// 通用请求
message Request {
  string version = 1;
  string client_id = 2;
  string access_token = 3;
  apollo.base.ResponseHeader header = 4;
}

// 通用响应
message Response {
  apollo.base.ResponseHeader header = 1;
}

// 批量请求
message BatchRequest {
  repeated Request requests = 1;
}

// 批量响应
message BatchResponse {
  repeated Response responses = 1;
}
```

## 3. 玩家服务 API

### 3.1 玩家基础信息

```protobuf
// player.proto
syntax = "proto3";

package apollo.player;

import "base.proto";
import "common.proto";

// 玩家服务
service PlayerService {
  // 创建角色
  rpc CreatePlayer(CreatePlayerRequest) returns (CreatePlayerResponse);

  // 获取玩家信息
  rpc GetPlayerInfo(GetPlayerInfoRequest) returns (GetPlayerInfoResponse);

  // 更新玩家信息
  rpc UpdatePlayerInfo(UpdatePlayerInfoRequest) returns (UpdatePlayerInfoResponse);

  // 删除角色
  rpc DeletePlayer(DeletePlayerRequest) returns (DeletePlayerResponse);

  // 玩家列表
  rpc ListPlayers(ListPlayersRequest) returns (ListPlayersResponse);

  // 玩家在线状态
  rpc GetPlayerOnlineStatus(GetPlayerOnlineStatusRequest) returns (GetPlayerOnlineStatusResponse);
}

// 创建角色请求
message CreatePlayerRequest {
  uint64 account_id = 1;
  string player_name = 2;
  int32 class_id = 3;
  int32 gender = 4;
  apollo.base.Position initial_pos = 5;
}

// 创建角色响应
message CreatePlayerResponse {
  apollo.base.ResponseHeader header = 1;
  uint64 player_id = 2;
  string player_name = 3;
  int64 create_time = 4;
}

// 玩家详细信息
message PlayerInfo {
  uint64 player_id = 1;
  string player_name = 2;
  int32 level = 3;
  int64 exp = 4;
  int32 class_id = 5;
  int32 gender = 6;
  apollo.base.Position position = 7;
  int64 gold = 8;
  int64 diamond = 9;
  uint64 guild_id = 10;
  string guild_name = 11;
  int64 online_time = 12;
  int64 last_login = 13;
  repeated apollo.base.Attribute attributes = 14;
  repeated apollo.base.Item items = 15;
}

// 获取玩家信息请求
message GetPlayerInfoRequest {
  oneof identifier {
    uint64 player_id = 1;
    string player_name = 2;
  }
  bool include_items = 3;
  bool include_attributes = 4;
}

// 获取玩家信息响应
message GetPlayerInfoResponse {
  apollo.base.ResponseHeader header = 1;
  PlayerInfo player_info = 2;
}
```

### 3.2 玩家属性管理

```protobuf
// 属性服务
service AttributeService {
  // 获取属性
  rpc GetAttributes(GetAttributesRequest) returns (GetAttributesResponse);

  // 更新属性
  rpc UpdateAttributes(UpdateAttributesRequest) returns (UpdateAttributesResponse);

  // 批量更新
  rpc BatchUpdateAttributes(BatchUpdateAttributesRequest) returns (BatchUpdateAttributesResponse);
}

// 属性更新操作
message AttributeUpdate {
  int32 attr_id = 1;
  int64 delta = 2;  // 变化量
  UpdateOperation operation = 3;
}

enum UpdateOperation {
  SET = 0;      // 设置值
  ADD = 1;      // 增加
  SUB = 2;      // 减少
  MUL = 3;      // 乘法
}
```

## 4. 战斗系统 API

### 4.1 技能系统

```protobuf
// battle.proto
syntax = "proto3";

package apollo.battle;

import "base.proto";
import "player.proto";

// 战斗服务
service BattleService {
  // 使用技能
  rpc CastSkill(CastSkillRequest) returns (CastSkillResponse);

  // 造成伤害
  rpc DealDamage(DealDamageRequest) returns (DealDamageResponse);

  // 应用Buff
  rpc ApplyBuff(ApplyBuffRequest) returns (ApplyBuffResponse);

  // 移除Buff
  rpc RemoveBuff(RemoveBuffRequest) returns (RemoveBuffResponse);

  // 战斗结算
  rpc BattleEnd(BattleEndRequest) returns (BattleEndResponse);
}

// 使用技能请求
message CastSkillRequest {
  uint64 caster_id = 1;
  uint64 target_id = 2;
  uint32 skill_id = 3;
  apollo.base.Position target_pos = 4;
  repeated uint64 extra_targets = 5;
}

// 使用技能响应
message CastSkillResponse {
  apollo.base.ResponseHeader header = 1;
  bool success = 2;
  uint64 cast_time = 3;
  repeated SkillEffect effects = 4;
}

// 技能效果
message SkillEffect {
  uint64 target_id = 1;
  EffectType type = 2;
  int64 value = 3;
  uint32 duration = 4;
}

enum EffectType {
  DAMAGE = 0;
  HEAL = 1;
  BUFF = 2;
  DEBUFF = 3;
  KNOCKBACK = 4;
}
```

### 4.2 战斗匹配

```protobuf
// 匹配服务
service MatchService {
  // 加入匹配队列
  rpc JoinMatch(JoinMatchRequest) returns (JoinMatchResponse);

  // 离开匹配队列
  rpc LeaveMatch(LeaveMatchRequest) returns (LeaveMatchResponse);

  // 获取匹配状态
  rpc GetMatchStatus(GetMatchStatusRequest) returns (GetMatchStatusResponse);

  // 接受匹配
  rpc AcceptMatch(AcceptMatchRequest) returns (AcceptMatchResponse);
}

// 匹配请求
message MatchRequest {
  uint64 player_id = 1;
  MatchType type = 2;
  repeated uint64 team_members = 3;
  map<string, int32> preferences = 4;  // 匹配偏好
}

enum MatchType {
  SOLO = 0;        // 单人
  TEAM = 1;        // 队伍
  RANKED = 2;      // 排位
  CASUAL = 3;      // 休闲
}
```

## 5. 社交系统 API

### 5.1 好友系统

```protobuf
// social.proto
syntax = "proto3";

package apollo.social;

import "base.proto";

// 社交服务
service SocialService {
  // 添加好友
  rpc AddFriend(AddFriendRequest) returns (AddFriendResponse);

  // 删除好友
  rpc RemoveFriend(RemoveFriendRequest) returns (RemoveFriendResponse);

  // 好友列表
  rpc GetFriendList(GetFriendListRequest) returns (GetFriendListResponse);

  // 搜索玩家
  rpc SearchPlayer(SearchPlayerRequest) returns (SearchPlayerResponse);

  // 黑名单操作
  rpc BlockPlayer(BlockPlayerRequest) returns (BlockPlayerResponse);
}

// 好友信息
message FriendInfo {
  uint64 player_id = 1;
  string player_name = 2;
  int32 level = 3;
  int32 class_id = 4;
  bool online = 5;
  string status = 6;
  int64 last_seen = 7;
  FriendRelationship relationship = 8;
}

enum FriendRelationship {
  FRIEND = 0;
  BLOCKED = 1;
  PENDING = 2;  // 待确认
}
```

### 5.2 公会系统

```protobuf
// 公会服务
service GuildService {
  // 创建公会
  rpc CreateGuild(CreateGuildRequest) returns (CreateGuildResponse);

  // 加入公会
  rpc JoinGuild(JoinGuildRequest) returns (JoinGuildResponse);

  // 离开公会
  rpc LeaveGuild(LeaveGuildRequest) returns (LeaveGuildResponse);

  // 公会信息
  rpc GetGuildInfo(GetGuildInfoRequest) returns (GetGuildInfoResponse);

  // 成员管理
  rpc ManageMember(ManageMemberRequest) returns (ManageMemberResponse);

  // 公会活动
  rpc GuildActivity(GuildActivityRequest) returns (GuildActivityResponse);
}

// 公会信息
message GuildInfo {
  uint64 guild_id = 1;
  string guild_name = 2;
  string description = 3;
  int32 level = 4;
  int64 exp = 5;
  int32 member_count = 6;
  int32 max_members = 7;
  uint64 leader_id = 8;
  string leader_name = 9;
  int64 create_time = 10;
  repeated GuildMember members = 11;
}

// 公会成员
message GuildMember {
  uint64 player_id = 1;
  string player_name = 2;
  int32 level = 3;
  GuildPosition position = 4;
  int64 join_time = 5;
  int32 contribution = 6;
  bool online = 7;
}

enum GuildPosition {
  LEADER = 0;
  OFFICER = 1;
  ELITE = 2;
  MEMBER = 3;
}
```

### 5.3 聊天系统

```protobuf
// 聊天服务
service ChatService {
  // 发送消息
  rpc SendMessage(SendMessageRequest) returns (SendMessageResponse);

  // 获取历史消息
  rpc GetChatHistory(GetChatHistoryRequest) returns (GetChatHistoryResponse);

  // 订阅频道
  rpc SubscribeChannel(SubscribeChannelRequest) returns (SubscribeChannelResponse);

  // 取消订阅
  rpc UnsubscribeChannel(UnsubscribeChannelRequest) returns (UnsubscribeChannelResponse);
}

// 聊天消息
message ChatMessage {
  uint64 message_id = 1;
  uint64 sender_id = 2;
  string sender_name = 3;
  ChatChannel channel = 4;
  string content = 5;
  MessageType type = 6;
  int64 timestamp = 7;
  repeated string receivers = 8;
}

enum ChatChannel {
  WORLD = 0;      // 世界
  GUILD = 1;      // 公会
  TEAM = 2;       // 队伍
  PRIVATE = 3;    // 私聊
  SYSTEM = 4;     // 系统
}

enum MessageType {
  TEXT = 0;       // 文本
  VOICE = 1;      // 语音
  EMOTE = 2;      // 表情
  ITEM = 3;       // 物品链接
}
```

## 6. 场景与移动 API

### 6.1 场景管理

```protobuf
// scene.proto
syntax = "proto3";

package apollo.scene;

import "base.proto";

// 场景服务
service SceneService {
  // 进入场景
  rpc EnterScene(EnterSceneRequest) returns (EnterSceneResponse);

  // 离开场景
  rpc LeaveScene(LeaveSceneRequest) returns (LeaveSceneResponse);

  // 场景内移动
  rpc Move(MoveRequest) returns (MoveResponse);

  // 传送
  rpc Teleport(TeleportRequest) returns (TeleportResponse);

  // 场景信息
  rpc GetSceneInfo(GetSceneInfoRequest) returns (GetSceneInfoResponse);
}

// 进入场景请求
message EnterSceneRequest {
  uint64 player_id = 1;
  uint32 scene_id = 2;
  apollo.base.Position position = 3;
  uint64 instance_id = 4;
}

// 进入场景响应
message EnterSceneResponse {
  apollo.base.ResponseHeader header = 1;
  SceneInfo scene_info = 2;
  repeated NearbyPlayer nearby_players = 3;
  repeated NearbyNPC nearby_npcs = 4;
}

// 场景信息
message SceneInfo {
  uint32 scene_id = 1;
  string scene_name = 2;
  SceneType type = 3;
  apollo.base.Position spawn_point = 4;
  int32 max_players = 5;
  int32 current_players = 6;
}

enum SceneType {
  FIELD = 0;      // 野外
  DUNGEON = 1;    // 副本
  CITY = 2;       // 城市
  INSTANCE = 3;   // 实例
}
```

### 6.2 AOI 服务

```protobuf
// aoi.proto
syntax = "proto3";

package apollo.aoi;

// AOI服务
service AOIService {
  // 更新位置
  rpc UpdatePosition(UpdatePositionRequest) returns (UpdatePositionResponse);

  // 订阅AOI事件
  rpc SubscribeAOI(SubscribeAOIRequest) returns (stream AOIEvent);

  // 获取附近实体
  rpc GetNearbyEntities(GetNearbyEntitiesRequest) returns (GetNearbyEntitiesResponse);
}

// 位置更新请求
message UpdatePositionRequest {
  uint64 entity_id = 1;
  apollo.base.Position position = 2;
  uint64 timestamp = 3;
}

// AOI事件
message AOIEvent {
  EventType type = 1;
  uint64 entity_id = 2;
  EntityType entity_type = 3;
  apollo.base.Position position = 4;
  map<string, string> properties = 5;
}

enum EventType {
  ENTER = 0;      // 进入视野
  LEAVE = 1;      // 离开视野
  MOVE = 2;       // 移动
}

enum EntityType {
  PLAYER = 0;
  NPC = 1;
  MONSTER = 2;
  ITEM = 3;
}
```

## 7. 任务系统 API

### 7.1 任务管理

```protobuf
// quest.proto
syntax = "proto3";

package apollo.quest;

// 任务服务
service QuestService {
  // 获取任务列表
  rpc GetQuestList(GetQuestListRequest) returns (GetQuestListResponse);

  // 接受任务
  rpc AcceptQuest(AcceptQuestRequest) returns (AcceptQuestResponse);

  // 完成任务
  rpc CompleteQuest(CompleteQuestRequest) returns (CompleteQuestResponse);

  // 放弃任务
  rpc AbandonQuest(AbandonQuestRequest) returns (AbandonQuestResponse);

  // 更新任务进度
  rpc UpdateQuestProgress(UpdateQuestProgressRequest) returns (UpdateQuestProgressResponse);
}

// 任务信息
message QuestInfo {
  uint32 quest_id = 1;
  string quest_name = 2;
  string description = 3;
  QuestStatus status = 4;
  QuestType type = 5;
  int32 level_requirement = 6;
  repeated QuestObjective objectives = 7;
  repeated QuestReward rewards = 8;
  int64 accept_time = 9;
  int64 complete_time = 10;
}

// 任务目标
message QuestObjective {
  uint32 objective_id = 1;
  ObjectiveType type = 2;
  int32 target_id = 3;
  int32 current = 4;
  int32 required = 5;
  string description = 6;
}

enum QuestStatus {
  AVAILABLE = 0;    // 可接
  IN_PROGRESS = 1;  // 进行中
  COMPLETED = 2;    // 已完成
  FAILED = 3;       // 失败
}
```

## 8. 经济系统 API

### 8.1 商城系统

```protobuf
// shop.proto
syntax = "proto3";

package apollo.shop;

// 商城服务
service ShopService {
  // 商品列表
  rpc GetShopItems(GetShopItemsRequest) returns (GetShopItemsResponse);

  // 购买商品
  rpc PurchaseItem(PurchaseItemRequest) returns (PurchaseItemResponse);

  // 出售商品
  rpc SellItem(SellItemRequest) returns (SellItemResponse);

  // 交易记录
  rpc GetTransactionHistory(GetTransactionHistoryRequest) returns (GetTransactionHistoryResponse);
}

// 商品信息
message ShopItem {
  uint32 item_id = 1;
  string item_name = 2;
  int32 price = 3;
  CurrencyType currency = 4;
  int32 stock = 5;
  int32 limit = 6;
  int32 sold = 7;
  DiscountInfo discount = 8;
}

enum CurrencyType {
  GOLD = 0;
  DIAMOND = 1;
  BINDING_DIAMOND = 2;
  HONOR = 3;
}

// 购买请求
message PurchaseItemRequest {
  uint64 player_id = 1;
  uint32 item_id = 2;
  int32 quantity = 3;
  CurrencyType currency = 4;
}
```

### 8.2 交易系统

```protobuf
// 交易服务
service TradeService {
  // 创建交易
  rpc CreateTrade(CreateTradeRequest) returns (CreateTradeResponse);

  // 添加物品
  rpc AddTradeItem(AddTradeItemRequest) returns (AddTradeItemResponse);

  // 确认交易
  rpc ConfirmTrade(ConfirmTradeRequest) returns (ConfirmTradeResponse);

  // 取消交易
  rpc CancelTrade(CancelTradeRequest) returns (CancelTradeResponse);
}

// 交易信息
message Trade {
  uint64 trade_id = 1;
  uint64 initiator_id = 2;
  uint64 target_id = 3;
  TradeStatus status = 4;
  repeated TradeSlot initiator_items = 5;
  repeated TradeSlot target_items = 6;
  int64 gold_initiator = 7;
  int64 gold_target = 8;
  int64 create_time = 9;
}
```

## 9. 排行榜 API

### 9.1 排行榜管理

```protobuf
// leaderboard.proto
syntax = "proto3";

package apollo.leaderboard;

// 排行榜服务
service LeaderboardService {
  // 获取排行榜
  rpc GetLeaderboard(GetLeaderboardRequest) returns (GetLeaderboardResponse);

  // 获取个人排名
  rpc GetPlayerRank(GetPlayerRankRequest) returns (GetPlayerRankResponse);

  // 更新分数
  rpc UpdateScore(UpdateScoreRequest) returns (UpdateScoreResponse);

  // 批量更新
  rpc BatchUpdateScores(BatchUpdateScoresRequest) returns (BatchUpdateScoresResponse);
}

// 排行榜请求
message GetLeaderboardRequest {
  LeaderboardType type = 1;
  int32 page = 2;
  int32 page_size = 3;
  TimeRange time_range = 4;
}

// 排行榜条目
message LeaderboardEntry {
  uint64 player_id = 1;
  string player_name = 2;
  int32 rank = 3;
  int64 score = 4;
  repeated string extra_data = 5;
}

enum LeaderboardType {
  LEVEL = 0;       // 等级
  POWER = 1;       // 战力
  ARENA = 2;       // 竞技场
  GUILD = 3;       // 公会
}
```

## 10. 系统管理 API

### 10.1 服务器管理

```protobuf
// admin.proto
syntax = "proto3";

package apollo.admin;

// 管理服务
service AdminService {
  // 服务器状态
  rpc GetServerStatus(GetServerStatusRequest) returns (GetServerStatusResponse);

  // 广播公告
  rpc BroadcastAnnouncement(BroadcastAnnouncementRequest) returns (BroadcastAnnouncementResponse);

  // 玩家管理
  rpc ManagePlayer(ManagePlayerRequest) returns (ManagePlayerResponse);

  // 系统配置
  rpc UpdateSystemConfig(UpdateSystemConfigRequest) returns (UpdateSystemConfigResponse);
}

// 服务器状态
message ServerStatus {
  string server_id = 1;
  ServerState state = 2;
  int32 online_players = 3;
  int32 max_players = 4;
  double cpu_usage = 5;
  double memory_usage = 6;
  int64 uptime = 7;
  string version = 8;
}

enum ServerState {
  STARTING = 0;
  RUNNING = 1;
  MAINTENANCE = 2;
  SHUTTING_DOWN = 3;
  ERROR = 4;
}
```

### 10.2 监控 API

```protobuf
// 监控服务
service MonitoringService {
  // 获取指标
  rpc GetMetrics(GetMetricsRequest) returns (GetMetricsResponse);

  // 获取日志
  rpc GetLogs(GetLogsRequest) returns (GetLogsResponse);

  // 性能分析
  rpc GetProfilingData(GetProfilingDataRequest) returns (GetProfilingDataResponse);
}

// 性能指标
message PerformanceMetrics {
  int64 timestamp = 1;
  double qps = 2;
  double latency_p50 = 3;
  double latency_p95 = 4;
  double latency_p99 = 5;
  int32 error_rate = 6;
  int32 active_connections = 7;
}
```

## 11. 错误码定义

### 11.1 全局错误码

```protobuf
// error_codes.proto
syntax = "proto3";

package apollo.error;

// 错误码映射表
enum ErrorCode {
  // 成功
  OK = 0;

  // 客户端错误 1000-1999
  INVALID_PARAMETER = 1000;
  MISSING_PARAMETER = 1001;
  INVALID_FORMAT = 1002;
  RATE_LIMIT_EXCEEDED = 1003;
  BANNED = 1004;

  // 认证错误 1100-1199
  INVALID_TOKEN = 1100;
  TOKEN_EXPIRED = 1101;
  INVALID_CREDENTIALS = 1102;
  ACCOUNT_LOCKED = 1103;

  // 权限错误 1200-1299
  PERMISSION_DENIED = 1200;
  INSUFFICIENT_LEVEL = 1201;
  INSUFFICIENT_RESOURCES = 1202;

  // 资源错误 1300-1399
  RESOURCE_NOT_FOUND = 1300;
  RESOURCE_ALREADY_EXISTS = 1301;
  RESOURCE_EXHAUSTED = 1302;

  // 业务错误 2000-2999
  PLAYER_NOT_FOUND = 2000;
  PLAYER_ALREADY_ONLINE = 2001;
  GUILD_NOT_FOUND = 2002;
  GUILD_FULL = 2003;
  QUEST_NOT_AVAILABLE = 2004;
  SKILL_IN_COOLDOWN = 2005;
  INSUFFICIENT_INVENTORY_SPACE = 2006;

  // 系统错误 3000-3999
  INTERNAL_SERVER_ERROR = 3000;
  SERVICE_UNAVAILABLE = 3001;
  DATABASE_ERROR = 3002;
  NETWORK_ERROR = 3003;
  TIMEOUT = 3004;
}
```

## 12. API 版本管理

### 12.1 版本策略

```protobuf
// 版本定义
syntax = "proto3";

package apollo.version;

// API版本信息
message APIVersion {
  string major = 1;    // 主版本：不兼容的API修改
  string minor = 2;    // 次版本：向下兼容的功能性新增
  string patch = 3;    // 修订号：向下兼容的问题修正
}

// 版本兼容性
message VersionCompatibility {
  string min_version = 1;
  string max_version = 2;
  string deprecated_version = 3;
}
```

### 12.2 路由策略

```yaml
# API路由配置
api_routes:
  v1:
    - path: /api/v1/player
      service: PlayerService
      deprecated: false
    - path: /api/v1/battle
      service: BattleService
      deprecated: false
  v2:
    - path: /api/v2/player
      service: PlayerServiceV2
      deprecated: false
```

## 13. 安全规范

### 13.1 认证授权

```protobuf
// 认证请求
message AuthRequest {
  string client_id = 1;
  string client_secret = 2;
  string grant_type = 3;
  string code = 4;
  string refresh_token = 5;
}

// 认证响应
message AuthResponse {
  string access_token = 1;
  string refresh_token = 2;
  int32 expires_in = 3;
  string token_type = 4;
}
```

### 13.2 签名验证

```cpp
// 消息签名算法
class MessageSigner {
public:
    // 生成签名
    std::string Sign(const std::string& message,
                     const std::string& secret_key);

    // 验证签名
    bool Verify(const std::string& message,
                const std::string& signature,
                const std::string& public_key);
};
```

## 14. 使用示例

### 14.1 客户端调用示例

```cpp
// C++客户端示例
class GameClient {
    std::unique_ptr<PlayerService::Stub> player_stub_;

public:
    // 创建角色
    bool CreatePlayer(uint64 account_id, const std::string& name) {
        CreatePlayerRequest request;
        request.set_account_id(account_id);
        request.set_player_name(name);

        CreatePlayerResponse response;
        ClientContext context;

        Status status = player_stub_->CreatePlayer(&context,
                                                  request,
                                                  &response);

        return status.ok();
    }
};
```

### 14.2 服务器间调用示例

```cpp
// 服务端调用示例
class BattleController {
    std::unique_ptr<BattleService::Stub> battle_stub_;

public:
    // 使用技能
    void HandleCastSkill(uint64 player_id, uint32 skill_id) {
        CastSkillRequest request;
        request.set_caster_id(player_id);
        request.set_skill_id(skill_id);

        // 异步调用
        battle_stub_->AsyncCastSkill(&context,
                                    request,
                                    &callback);
    }
};
```

## 15. 总结

本API设计文档定义了Apollo MMORPG服务器的完整接口规范，包括：

1. **统一的API设计风格**: 使用Protocol Buffers定义清晰的接口
2. **完整的功能覆盖**: 涵盖玩家、战斗、社交、任务等核心系统
3. **版本管理**: 支持API版本演进和向后兼容
4. **安全机制**: 完整的认证授权和签名验证
5. **错误处理**: 统一的错误码和错误信息

这些API将作为系统间通信的契约，确保各个服务之间的协同工作。