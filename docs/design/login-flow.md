# 登录流程

loginServer 认证、gateway 绑定连接、Base 经 database 激活长期对象；进入玩法由 World 管理分配、cell 创建模拟实体。术语见[体系对照](./bigworld-apollo-mapping.md)。

## 典型流程

```text
Client → loginServer：认证，取得凭证与接入信息
Client → gateway：连接、验证凭证、绑定会话
目录：解析所属 Base（不因跨服改变）
baseServer → database：加载长期资料，确认激活
Client → gateway → Base：基础业务或申请玩法
Base → worldServer：成员资格、Base 关联、玩法快照与准入申请
World → cell：分配 Space/Cell，创建临时 Instance（如需要）
目录/gateway：记录已提交 World/Space/Cell/执行端位置投影
cell ↔ gateway ↔ Client：初始场景与后续模拟输入/状态
```

服务端验证区服、版本、容量、凭证与成员；客户端不能自行指定资料归属或数据库。未进玩法的玩家也可有长期 Base 对象。World 不另持一份模拟实体；cell 不加载完整长期档案。

## 重连与实现状态

重复登录撤销旧会话；重连恢复原 Base 和当前模拟位置，不创建第二个资料写入者。已有认证、接入和目录原语可复用，Base/database 原型已提供远程读写，默认登录路由、World/cell 拆分及真实恢复仍待接线。测试档案或注入会话不代表生产认证验收，任务见根目录 todo.md。
