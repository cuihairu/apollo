#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include <vector>
#include <map>
#include <sstream>
#include <cctype>
#include <cstdint>

namespace apollo {
namespace net {
namespace http {

//==============================================================================
// HTTP 方法
//==============================================================================

enum class Method {
    Get,
    Head,
    Post,
    Put,
    Delete,
    Connect,
    Options,
    Trace,
    Patch
};

inline const char* toString(Method method) {
    switch (method) {
        case Method::Get:     return "GET";
        case Method::Head:    return "HEAD";
        case Method::Post:    return "POST";
        case Method::Put:     return "PUT";
        case Method::Delete:  return "DELETE";
        case Method::Connect: return "CONNECT";
        case Method::Options: return "OPTIONS";
        case Method::Trace:   return "TRACE";
        case Method::Patch:   return "PATCH";
    }
    return "UNKNOWN";
}

inline Method fromString(const std::string& method) {
    if (method == "GET")     return Method::Get;
    if (method == "HEAD")    return Method::Head;
    if (method == "POST")    return Method::Post;
    if (method == "PUT")     return Method::Put;
    if (method == "DELETE")  return Method::Delete;
    if (method == "CONNECT") return Method::Connect;
    if (method == "OPTIONS") return Method::Options;
    if (method == "TRACE")   return Method::Trace;
    if (method == "PATCH")   return Method::Patch;
    return Method::Get;
}

//==============================================================================
// HTTP 状态码
//==============================================================================

enum class StatusCode {
    // 1xx Informational
    Continue = 100,
    SwitchingProtocols = 101,
    Processing = 102,

    // 2xx Success
    OK = 200,
    Created = 201,
    Accepted = 202,
    NonAuthoritativeInformation = 203,
    NoContent = 204,
    ResetContent = 205,
    PartialContent = 206,

    // 3xx Redirection
    MultipleChoices = 300,
    MovedPermanently = 301,
    Found = 302,
    SeeOther = 303,
    NotModified = 304,
    UseProxy = 305,
    TemporaryRedirect = 307,
    PermanentRedirect = 308,

    // 4xx Client Error
    BadRequest = 400,
    Unauthorized = 401,
    PaymentRequired = 402,
    Forbidden = 403,
    NotFound = 404,
    MethodNotAllowed = 405,
    NotAcceptable = 406,
    ProxyAuthenticationRequired = 407,
    RequestTimeout = 408,
    Conflict = 409,
    Gone = 410,
    LengthRequired = 411,
    PreconditionFailed = 412,
    PayloadTooLarge = 413,
    URITooLong = 414,
    UnsupportedMediaType = 415,
    RangeNotSatisfiable = 416,
    ExpectationFailed = 417,
    ImATeapot = 418,
    UnprocessableEntity = 422,
    TooManyRequests = 429,

    // 5xx Server Error
    InternalServerError = 500,
    NotImplemented = 501,
    BadGateway = 502,
    ServiceUnavailable = 503,
    GatewayTimeout = 504,
    HTTPVersionNotSupported = 505
};

inline const char* toString(StatusCode status) {
    switch (status) {
        case StatusCode::OK: return "OK";
        case StatusCode::Created: return "Created";
        case StatusCode::Accepted: return "Accepted";
        case StatusCode::NoContent: return "No Content";
        case StatusCode::MovedPermanently: return "Moved Permanently";
        case StatusCode::Found: return "Found";
        case StatusCode::NotFound: return "Not Found";
        case StatusCode::BadRequest: return "Bad Request";
        case StatusCode::Unauthorized: return "Unauthorized";
        case StatusCode::Forbidden: return "Forbidden";
        case StatusCode::MethodNotAllowed: return "Method Not Allowed";
        case StatusCode::RequestTimeout: return "Request Timeout";
        case StatusCode::Conflict: return "Conflict";
        case StatusCode::Gone: return "Gone";
        case StatusCode::TooManyRequests: return "Too Many Requests";
        case StatusCode::InternalServerError: return "Internal Server Error";
        case StatusCode::NotImplemented: return "Not Implemented";
        case StatusCode::ServiceUnavailable: return "Service Unavailable";
        case StatusCode::BadGateway: return "Bad Gateway";
        case StatusCode::GatewayTimeout: return "Gateway Timeout";
        default: return "Unknown";
    }
}

inline int toInt(StatusCode status) {
    return static_cast<int>(status);
}

//==============================================================================
// HTTP 版本
//==============================================================================

struct Version {
    uint8_t major = 1;
    uint8_t minor = 1;

