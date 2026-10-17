
# 登录流程

本文说明房间制模型中的登录边界。目标拓扑见[架构总览](../guide/architecture.md)。

## 职责

- `Login` 验证账号并签发短期登录凭证。
- 客户端使用凭证建立游戏连接到 `Gateway`。
- `Gateway` 校验凭证并建立连接会话，再根据在线路由将消息转发给玩家当前所在的 RoomServer。
- RoomServer 接受加入房间请求后创建/恢复玩家的对局运行对象；它是对局状态唯一权威。

账号认证、客户端连接和对局模拟是三个职责。登录凭证只证明账号认证通过，不直接决定玩家对象由哪个进程持有。进入房间时的准入和路由信息必须由服务端验证，客户端不能自行指定权威 RoomServer。

## 典型流程

```text
Client → Login: 账号认证
Login → Client: 登录凭证
Client → Gateway: 建立游戏连接并提交凭证
Gateway → RoomServer: 验证会话并路由入房请求
RoomServer → Gateway/Client: 接受入房并返回初始状态
```

具体的票据格式、加密握手、过期时间、重放防护、排队和账号存储方式由实现设计确定；不得在本文把未实现细节表述为当前能力。

## 重连与迁移

同一账号重新登录时，旧会话必须失效或被明确替换。断线重连恢复会话身份，但连接本身可能重建。跨服迁移由[会话与在线路由](./session-and-online-directory.md)定义：RoomServer 先完成权威接管，随后更新 Gateway 的路由。

## 实现状态

仓库含 `login-app` 和 `gateway-app` 原型。当前端到端登录、Gateway 凭证校验、RoomServer 路由和跨服接管仍需按此流程验收。原型代码中的 Base/Cell 路由不构成目标流程。
