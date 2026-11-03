#include "world_app/dashboard.hpp"

#if !defined(_WIN32)

#include <cstring>
#include <sstream>

namespace world_app {

namespace {

// 读侧缓冲（MetricsHttp 同值——单响应语义，一次 read chunk）
constexpr std::size_t kReadChunk = 4096;

// 内嵌单页 HTML（§6 页面结构）：无框架，内嵌 script 每 2s fetch('/api/state')
// 渲染六块。内容随阶段 3 完整化；本批先给骨架（token 状态/区块占位），
// JSON 面先于页面完整化。
const char kDashboardHtml[] = R"HTML(<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Apollo 状态</title>
<style>
body{font-family:system-ui,sans-serif;margin:16px;background:#111;color:#eee}
h1{font-size:1.2em}h2{font-size:1em;margin:12px 0 4px}
table{border-collapse:collapse;width:100%;margin-bottom:8px}
th,td{border:1px solid #444;padding:2px 6px;text-align:left;font-size:0.9em}
th{background:#222}
.ok{color:#4c4}.bad{color:#c44}
.card{background:#1a1a1a;border:1px solid #333;border-radius:4px;padding:8px;margin-bottom:8px}
.card span{display:inline-block;margin-right:16px;font-size:0.9em}
</style>
</head>
<body>
<h1>Apollo 状态 <span id="role"></span> · uptime <span id="uptime"></span></h1>
<div class="card" id="process"><h2>⑥ 进程信息</h2><div id="process-body"></div></div>
<div class="card" id="fleet"><h2>① 编队成员</h2><table id="fleet-table"></table></div>
<div class="card" id="links"><h2>② link/TLS</h2><table id="links-table"></table></div>
<div class="card" id="orders"><h2>③ orders 队列</h2><div id="orders-body"></div><table id="orders-table"></table></div>
<div class="card" id="script"><h2>④ script runtime</h2><table id="script-table"></table></div>
<div class="card" id="load"><h2>⑤ tick/负载</h2><div id="load-body"></div></div>
<script>
let token_ok = true;
async function refresh() {
  try {
    const r = await fetch('/api/state');
    if (r.status === 401) { token_ok = false; document.getElementById('role').textContent = '[token 未授权]'; return; }
    token_ok = true;
    const s = await r.json();
    const uptime_s = s.process.uptime_ms / 1000;
    const h = Math.floor(uptime_s / 3600);
    const m = Math.floor((uptime_s % 3600) / 60);
    document.getElementById('uptime').textContent = h + 'h' + m + 'm';
    document.getElementById('role').textContent = s.process.role || '';
    document.getElementById('process-body').innerHTML =
      '<span>pid ' + s.process.pid + '</span><span>rss ' + s.process.rss_kb + ' KB</span>'
      + '<span>threads ' + s.process.threads + '</span><span>started ' + new Date(s.process.started_ms).toLocaleString() + '</span>';
    const ft = document.getElementById('fleet-table');
    ft.innerHTML = '<tr><th>role</th><th>pid</th><th>state</th><th>ready</th><th>log tail</th></tr>';
    for (const f of s.fleet) {
      ft.innerHTML += '<tr><td>' + f.role + '</td><td>' + f.pid + '</td><td>' + (f.alive ? 'running' : 'stopped')
        + '</td><td class="' + (f.ready ? 'ok' : 'bad') + '">' + (f.ready ? 'ready' : 'no') + '</td><td><pre style="margin:0;white-space:pre-wrap;font-size:0.8em">' + (f.log_tail || []).join('\n') + '</pre></td></tr>';
    }
    const lt = document.getElementById('links-table');
    lt.innerHTML = '<tr><th>peer</th><th>state</th><th>tls</th><th>tx</th><th>rx</th></tr>';
    for (const l of s.links) {
      lt.innerHTML += '<tr><td>' + l.peer + '</td><td class="' + (l.state === 'up' ? 'ok' : 'bad') + '">' + l.state
        + '</td><td>' + (l.tls ? 'yes' : 'no') + '</td><td>' + l.tx + '</td><td>' + l.rx + '</td></tr>';
    }
    document.getElementById('orders-body').innerHTML =
      '<span>pending ' + s.orders.pending + '</span><span>delivered ' + s.orders.delivered + '</span>';
    const ot = document.getElementById('orders-table');
    ot.innerHTML = '<tr><th>order</th><th>state</th></tr>';
    for (const o of (s.orders.pending_detail || []).slice(0, 20)) {
      ot.innerHTML += '<tr><td>' + o.order_id + '</td><td>' + o.state + '</td></tr>';
    }
    const st = document.getElementById('script-table');
    st.innerHTML = '<tr><th>module</th><th>version</th></tr>';
    for (const m of s.script.modules) {
      st.innerHTML += '<tr><td>' + m.name + '</td><td>' + m.version + '</td></tr>';
    }
    document.getElementById('load-body').innerHTML =
      '<span>ticks_total ' + s.load.ticks_total + '</span>'
      + (s.load.gauges || []).map(g => '<span>' + g.name + ' ' + g.value + '</span>').join('');
  } catch (e) { document.getElementById('uptime').textContent = 'refresh failed'; }
}
refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)HTML";

// 401/404 响应体
constexpr const char* kUnauthorizedBody = "Unauthorized\r\n";
constexpr const char* kNotFoundBody = "Not Found\r\n";

// 请求头里取 Authorization 值（首个冒号后的空白剥除，行尾 \r 剥除——读侧
// 按完整头聚合，\r\n\r\n 分界）。
std::string header_value(const std::string& request, const char* name) {
    const std::string needle = std::string(name) + ":";
    std::size_t pos = 0;
    while ((pos = request.find(needle, pos)) != std::string::npos) {
        const auto line_end = request.find("\r\n", pos);
        if (line_end == std::string::npos) {
            break;
        }
        if (pos > 0 && request[pos - 1] != '\n' && request[pos - 1] != '\r') {
            // 行首必须紧贴行首（非 name 内子串——例如 "XAuthorization"）
            pos += needle.size();
            continue;
        }
        std::size_t value_pos = pos + needle.size();
        while (value_pos < line_end && (request[value_pos] == ' '
                                        || request[value_pos] == '\t')) {
            ++value_pos;
        }
        return request.substr(value_pos, line_end - value_pos);
    }
    return {};
}

}  // namespace

DashboardResponder::DashboardResponder(DashboardConfig config,
                                       DashboardStateFn state_fn)
    : config_(std::move(config)), state_fn_(std::move(state_fn)) {}

bool DashboardResponder::start() {
    if (listener_.listening()) {
        return true;
    }
    return listener_.start(config_.host, config_.port);
}

void DashboardResponder::stop() {
    listener_.close();
    conns_.clear();
}

std::uint16_t DashboardResponder::port() const { return listener_.port(); }

std::size_t DashboardResponder::connections() const { return conns_.size(); }

std::uint64_t DashboardResponder::served() const {
    return served_.load(std::memory_order_relaxed);
}

std::uint64_t DashboardResponder::refused() const {
    return refused_.load(std::memory_order_relaxed);
}

std::uint64_t DashboardResponder::aborted() const {
    return aborted_.load(std::memory_order_relaxed);
}

std::uint64_t DashboardResponder::unauthorized() const {
    return unauthorized_.load(std::memory_order_relaxed);
}

void DashboardResponder::accept_new() {
    for (;;) {
        auto transport = listener_.accept();
        if (transport == nullptr) {
            break;
        }
        if (conns_.size() >= config_.max_connections) {
            ++refused_;
            continue;
        }
        Conn conn;
        conn.transport = std::move(transport);
        conn.deadline_ms = 0;
        conns_.push_back(std::move(conn));
    }
}

void DashboardResponder::pump(std::uint64_t now_ms) {
    if (!listener_.listening()) {
        return;
    }
    accept_new();

    for (std::size_t i = 0; i < conns_.size();) {
        Conn& conn = conns_[i];
        if (conn.deadline_ms == 0) {
            conn.deadline_ms = now_ms;
        }
        if (now_ms - conn.deadline_ms > config_.write_timeout_ms) {
            ++aborted_;
            conn.transport->close();
            conns_.erase(conns_.begin() + i);
            continue;
        }
        if (!conn.queued) {
            if (!read_and_maybe_answer(conn, now_ms)) {
                ++aborted_;
                conn.transport->close();
                conns_.erase(conns_.begin() + i);
                continue;
            }
            if (!conn.queued) {
                ++i;
                continue;
            }
            continue;  // 本拍刚生成响应——下拍推进写出
        }
        if (!write_out(conn)) {
            ++aborted_;
            conn.transport->close();
            conns_.erase(conns_.begin() + i);
            continue;
        }
        if (conn.sent >= conn.response.size()) {
            conn.transport->close();
            ++served_;
            conns_.erase(conns_.begin() + i);
            continue;
        }
        ++i;
    }
}

bool DashboardResponder::authorized(const std::string& header) const {
    if (config_.token.empty()) {
        return true;
    }
    return header == "Bearer " + config_.token;
}

bool DashboardResponder::read_and_maybe_answer(Conn& conn,
                                               std::uint64_t now_ms) {
    std::uint8_t buf[kReadChunk];
    for (;;) {
        const int n = conn.transport->read(buf, sizeof buf);
        if (n < 0) {
            return false;
        }
        if (n == 0) {
            break;
        }
        conn.recv.append(reinterpret_cast<const char*>(buf),
                         static_cast<std::size_t>(n));
    }
    if (conn.queued) {
        return true;
    }
    if (conn.recv.find("\r\n\r\n") == std::string::npos) {
        return true;
    }
    conn.recv.erase(conn.recv.find("\r\n\r\n") + 4);

    const std::string auth = header_value(conn.recv, "Authorization");
    if (!authorized(auth)) {
        ++unauthorized_;
        std::ostringstream out;
        out << "HTTP/1.1 401 Unauthorized\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Content-Length: " << std::strlen(kUnauthorizedBody) << "\r\n"
            << "Connection: close\r\n\r\n"
            << kUnauthorizedBody;
        conn.response = out.str();
        conn.queued = true;
        return true;
    }

    conn.response = route(conn.recv, now_ms);
    conn.queued = true;
    return true;
}

std::string DashboardResponder::route(const std::string& request,
                                      std::uint64_t now_ms) const {
    (void)now_ms;
    std::ostringstream out;
    if (request.rfind("GET / ", 0) == 0
        || request.rfind("GET / HTTP", 0) == 0) {
        out << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: text/html; charset=utf-8\r\n"
            << "Content-Length: " << std::strlen(kDashboardHtml) << "\r\n"
            << "Connection: close\r\n\r\n"
            << kDashboardHtml;
    } else if (request.rfind("GET /api/state", 0) == 0) {
        const std::string body = state_fn_ ? state_fn_() : "{}";
        out << "HTTP/1.1 200 OK\r\n"
            << "Content-Type: application/json\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Connection: close\r\n\r\n"
            << body;
    } else {
        out << "HTTP/1.1 404 Not Found\r\n"
            << "Content-Type: text/plain; charset=utf-8\r\n"
            << "Content-Length: " << std::strlen(kNotFoundBody) << "\r\n"
            << "Connection: close\r\n\r\n"
            << kNotFoundBody;
    }
    return out.str();
}

bool DashboardResponder::write_out(Conn& conn) {
    const std::uint8_t* data = reinterpret_cast<const std::uint8_t*>(
                                   conn.response.data()) +
                               conn.sent;
    const std::size_t rest = conn.response.size() - conn.sent;
    const int n = conn.transport->write(data, rest);
    if (n < 0) {
        return false;
    }
    conn.sent += static_cast<std::size_t>(n);
    return true;
}

}  // namespace world_app

#endif  // !_WIN32