    std::string toString() const {
        return "HTTP/" + std::to_string(major) + "." + std::to_string(minor);
    }

    static Version HTTP_1_0() { return {1, 0}; }
    static Version HTTP_1_1() { return {1, 1}; }
    static Version HTTP_2_0() { return {2, 0}; }
};

//==============================================================================
// HTTP 头部
//==============================================================================

struct Headers {
    using Map = std::unordered_map<std::string, std::string, StringHashIgnoreCase, StringEqualIgnoreCase>;

    Map headers;

    void set(const std::string& key, const std::string& value) {
        headers[key] = value;
    }

    std::string get(const std::string& key, const std::string& defaultValue = "") const {
        auto it = headers.find(key);
        return it != headers.end() ? it->second : defaultValue;
    }

    bool has(const std::string& key) const {
        return headers.find(key) != headers.end();
    }

    void remove(const std::string& key) {
        headers.erase(key);
    }

    void clear() {
        headers.clear();
    }

    size_t size() const {
        return headers.size();
    }

    bool empty() const {
        return headers.empty();
    }

    Map::const_iterator begin() const { return headers.begin(); }
    Map::const_iterator end() const { return headers.end(); }

    // 常用头部辅助方法
    std::string contentType() const { return get("Content-Type"); }
    void setContentType(const std::string& type) { set("Content-Type", type); }

    int64_t contentLength() const {
        auto len = get("Content-Length");
        return len.empty() ? -1 : std::stoll(len);
    }
    void setContentLength(int64_t len) { set("Content-Length", std::to_string(len)); }

    std::string transferEncoding() const { return get("Transfer-Encoding"); }
    void setTransferEncoding(const std::string& encoding) { set("Transfer-Encoding", encoding); }

    std::string connection() const { return get("Connection"); }
    void setConnection(const std::string& value) { set("Connection", value); }

    std::string host() const { return get("Host"); }
    void setHost(const std::string& value) { set("Host", value); }

    std::string userAgent() const { return get("User-Agent"); }
    void setUserAgent(const std::string& value) { set("User-Agent", value); }

private:
    // 不区分大小写的哈希和相等比较
    struct StringHashIgnoreCase {
        size_t operator()(const std::string& key) const {
            size_t hash = 0;
            for (char c : key) {
                hash = hash * 31 + std::tolower(static_cast<unsigned char>(c));
            }
            return hash;
        }
    };

    struct StringEqualIgnoreCase {
        bool operator()(const std::string& a, const std::string& b) const {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); ++i) {
                if (std::tolower(static_cast<unsigned char>(a[i])) !=
                    std::tolower(static_cast<unsigned char>(b[i]))) {
                    return false;
                }
            }
            return true;
        }
    };
};

//==============================================================================
// HTTP 请求
//==============================================================================

struct Request {
    Method method = Method::Get;
    std::string uri;
    Version version = Version::HTTP_1_1();
    Headers headers;
    std::string body;

    // 解析后的查询参数
    std::unordered_map<std::string, std::string> queryParams;

    // 路径参数（用于路由匹配）
    std::unordered_map<std::string, std::string> pathParams;

    // 客户端信息
    std::string remoteAddress;
    uint16_t remotePort = 0;

    Request() = default;

