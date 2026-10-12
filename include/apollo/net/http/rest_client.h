#pragma once
//
// 出站 HTTP 客户端（net-abstraction §5.10 裁决①④，2026-10-12 交付）：
//   ① curl 进 vcpkg，rest_client 转真实现——APOLLO_CURL_STUB 桩路径删除；
//   ④ 目标白名单（SSRF 面）：出站目标必须在构建期给定的白名单内
//     （RestTemplateBuilder::allowHost/allowHosts，运行期不可追加），
//     未配置 = 全拒——「脚本可发起的出站目标必须可枚举」。条目形态：
//     精确主机名（api.example.com）/ 前导点后缀（.example.com = 其子域）/
//     IPv4 网段 CIDR（10.0.0.0/8）。判定对象是 URL 字面 host（端口与
//     userinfo 剥离、大小写归一）；重定向每一跳同守卫
//     （CURLOPT_PREREQFUNCTION，跨白名单跳转即断）。
//   （裁决③同步 API 禁场景线程直调：异步交接归 scripting-lua §8 批，
//     本面同步语义与 SQL/Redis 同族；裁决② Drogon 备选分支已删除。）

#include <string>
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <future>

namespace apollo {
namespace net {
namespace http {

//==============================================================================
// HTTP 方法枚举
//==============================================================================

// DELETE 成员不带 winnt.h 的 DELETE 宏名（Windows 经 winsock2.h 链定义
// (0x00010000L)，预处理期会改写同名源码文本——含 windows.h 的任何包含
// 顺序都炸）；wire 语义由 toString 保持 "DELETE" 字面。
enum class HttpMethod {
    GET,
    POST,
    PUT,
    DEL,
    PATCH,
    HEAD,
    OPTIONS
};

inline const char* toString(HttpMethod method) {
    switch (method) {
        case HttpMethod::GET: return "GET";
        case HttpMethod::POST: return "POST";
        case HttpMethod::PUT: return "PUT";
        case HttpMethod::DEL: return "DELETE";
        case HttpMethod::PATCH: return "PATCH";
        case HttpMethod::HEAD: return "HEAD";
        case HttpMethod::OPTIONS: return "OPTIONS";
    }
    return "UNKNOWN";
}

//==============================================================================
// HTTP 头部
//==============================================================================

using HttpHeaders = std::map<std::string, std::string>;

//==============================================================================
// HTTP 响应
//==============================================================================

struct HttpResponse {
    int statusCode = 0;              // 状态码 (200, 404, etc.)
    std::string statusMessage;       // 状态消息 (OK, Not Found, etc.)
    HttpHeaders headers;             // 响应头
    std::string body;                // 响应体
    std::string contentType;         // Content-Type
    int64_t contentLength = -1;      // Content-Length
    std::string error;               // 错误信息 (如果有)

    // 便捷方法
    bool isOk() const { return statusCode >= 200 && statusCode < 300; }
    bool isClientError() const { return statusCode >= 400 && statusCode < 500; }
    bool isServerError() const { return statusCode >= 500 && statusCode < 600; }

    // 获取头部
    std::string getHeader(const std::string& name) const {
        auto it = headers.find(name);
        return it != headers.end() ? it->second : "";
    }

    bool hasHeader(const std::string& name) const {
        return headers.find(name) != headers.end();
    }
};

//==============================================================================
// HTTP 请求实体
//==============================================================================

struct HttpEntity {
    std::string body;                // 请求体
    HttpHeaders headers;             // 请求头

    // 设置 Content-Type
    HttpEntity& setContentType(const std::string& contentType) {
        headers["Content-Type"] = contentType;
        return *this;
    }

    // 设置其他头部
    HttpEntity& setHeader(const std::string& name, const std::string& value) {
        headers[name] = value;
        return *this;
    }
};

//==============================================================================
// 请求选项
//==============================================================================

struct RequestOptions {
    uint32_t timeoutMs = 30000;         // 请求超时 (30秒)
    uint32_t connectTimeoutMs = 10000;  // 连接超时 (10秒)
    bool followRedirects = true;        // 跟随重定向
    int maxRedirects = 5;               // 最大重定向次数
    bool verifyPeer = true;             // 验证 SSL 证书
    bool verifyHost = true;             // 验证 SSL 主机名
    std::string proxy;                  // 代理地址 (http://proxy:port)
    std::string proxyUsername;          // 代理用户名
    std::string proxyPassword;          // 代理密码
    std::string userAgent = "ApolloHttpClient/1.0";  // User-Agent
};

//==============================================================================
// RestTemplate - 类似 Spring RestTemplate 的 HTTP 客户端
//==============================================================================

class RestTemplate {
public:
    RestTemplate();
    ~RestTemplate();

