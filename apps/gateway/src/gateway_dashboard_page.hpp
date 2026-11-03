#pragma once

// gateway 维度 dashboard 首页（DASH 复用批）：单页 HTML，无框架，2s 轮询
// /api/state 渲染六块——进程 / 监听·限流 / 会话 / 准入·拒绝 / intents / tick。
// 风格与 world 内建页同源（暗色卡片表），传输面复用 world_app::
// DashboardResponder（DashboardConfig.html 覆写装配）。
//
// 数据源形状见 GatewayServer::dashboardJson()：role/process/listen/sessions/
// auth/rejects/rate/intents/tick/metrics。

namespace gateway {

inline constexpr const char kGatewayDashboardHtml[] = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Apollo Gateway 状态</title>
<style>
body{font-family:system-ui,sans-serif;margin:16px;background:#111;color:#eee}
h1{font-size:1.2em}h2{font-size:1em;margin:12px 0 4px}
table{border-collapse:collapse;width:100%;margin-bottom:8px}
th,td{border:1px solid #444;padding:2px 6px;text-align:left;font-size:0.9em}
th{background:#222}
.ok{color:#4c4}.bad{color:#c44}.warn{color:#e80}
.card{background:#1a1a1a;border:1px solid #333;border-radius:4px;padding:8px;margin-bottom:8px}
.card span{display:inline-block;margin-right:16px;font-size:0.9em}
</style>
</head>
<body>
<h1>Apollo Gateway <span id="listen"></span> · uptime <span id="uptime"></span></h1>
<div class="card" id="process"><h2>① 进程</h2><div id="process-body"></div></div>
<div class="card" id="sessions"><h2>② 会话</h2><div id="sessions-body"></div></div>
<div class="card" id="auth"><h2>③ 准入·拒绝</h2><div id="auth-body"></div><div id="rejects-body"></div></div>
<div class="card" id="rate"><h2>④ 限流·排空</h2><div id="rate-body"></div></div>
<div class="card" id="intents"><h2>⑤ intents（M1 出口）</h2><div id="intents-body"></div></div>
<div class="card" id="tick"><h2>⑥ 网关 tick</h2><div id="tick-body"></div><table id="metrics-table"></table></div>
<script>
let token_ok = true;
async function refresh() {
  try {
    const r = await fetch('/api/state');
    if (r.status === 401) { token_ok = false; document.getElementById('uptime').textContent = '[token 未授权]'; return; }
    token_ok = true;
    const s = await r.json();
    const uptime_s = s.process.uptime_ms / 1000;
    const h = Math.floor(uptime_s / 3600);
    const m = Math.floor((uptime_s % 3600) / 60);
    document.getElementById('uptime').textContent = h + 'h' + m + 'm';
    document.getElementById('listen').textContent =
      (s.listen.host + ':' + s.listen.port + (s.listen.ws_port ? ' ws:' + s.listen.ws_port : '')
       + (s.listen.wss_port ? ' wss:' + s.listen.wss_port : ''));
    document.getElementById('process-body').innerHTML =
      '<span>pid ' + s.process.pid + '</span><span>rss ' + s.process.rss_kb + ' KB</span>'
      + '<span>threads ' + s.process.threads + '</span><span>started ' + new Date(s.process.started_ms).toLocaleString() + '</span>';
    const sb = document.getElementById('sessions-body');
    sb.innerHTML =
      '<span>在线 <b>' + s.sessions.online + '</b></span>'
      + '<span>已认证 <b class="ok">' + s.sessions.authenticated + '</b></span>'
      + '<span>保留窗 ' + s.sessions.retained + '</span>'
      + '<span>连接 ' + s.listen.active_connections + '/' + s.listen.max_connections + '</span>'
      + '<span>心跳超时累计 ' + s.sessions.heartbeat_timeout_total + '</span>';
    document.getElementById('auth-body').innerHTML =
      '<span>准入请求 ' + s.auth.requests + '</span>'
      + '<span>通过 <b class="ok">' + s.auth.accepted + '</b></span>'
      + '<span>拒绝 <b class="' + (s.auth.rejected ? 'bad' : 'ok') + '">' + s.auth.rejected + '</b></span>';
    document.getElementById('rejects-body').innerHTML =
      '<span>未认证 world 帧 ' + s.rejects.unauthenticated_world + '</span>'
      + '<span>未认证 base 帧 ' + s.rejects.unauthenticated_base + '</span>'
      + '<span>重复/乱序帧 ' + s.rejects.duplicate_frames + '</span>';
    const rateEl = document.getElementById('rate-body');
    rateEl.innerHTML =
      '<span>排空状态 <b class="' + (s.rate.drain_state === 'serving' ? 'ok' : 'warn') + '">' + s.rate.drain_state + '</b></span>'
      + '<span>Base 归属解析 ' + s.rate.base_forward_resolves + '</span>'
      + '<span>Base 拒收 ' + s.rate.base_forward_rejections + '</span>';
    if (s.rate.drain_detail) rateEl.innerHTML += '<div style="font-size:0.8em;color:#888">' + s.rate.drain_detail + '</div>';
    const it = s.intents;
    document.getElementById('intents-body').innerHTML =
      '<span>sent <b>' + it.sent + '</b></span><span>acked <b class="ok">' + it.acked + '</b></span>'
      + '<span>failed <span class="' + (it.failed ? 'bad' : '') + '">' + it.failed + '</span></span>'
      + '<span>timed_out <span class="' + (it.timed_out ? 'bad' : '') + '">' + it.timed_out + '</span></span>'
      + '<span>unforwardable ' + it.unforwardable + '</span><span>dropped ' + it.dropped + '</span>';
    document.getElementById('tick-body').innerHTML =
      '<span>pump ' + s.tick.pump_hz + ' Hz</span><span>ticks ' + s.tick.pump_ticks + '</span>';
    const mt = document.getElementById('metrics-table');
    mt.innerHTML = '<tr><th>metric</th><th>value</th></tr>';
    for (const g of (s.metrics || [])) {
      const v = g.is_histogram ? ('count=' + g.count + ' sum=' + Math.round(g.sum)) : g.value;
      mt.innerHTML += '<tr><td>' + g.name + '</td><td>' + v + '</td></tr>';
    }
  } catch (e) { document.getElementById('uptime').textContent = 'refresh failed'; }
}
refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)HTML";

} // namespace gateway