    // 获取完整的 URL
    std::string fullPath() const {
        if (queryParams.empty()) {
            return uri;
        }
        std::string path = uri;
        path += "?";
        bool first = true;
        for (const auto& [key, value] : queryParams) {
            if (!first) path += "&";
            path += key + "=" + value;
            first = false;
        }
        return path;
    }

    // 获取查询参数
    std::string getQuery(const std::string& key, const std::string& defaultValue = "") const {
        auto it = queryParams.find(key);
        return it != queryParams.end() ? it->second : defaultValue;
    }

    int64_t getQueryAsInt(const std::string& key, int64_t defaultValue = 0) const {
        auto val = getQuery(key);
        return val.empty() ? defaultValue : std::stoll(val);
    }

    bool hasQuery(const std::string& key) const {
        return queryParams.find(key) != queryParams.end();
    }

    // 保持连接
    bool shouldKeepAlive() const {
        auto conn = headers.connection();
        return conn.empty() || conn == "keep-alive";
    }
};

//==============================================================================
// HTTP 响应
//==============================================================================

struct Response {
    StatusCode status = StatusCode::OK;
    Version version = Version::HTTP_1_1();
    Headers headers;
    std::string body;

    Response() = default;

    Response(StatusCode code, const std::string& body = "")
        : status(code), body(body) {}

    // 设置响应体
    void setBody(const std::string& content, const std::string& contentType = "text/plain") {
        body = content;
        headers.setContentType(contentType);
        headers.setContentLength(body.size());
    }

    // 设置 JSON 响应
    void setJson(const std::string& json) {
        setBody(json, "application/json");
    }

    // 设置 HTML 响应
    void setHtml(const std::string& html) {
        setBody(html, "text/html");
    }

    // 重定向
    void redirect(const std::string& url, StatusCode code = StatusCode::Found) {
        status = code;
        headers.set("Location", url);
    }

    // 序列化为 HTTP 响应字符串
    std::string toString() const {
        std::ostringstream oss;

        // 状态行
        oss << version.toString() << " " << static_cast<int>(status)
            << " " << toString(status) << "\r\n";

        // 头部
        for (const auto& [key, value] : headers.headers) {
            oss << key << ": " << value << "\r\n";
        }

        // 空行
        oss << "\r\n";

        // 消息体
        oss << body;

        return oss.str();
    }

    // 创建常用响应
    static Response ok(const std::string& body = "") {
        Response resp(StatusCode::OK, body);
        if (!body.empty()) {
            resp.headers.setContentLength(body.size());
        }
        return resp;
    }

    static Response notFound(const std::string& body = "Not Found") {
        Response resp(StatusCode::NotFound, body);
        resp.headers.setContentLength(body.size());
        return resp;
    }

    static Response badRequest(const std::string& body = "Bad Request") {
        Response resp(StatusCode::BadRequest, body);
        resp.headers.setContentLength(body.size());
        return resp;
    }

    static Response serverError(const std::string& body = "Internal Server Error") {
        Response resp(StatusCode::InternalServerError, body);
        resp.headers.setContentLength(body.size());
        return resp;
    }