    // 禁止拷贝，允许移动
    RestTemplate(const RestTemplate&) = delete;
    RestTemplate& operator=(const RestTemplate&) = delete;
    RestTemplate(RestTemplate&&) noexcept;
    RestTemplate& operator=(RestTemplate&&) noexcept;

    //==========================================================================
    // 同步请求方法
    //==========================================================================

    // GET 请求
    HttpResponse get(const std::string& url);
    HttpResponse get(const std::string& url, const RequestOptions& options);
    HttpResponse get(const std::string& url, const HttpHeaders& headers);

    // POST 请求 (字符串 body)
    HttpResponse post(const std::string& url, const std::string& body);
    HttpResponse post(const std::string& url, const std::string& body,
                      const std::string& contentType);
    HttpResponse post(const std::string& url, const HttpEntity& entity);
    HttpResponse post(const std::string& url, const HttpEntity& entity,
                      const RequestOptions& options);

    // PUT 请求
    HttpResponse put(const std::string& url, const std::string& body);
    HttpResponse put(const std::string& url, const HttpEntity& entity);

    // DELETE 请求
    HttpResponse delete_(const std::string& url);
    HttpResponse delete_(const std::string& url, const std::string& body);

    // PATCH 请求
    HttpResponse patch(const std::string& url, const std::string& body);
    HttpResponse patch(const std::string& url, const HttpEntity& entity);

    // HEAD 请求
    HttpResponse head(const std::string& url);

    // OPTIONS 请求
    HttpResponse options(const std::string& url);

    // 通用请求方法
    HttpResponse request(HttpMethod method, const std::string& url);
    HttpResponse request(HttpMethod method, const std::string& url,
                        const HttpEntity& entity);
    HttpResponse request(HttpMethod method, const std::string& url,
                        const HttpEntity& entity, const RequestOptions& options);

    //==========================================================================
    // 异步请求方法
    //==========================================================================

    using Callback = std::function<void(const HttpResponse&)>;

    std::future<HttpResponse> getAsync(const std::string& url);
    std::future<HttpResponse> postAsync(const std::string& url, const std::string& body);
    std::future<HttpResponse> postAsync(const std::string& url, const HttpEntity& entity);

    void getAsync(const std::string& url, Callback callback);
    void postAsync(const std::string& url, const std::string& body, Callback callback);

    //==========================================================================
    // 便捷方法 - JSON
    //==========================================================================

    // 发送 JSON 请求
    HttpResponse postForJson(const std::string& url, const std::string& jsonBody);
    HttpResponse putForJson(const std::string& url, const std::string& jsonBody);

    // 获取并解析为 JSON
    template<typename T>
    T getForObject(const std::string& url);

    // 发送对象作为 JSON
    template<typename T>
    HttpResponse postForObject(const std::string& url, const T& obj);

    //==========================================================================
    // 便捷方法 - 表单
    //==========================================================================

    // 发送表单数据
    HttpResponse postForForm(const std::string& url,
                            const std::map<std::string, std::string>& formData);

    //==========================================================================
    // 配置方法
    //==========================================================================

    // 设置默认超时
    void setTimeout(uint32_t timeoutMs);

    // 设置默认请求头
    void setDefaultHeader(const std::string& name, const std::string& value);
    void setDefaultHeaders(const HttpHeaders& headers);

    // 设置基本认证
    void setBasicAuth(const std::string& username, const std::string& password);

    // 设置 Bearer Token
    void setBearerToken(const std::string& token);

    // 设置代理
    void setProxy(const std::string& proxy);

    // 设置 SSL 验证
    void setVerifyPeer(bool verify);
    void setVerifyHost(bool verify);

    //==========================================================================
    // 连接池管理
    //==========================================================================

    // 启用连接池
    void enableConnectionPool(bool enable = true, size_t maxConnections = 10);

