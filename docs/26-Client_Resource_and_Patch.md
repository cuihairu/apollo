# 客户端资源与补丁策略

> 目的：统一 Unity/U3D 客户端的资源打包、版本控制、热更新流程，使其与服务器的配置与灰度策略匹配，类似火龙果等产品提供的 SDK/资源方案。

## 1. 总体流程

```
资源制作 (美术/策划) → AssetBundle 构建 → 版本打包 → CDN/OSS 发布
                      ↘ 差异包生成 → Patch Manifest → SDK 资源管理器
客户端启动 → 检查 Manifest → 下载差异 → 校验 → 加载运行
```

服务器参与点：
- 向 SDK 返回当前服要求的资源版本/灰度信息。
- 若版本不符，提示更新或强制下载。

## 2. 资源结构

| 目录 | 内容 |
|------|------|
| `assets/bundles` | Unity AssetBundle、单元资源 |
| `assets/config` | JSON/ScriptableObject 配置（与服务器配置对应） |
| `assets/localization` | 语言包 |
| `patch/manifest.json` | 包含版本、依赖、大小、hash 的文件描述 |
| `patch/diffs/` | 差分包目录 |

Manifest 示例：
```json
{
  "app_version": "1.0.0",
  "resource_version": "2025.01.05",
  "bundles": [
    {
      "name": "characters.ab",
      "hash": "abcdef...",
      "size": 5242880,
      "dependencies": ["animations.ab"],
      "priority": 1
    }
  ]
}
```

## 3. Unity SDK 资源管理

### 3.1 功能
- 启动时获取服务器的资源版本（通过 API），对比本地 Manifest。
- 下载差分包或全量资源，支持断点续传、多线程。
- 校验 Hash，存储到本地缓存（Versioned folder）。
- 提供事件回调（进度、失败重试）。
- 支持灰度：根据 `slot`（来自服务器 NetCore 握手）选择不同资源版本。

### 3.2 热更新流程
1. 客户端启动，加载内置 Manifest（与 APK 同步）。
2. 调用服务器 API（或 CDN Manifest）获取最新版本。
3. 若版本较新，下载差异文件（按优先级）并更新 Manifest。
4. 对配置/脚本（如 Lua）可加载内存执行，资源（模型、UI）加载到 AssetBundle。

### 3.3 安全
- 下载使用 HTTPS；Manifest 签名/加密，防篡改。
- 客户端验证文件 Hash 与签名。
- 可启用文件加密（XOR/AES），配合 License 控制。

## 4. CDN/发布策略

### 4.1 多环境
- Dev/Test/Staging/Prod 各自拥有独立资源仓库。
- 支持灰度/地区区分（例如 `cdn.com/prod/asia/`）。

### 4.2 发布流程
1. 资源构建 → 上传 OSS（含版本号）。
2. 生成 Manifest + 差分包（基于上一版本）。
3. 触发 CDN 刷新（特定路径）。
4. 更新版本数据库（供服务器/SDK 查询）。

工具建议：Unity Addressables + 自研 Manifest；或结合第三方（AssetGraph）。

## 5. 服务器接口

### 5.1 资源版本 API
```http
GET /api/v1/resources/version?app=client&platform=android&slot=A
Response:
{
  "resource_version": "2025.01.05",
  "manifest_url": "https://cdn/.../manifest.json",
  "force_update": false,
  "message": "Optional update"
}
```
- 服务器根据玩家所属大区/灰度 slot 返回不同版本。
- `force_update=true` 时客户端必须更新后才能进入游戏。

### 5.2 配置更新
- 当服务器配置更新（`docs/02`）时，应同步生成客户端配置包，确保字段一致。
- 可使用相同 IDL/生成工具输出 JSON/Lua，避免手工同步。

## 6. 灰度与回滚
- 版本策略：`major.minor.patch` + 资源日期，如 `1.2.0+20250105`.
- 灰度发布：
  1. 将 Manifest 标记为灰度版本，关联玩家 slot。
  2. SDK 根据 slot 下载对应资源。
  3. 若监控正常，再将版本切换为全量。
- 回滚：保留上一版本 Manifest + 资源包，切换 API 指向旧版本即可。

## 7. 监控与告警
- 指标：下载成功率、平均下载时间、失败次数、灰度覆盖率。
- SDK 上传下载日志（带 trace_id），便于分析问题。
- CDN/OSS 使用监控（QPS、带宽、错误率）。

## 8. 实施步骤
1. 确定资源打包方案（Unity Addressables/自研）。
2. 开发 Manifest 生成工具、差异包生成工具。
3. 搭建 CDN/OSS 流程、版本数据库、发布脚本。
4. 在 Unity SDK 中实现资源管理模块，支持热更新、灰度。
5. 与服务器 API 接入，完成版本控制。
6. 建立监控/日志系统，定期回顾。

---

通过该策略，客户端资源更新将与服务器配置/灰度机制同步，可高效、安全地向玩家发布新内容或修复补丁。***