    static Response json(const std::string& json) {
        Response resp(StatusCode::OK, json);
        resp.headers.setContentType("application/json");
        resp.headers.setContentLength(json.size());
        return resp;
    }
};

//==============================================================================
// HTTP 请求解析器
//==============================================================================

class RequestParser {
public:
    // 解析 HTTP 请求
    static bool parse(const std::string& data, Request& request, size_t& bytesConsumed) {
        bytesConsumed = 0;
        request = Request();

        std::istringstream iss(data);
        std::string line;

        // 解析请求行
        if (!std::getline(iss, line)) {
            return false; // 不完整
        }

        // 移除 \r
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        size_t pos = 0;
        size_t end;

        // 方法
        end = line.find(' ', pos);
        if (end == std::string::npos) return false;
        request.method = fromString(line.substr(pos, end - pos));
        pos = end + 1;

        // URI
        end = line.find(' ', pos);
        if (end == std::string::npos) return false;
        request.uri = line.substr(pos, end - pos);
        pos = end + 1;

        // 版本
        request.version.major = 1;
        request.version.minor = 1;

        // 解析查询参数
        size_t queryPos = request.uri.find('?');
        if (queryPos != std::string::npos) {
            std::string queryString = request.uri.substr(queryPos + 1);
            request.uri = request.uri.substr(0, queryPos);
            parseQuery(queryString, request.queryParams);
        }

        bytesConsumed += line.size() + 2; // +2 for \r\n

        // 解析头部
        bool headersComplete = false;
        int64_t contentLength = -1;

        while (std::getline(iss, line)) {
            bytesConsumed += line.size() + 2;

            // 移除 \r
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            // 空行表示头部结束
            if (line.empty()) {
                headersComplete = true;
                break;
            }

            // 头部字段
            size_t colonPos = line.find(':');
            if (colonPos != std::string::npos) {
                std::string key = line.substr(0, colonPos);
                std::string value = line.substr(colonPos + 1);

                // 去除首尾空白
                size_t start = value.find_first_not_of(" \t");
                size_t end2 = value.find_last_not_of(" \t");
                if (start != std::string::npos && end2 != std::string::npos) {
                    value = value.substr(start, end2 - start + 1);
                }

                request.headers.set(key, value);

                if (key == "Content-Length") {
                    contentLength = std::stoll(value);
                }
            }
        }

        if (!headersComplete) {
            return false; // 不完整
        }

        // 读取消息体
        if (contentLength > 0) {
            size_t headerEnd = bytesConsumed;
            if (data.size() >= headerEnd + contentLength) {
                request.body = data.substr(headerEnd, contentLength);
                bytesConsumed += contentLength;
            } else {
                return false; // 不完整
            }
        }

        return true;
    }

private:
    static void parseQuery(const std::string& query, std::unordered_map<std::string, std::string>& params) {
        size_t start = 0;
        while (start < query.size()) {
            size_t pos = query.find('&', start);
            std::string pair = query.substr(start, pos - start);

            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = pair.substr(0, eqPos);
                std::string value = pair.substr(eqPos + 1);
                params[key] = value;
            } else {
                params[pair] = "";
            }

            start = (pos == std::string::npos) ? std::string::npos : pos + 1;
        }
    }
};

//==============================================================================
// 请求处理器
//==============================================================================

using RequestHandler = std::function<Response(const Request&)>;

// 路由匹配器
class Router {
public:
    // 添加路由
    void addRoute(const std::string& path, Method method, RequestHandler handler) {
        routes_.emplace_back(path, method, std::move(handler));
    }

    void get(const std::string& path, RequestHandler handler) {
        addRoute(path, Method::Get, std::move(handler));
    }

    void post(const std::string& path, RequestHandler handler) {
        addRoute(path, Method::Post, std::move(handler));
    }

    void put(const std::string& path, RequestHandler handler) {
        addRoute(path, Method::Put, std::move(handler));
    }

    void del(const std::string& path, RequestHandler handler) {
        addRoute(path, Method::Delete, std::move(handler));
    }

    // 路由请求
    Response route(const Request& request) {
        for (const auto& route : routes_) {
            if (route.method == request.method && matchPath(route.path, request.uri)) {
                return route.handler(request);
            }
        }
        return Response::notFound();
    }

private:
    struct Route {
        std::string path;
        Method method;
        RequestHandler handler;

        Route(const std::string& p, Method m, RequestHandler h)
            : path(p), method(m), handler(std::move(h)) {}
    };

    std::vector<Route> routes_;

    static bool matchPath(const std::string& pattern, const std::string& path) {
        // 简单匹配，支持 * 通配符
        if (pattern == path) return true;
        if (pattern == "*") return true;
        if (pattern.back() == '*' && path.size() >= pattern.size() - 1) {
            return path.substr(0, pattern.size() - 1) == pattern.substr(0, pattern.size() - 1);
        }
        return false;
    }
};

} // namespace http
} // namespace net
} // namespace apollo