    // 设置 Keep-Alive
    void setKeepAlive(bool enable, int idleTimeoutMs = 60000);

    //==========================================================================
    // 出站目标白名单（§5.10：配置面在 RestTemplateBuilder，运行期不可加）
    //==========================================================================

    // URL 是否在白名单内（未配置 = 恒 false；测试与预检共用）
    bool destinationAllowed(const std::string& url) const;

    // URL 字面 host 提取（白名单装配面用；去 scheme/userinfo/端口、小写）
    static std::string hostOf(const std::string& url);

    //==========================================================================
    // 工具方法
    //==========================================================================

    // URL 编码
    static std::string encodeUrl(const std::string& url);

    // URL 编码参数
    static std::string encodeParam(const std::string& param);

    // 构建查询字符串
    static std::string buildQueryString(
        const std::map<std::string, std::string>& params);

    // 解析 JSON 响应 (需要 JSON 库)
    template<typename T>
    static T parseJson(const std::string& json);

    // 序列化对象为 JSON
    template<typename T>
    static std::string toJson(const T& obj);

private:
    class Impl;
    Impl* impl_;

    // 白名单传递缝：仅 RestTemplateBuilder::build() 可达（§5.10 运行期不可加）
    friend class RestTemplateBuilder;
    void applyWhitelist(std::vector<std::string> hosts);
};

//==============================================================================
// RestTemplateBuilder - 构建器模式
//==============================================================================

class RestTemplateBuilder {
public:
    RestTemplateBuilder();

    // 链式配置
    RestTemplateBuilder& timeout(uint32_t timeoutMs);
    RestTemplateBuilder& connectTimeout(uint32_t timeoutMs);
    RestTemplateBuilder& followRedirects(bool follow);
    RestTemplateBuilder& maxRedirects(int max);
    RestTemplateBuilder& verifyPeer(bool verify);
    RestTemplateBuilder& verifyHost(bool verify);
    RestTemplateBuilder& proxy(const std::string& proxy);
    RestTemplateBuilder& basicAuth(const std::string& username,
                                  const std::string& password);
    RestTemplateBuilder& bearerToken(const std::string& token);
    RestTemplateBuilder& defaultHeader(const std::string& name,
                                      const std::string& value);
    RestTemplateBuilder& connectionPool(size_t maxConnections);
    RestTemplateBuilder& keepAlive(bool enable, int idleTimeoutMs = 60000);

    // 出站目标白名单条目追加（构建期封闭——build 后运行期不可加，§5.10）。
    // 条目形态：精确主机名 / 前导点后缀（.example.com = 其子域）/
    // IPv4 网段 CIDR。不配置 = 出站全拒。
    RestTemplateBuilder& allowHost(const std::string& entry);
    RestTemplateBuilder& allowHosts(const std::vector<std::string>& entries);

    // 构建
    RestTemplate build() const;

private:
    struct Config {
        uint32_t timeoutMs = 30000;
        uint32_t connectTimeoutMs = 10000;
        bool followRedirects = true;
        int maxRedirects = 5;
        bool verifyPeer = true;
        bool verifyHost = true;
        std::string proxy;
        std::string basicAuthUsername;
        std::string basicAuthPassword;
        std::string bearerToken;
        HttpHeaders defaultHeaders;
        bool enableConnectionPool = true;
        size_t maxConnections = 10;
        bool keepAlive = true;
        int keepAliveIdleTimeoutMs = 60000;
        std::vector<std::string> allowedHosts;
    } config_;

    friend class RestTemplate;
};

//==============================================================================
// 便捷全局函数（§5.10 白名单立场注记：白名单未配置 = 出站全拒——本组
// 快捷面恒拒；生产用法走 RestTemplateBuilder 配置目标后实例化）
//==============================================================================

// 快速 GET 请求
inline HttpResponse httpGet(const std::string& url) {
    RestTemplate client;
    return client.get(url);
}

// 快速 POST JSON
inline HttpResponse httpPostJson(const std::string& url, const std::string& json) {
    RestTemplate client;
    return client.postForJson(url, json);
}

// 快速 POST 表单
inline HttpResponse httpPostForm(const std::string& url,
                                const std::map<std::string, std::string>& form) {
    RestTemplate client;
    return client.postForForm(url, form);
}

} // namespace http
} // namespace net
} // namespace apollo
