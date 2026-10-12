/**
 * @file rest_client.cpp
 * @brief 基于 libcurl 的 REST 客户端实现 (类似 Spring RestTemplate)
 */

#include "apollo/net/http/rest_client.h"
#include <cstdint>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <mutex>

// libcurl 为本目标强制依赖（net-abstraction §5.10 裁决①：curl 进 vcpkg，
// APOLLO_CURL_STUB 桩路径删除——桩与真实现并存是负资产）
#include <curl/curl.h>

namespace apollo {
namespace net {
namespace http {

//==============================================================================
// libcurl 初始化
//==============================================================================

namespace detail {
    struct CurlInitializer {
        CurlInitializer() {
            curl_global_init(CURL_GLOBAL_ALL);
        }
        ~CurlInitializer() {
            curl_global_cleanup();
        }
    };

    static CurlInitializer g_curlInit;

    // URL 编码
    inline std::string urlEncode(const std::string& str) {
        CURL* curl = curl_easy_init();
        if (!curl) return str;

        char* encoded = curl_easy_escape(curl, str.c_str(), static_cast<int>(str.length()));
        std::string result;
        if (encoded) {
            result = encoded;
            curl_free(encoded);
        }
        curl_easy_cleanup(curl);
        return result;
    }

    // ---- 出站目标白名单（net-abstraction §5.10 SSRF 面）----
    // 条目三种形态：精确主机名 / 前导点后缀（子域）/ IPv4 网段 CIDR。
    // 判定对象是 URL 字面 host——域名条目不覆盖经 DNS 解析出的 IP，
    // 运行期不可追加（RestTemplateBuilder 构建期给定）。

    inline char toLowerAscii(char c) {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }

    // scheme://[userinfo@]host[:port] 取 host（去端口、去 userinfo、小写）
    inline std::string urlHost(const std::string& url) {
        static const std::string kSchemeSep = "://";
        const std::size_t scheme = url.find(kSchemeSep);
        std::size_t begin = (scheme == std::string::npos) ? 0 : scheme + kSchemeSep.size();
        std::size_t end = url.size();
        for (std::size_t i = begin; i < url.size(); ++i) {
            const char c = url[i];
            if (c == '/' || c == '?' || c == '#') {
                end = i;
                break;
            }
        }
        std::string authority = url.substr(begin, end - begin);
        const std::size_t at = authority.rfind('@');
        if (at != std::string::npos) {
            authority = authority.substr(at + 1);
        }
        if (!authority.empty() && authority.front() == '[') {  // IPv6 字面量
            const std::size_t close = authority.find(']');
            if (close != std::string::npos) {
                authority = authority.substr(0, close + 1);
            }
        } else {  // IPv4 / 域名：端口在第一个 ':' 后
            const std::size_t colon = authority.find(':');
            if (colon != std::string::npos) {
                authority = authority.substr(0, colon);
            }
        }
        for (char& c : authority) {
            c = toLowerAscii(c);
        }
        return authority;
    }

    inline bool ipv4ToU32(const std::string& host, std::uint32_t& out) {
        std::uint32_t value = 0;
        std::size_t begin = 0;
        for (int octet = 0; octet < 4; ++octet) {
            const std::size_t dot = host.find('.', begin);
            const std::size_t field_end = (dot == std::string::npos) ? host.size() : dot;
            if (begin >= field_end || field_end - begin > 3) {
                return false;
            }
            std::uint32_t n = 0;
            for (std::size_t i = begin; i < field_end; ++i) {
                const char c = host[i];
                if (c < '0' || c > '9') {
                    return false;
                }
                n = n * 10 + static_cast<std::uint32_t>(c - '0');
                if (n > 255) {
                    return false;
                }
            }
            value = (value << 8) | n;
            begin = (dot == std::string::npos) ? field_end : dot + 1;
        }
        out = value;
        return begin == host.size();
    }

    // 条目与 host 匹配：精确 / 前导点后缀 / IPv4 CIDR 网段
    inline bool hostMatchesEntry(const std::string& host, const std::string& entry) {
        std::string normalized = entry;
        for (char& c : normalized) {
            c = toLowerAscii(c);
        }
        if (normalized.empty() || host.empty()) {
            return false;
        }
        const std::size_t slash = normalized.find('/');
        if (slash != std::string::npos) {
            const std::string addr_part = normalized.substr(0, slash);
            const std::string prefix_part = normalized.substr(slash + 1);
            std::uint32_t prefix = 0;
            for (const char c : prefix_part) {
                if (c < '0' || c > '9') {
                    return false;
                }
                prefix = prefix * 10 + static_cast<std::uint32_t>(c - '0');
                if (prefix > 32) {
                    return false;
                }
            }
            std::uint32_t network = 0;
            std::uint32_t address = 0;
            if (!ipv4ToU32(addr_part, network) || !ipv4ToU32(host, address)) {
                return false;
            }
            const std::uint32_t mask = (prefix == 0) ? 0u : (~0u << (32 - prefix));
            return (network & mask) == (address & mask);
        }
        if (normalized.front() == '.') {
            // 前导点 = 子域（自然带标签边界，"notexample.com" 不中 ".example.com"）
            return host.size() >= normalized.size() &&
                   host.compare(host.size() - normalized.size(), normalized.size(),
                                normalized) == 0;
        }
        return host == normalized;
    }

    // 整表白名单判定：空表 = 全拒（§5.10「目标必须可枚举」）
    inline bool hostAllowed(const std::string& url, const std::vector<std::string>& hosts) {
        if (hosts.empty()) {
            return false;
        }
        const std::string host = urlHost(url);
        for (const auto& entry : hosts) {
            if (hostMatchesEntry(host, entry)) {
                return true;
            }
        }
        return false;
    }
}

//==============================================================================
// 回调数据结构
//==============================================================================

struct CallbackData {
    std::string* output;
    HttpHeaders* headers;
    std::string* headerOutput;
};

//==============================================================================
// libcurl 回调函数
//==============================================================================

size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* output = static_cast<std::string*>(userp);
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

size_t HeaderCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string line(static_cast<char*>(contents), totalSize);

    // 解析头部
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string name = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);

        // 去除首尾空白和 \r\n
        size_t start = value.find_first_not_of(" \t\r\n");
        size_t end = value.find_last_not_of(" \t\r\n");
        if (start != std::string::npos && end != std::string::npos) {
            value = value.substr(start, end - start + 1);
        }

        HttpHeaders* headers = static_cast<HttpHeaders*>(userp);
        (*headers)[name] = value;
    }

    return totalSize;
}

// 重定向/每次连接前置面：跳转目标也须在白名单内（§5.10 SSRF 面的
// 重定向兜底——FOLLOWLOCATION 的每一跳都过本守卫）。注意 clientp 在
// 首位（curl_prereq_callback 原型，与 write/header 回调尾部序相反）。
#if LIBCURL_VERSION_NUM >= 0x075500
int prereqHostGuard(void* clientp, char* url, char*, int, int) {
    const auto* hosts = static_cast<const std::vector<std::string>*>(clientp);
    return detail::hostAllowed(url ? url : "", *hosts) ? CURL_PREREQFUNC_OK
                                                       : CURL_PREREQFUNC_ABORT;
}
#endif

//==============================================================================
// RestTemplate 实现
//==============================================================================

class RestTemplate::Impl {
public:
    Impl() : curl_(nullptr) {
        curl_ = curl_easy_init();
    }

    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
    }

    HttpResponse request(HttpMethod method, const std::string& url,
                        const HttpEntity* entity,
                        const RequestOptions* options) {

        HttpResponse response;
        response.statusCode = -1;

        if (!curl_) {
            response.error = "CURL not initialized";
            return response;
        }

        // 出站目标白名单（§5.10 SSRF 面）：连接前置判定，拒绝不发起拨号；
        // 重定向跳转由 CURLOPT_PREREQFUNCTION 前置面兜同纪律（下方）
        if (!detail::hostAllowed(url, allowedHosts_)) {
            response.error = "destination not allowed by outbound whitelist";
            return response;
        }

        CURLcode code;
        struct curl_slist* headersList = nullptr;
        std::string requestBody;

        // 重置 CURL 句柄
        curl_easy_reset(curl_);

        // 设置 URL
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());

        // 应用默认头部
        for (const auto& [name, value] : defaultHeaders_) {
            std::string headerLine = name + ": " + value;
            headersList = curl_slist_append(headersList, headerLine.c_str());
        }

        // 应用请求实体头部
        if (entity) {
            for (const auto& [name, value] : entity->headers) {
                std::string headerLine = name + ": " + value;
                headersList = curl_slist_append(headersList, headerLine.c_str());
            }
            requestBody = entity->body;
        }

        // 设置头部
        if (headersList) {
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headersList);
        }

        // 设置回调函数
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response.body);
        curl_easy_setopt(curl_, CURLOPT_HEADERFUNCTION, HeaderCallback);
        curl_easy_setopt(curl_, CURLOPT_HEADERDATA, &response.headers);

        // 应用选项
        const RequestOptions& opts = options ? *options : defaultOptions_;

        // 超时设置
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT_MS, opts.timeoutMs);
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT_MS, opts.connectTimeoutMs);

        // 重定向
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, opts.followRedirects ? 1L : 0L);
        curl_easy_setopt(curl_, CURLOPT_MAXREDIRS, opts.maxRedirects);
#if LIBCURL_VERSION_NUM >= 0x075500
        curl_easy_setopt(curl_, CURLOPT_PREREQFUNCTION, prereqHostGuard);
        curl_easy_setopt(curl_, CURLOPT_PREREQDATA, &allowedHosts_);
#else
        // 旧 curl 无前置面：白名单已配则禁跟随（跳转目标不可判 = 不跳）
        if (!allowedHosts_.empty()) {
            curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 0L);
        }
#endif

        // SSL 验证
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYPEER, opts.verifyPeer ? 1L : 0L);
        curl_easy_setopt(curl_, CURLOPT_SSL_VERIFYHOST, opts.verifyHost ? 2L : 0L);

        // 代理
        if (!opts.proxy.empty()) {
            curl_easy_setopt(curl_, CURLOPT_PROXY, opts.proxy.c_str());
            if (!opts.proxyUsername.empty()) {
                curl_easy_setopt(curl_, CURLOPT_PROXYUSERNAME, opts.proxyUsername.c_str());
                curl_easy_setopt(curl_, CURLOPT_PROXYPASSWORD, opts.proxyPassword.c_str());
            }
        }

        // User-Agent
        curl_easy_setopt(curl_, CURLOPT_USERAGENT, opts.userAgent.c_str());

        // 设置方法
        switch (method) {
            case HttpMethod::GET:
                // GET 是默认的，不需要特殊设置
                break;
            case HttpMethod::POST:
                curl_easy_setopt(curl_, CURLOPT_POST, 1L);
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, requestBody.c_str());
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, requestBody.size());
                break;
            case HttpMethod::PUT:
                curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PUT");
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, requestBody.c_str());
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, requestBody.size());
                break;
            case HttpMethod::DEL:
                curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "DELETE");
                if (!requestBody.empty()) {
                    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, requestBody.c_str());
                    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, requestBody.size());
                }
                break;
            case HttpMethod::PATCH:
                curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "PATCH");
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, requestBody.c_str());
                curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, requestBody.size());
                break;
            case HttpMethod::HEAD:
                curl_easy_setopt(curl_, CURLOPT_NOBODY, 1L);
                break;
            case HttpMethod::OPTIONS:
                curl_easy_setopt(curl_, CURLOPT_CUSTOMREQUEST, "OPTIONS");
                break;
        }

        // 设置基本认证
        if (!basicAuthUsername_.empty()) {
            curl_easy_setopt(curl_, CURLOPT_USERNAME, basicAuthUsername_.c_str());
            curl_easy_setopt(curl_, CURLOPT_PASSWORD, basicAuthPassword_.c_str());
        }

        // 设置 Bearer Token
        if (!bearerToken_.empty()) {
            std::string authHeader = "Authorization: Bearer " + bearerToken_;
            headersList = curl_slist_append(headersList, authHeader.c_str());
            curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headersList);
        }

        // 执行请求
        code = curl_easy_perform(curl_);

        // 清理头部列表
        if (headersList) {
            curl_slist_free_all(headersList);
        }

        // 处理结果
        if (code == CURLE_OK) {
            long httpCode = 0;
            curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &httpCode);
            response.statusCode = static_cast<int>(httpCode);

            // 获取 Content-Type
            response.contentType = response.getHeader("Content-Type");

            // 获取 Content-Length
            auto lenStr = response.getHeader("Content-Length");
            if (!lenStr.empty()) {
                try {
                    response.contentLength = std::stoll(lenStr);
                } catch (...) {}
            }

            // 状态消息
            response.statusMessage = getStatusMessage(response.statusCode);
        } else {
            response.error = curl_easy_strerror(code);
            response.statusCode = -1;
        }

        return response;
    }

    // 配置方法
    void setTimeout(uint32_t timeoutMs) { defaultOptions_.timeoutMs = timeoutMs; }
    void setConnectTimeout(uint32_t timeoutMs) { defaultOptions_.connectTimeoutMs = timeoutMs; }
    void setDefaultHeader(const std::string& name, const std::string& value) {
        defaultHeaders_[name] = value;
    }
    void setDefaultHeaders(const HttpHeaders& headers) { defaultHeaders_ = headers; }
    void setBasicAuth(const std::string& username, const std::string& password) {
        basicAuthUsername_ = username;
        basicAuthPassword_ = password;
    }
    void setBearerToken(const std::string& token) { bearerToken_ = token; }
    void setProxy(const std::string& proxy) { defaultOptions_.proxy = proxy; }
    void setVerifyPeer(bool verify) { defaultOptions_.verifyPeer = verify; }
    void setVerifyHost(bool verify) { defaultOptions_.verifyHost = verify; }

    void enableConnectionPool(bool enable, size_t maxConnections) {
        // 连接池配置 (在完整实现中会用到)
        (void)enable;
        (void)maxConnections;
    }

    void setKeepAlive(bool enable, int idleTimeoutMs) {
        // Keep-Alive 配置
        (void)enable;
        (void)idleTimeoutMs;
    }

private:
    static const char* getStatusMessage(int statusCode) {
        switch (statusCode) {
            case 200: return "OK";
            case 201: return "Created";
            case 202: return "Accepted";
            case 204: return "No Content";
            case 301: return "Moved Permanently";
            case 302: return "Found";
            case 304: return "Not Modified";
            case 400: return "Bad Request";
            case 401: return "Unauthorized";
            case 403: return "Forbidden";
            case 404: return "Not Found";
            case 405: return "Method Not Allowed";
            case 409: return "Conflict";
            case 500: return "Internal Server Error";
            case 502: return "Bad Gateway";
            case 503: return "Service Unavailable";
            default: return "";
        }
    }

    CURL* curl_;
    RequestOptions defaultOptions_;
    HttpHeaders defaultHeaders_;
    std::string basicAuthUsername_;
    std::string basicAuthPassword_;
    std::string bearerToken_;
    std::vector<std::string> allowedHosts_;  // 空 = 全拒（§5.10）；builder 构建期给定

public:
    // 白名单只在构建期给定（§5.10 运行期不可加）；本方法仅
    // RestTemplateBuilder::build() 可达（friend 面）。
    void applyWhitelist(std::vector<std::string> hosts) {
        allowedHosts_ = std::move(hosts);
    }

    bool destinationAllowed(const std::string& url) const {
        return detail::hostAllowed(url, allowedHosts_);
    }
};

//==============================================================================
// RestTemplate 公共接口实现
//==============================================================================

RestTemplate::RestTemplate() : impl_(new Impl()) {}

RestTemplate::~RestTemplate() {
    delete impl_;
}

RestTemplate::RestTemplate(RestTemplate&& other) noexcept
    : impl_(other.impl_) {
    other.impl_ = nullptr;
}

RestTemplate& RestTemplate::operator=(RestTemplate&& other) noexcept {
    if (this != &other) {
        delete impl_;
        impl_ = other.impl_;
        other.impl_ = nullptr;
    }
    return *this;
}

// 同步请求方法
HttpResponse RestTemplate::get(const std::string& url) {
    return impl_->request(HttpMethod::GET, url, nullptr, nullptr);
}

HttpResponse RestTemplate::get(const std::string& url, const RequestOptions& options) {
    return impl_->request(HttpMethod::GET, url, nullptr, &options);
}

HttpResponse RestTemplate::get(const std::string& url, const HttpHeaders& headers) {
    HttpEntity entity;
    entity.headers = headers;
    return impl_->request(HttpMethod::GET, url, &entity, nullptr);
}

HttpResponse RestTemplate::post(const std::string& url, const std::string& body) {
    HttpEntity entity;
    entity.body = body;
    entity.setContentType("text/plain");
    return impl_->request(HttpMethod::POST, url, &entity, nullptr);
}

HttpResponse RestTemplate::post(const std::string& url, const std::string& body,
                               const std::string& contentType) {
    HttpEntity entity;
    entity.body = body;
    entity.setContentType(contentType);
    return impl_->request(HttpMethod::POST, url, &entity, nullptr);
}

HttpResponse RestTemplate::post(const std::string& url, const HttpEntity& entity) {
    return impl_->request(HttpMethod::POST, url, &entity, nullptr);
}

HttpResponse RestTemplate::post(const std::string& url, const HttpEntity& entity,
                               const RequestOptions& options) {
    return impl_->request(HttpMethod::POST, url, &entity, &options);
}

HttpResponse RestTemplate::put(const std::string& url, const std::string& body) {
    HttpEntity entity;
    entity.body = body;
    entity.setContentType("text/plain");
    return impl_->request(HttpMethod::PUT, url, &entity, nullptr);
}

HttpResponse RestTemplate::put(const std::string& url, const HttpEntity& entity) {
    return impl_->request(HttpMethod::PUT, url, &entity, nullptr);
}

HttpResponse RestTemplate::delete_(const std::string& url) {
    return impl_->request(HttpMethod::DEL, url, nullptr, nullptr);
}

HttpResponse RestTemplate::delete_(const std::string& url, const std::string& body) {
    HttpEntity entity;
    entity.body = body;
    return impl_->request(HttpMethod::DEL, url, &entity, nullptr);
}

HttpResponse RestTemplate::patch(const std::string& url, const std::string& body) {
    HttpEntity entity;
    entity.body = body;
    entity.setContentType("application/json");
    return impl_->request(HttpMethod::PATCH, url, &entity, nullptr);
}

HttpResponse RestTemplate::patch(const std::string& url, const HttpEntity& entity) {
    return impl_->request(HttpMethod::PATCH, url, &entity, nullptr);
}

HttpResponse RestTemplate::head(const std::string& url) {
    return impl_->request(HttpMethod::HEAD, url, nullptr, nullptr);
}

HttpResponse RestTemplate::options(const std::string& url) {
    return impl_->request(HttpMethod::OPTIONS, url, nullptr, nullptr);
}

HttpResponse RestTemplate::request(HttpMethod method, const std::string& url) {
    return impl_->request(method, url, nullptr, nullptr);
}

HttpResponse RestTemplate::request(HttpMethod method, const std::string& url,
                                   const HttpEntity& entity) {
    return impl_->request(method, url, &entity, nullptr);
}

HttpResponse RestTemplate::request(HttpMethod method, const std::string& url,
                                   const HttpEntity& entity,
                                   const RequestOptions& options) {
    return impl_->request(method, url, &entity, &options);
}

// 异步请求方法
std::future<HttpResponse> RestTemplate::getAsync(const std::string& url) {
    return std::async(std::launch::async, [this, url]() {
        return get(url);
    });
}

std::future<HttpResponse> RestTemplate::postAsync(const std::string& url,
                                                  const std::string& body) {
    return std::async(std::launch::async, [this, url, body]() {
        return post(url, body);
    });
}

std::future<HttpResponse> RestTemplate::postAsync(const std::string& url,
                                                  const HttpEntity& entity) {
    return std::async(std::launch::async, [this, url, entity]() {
        return post(url, entity);
    });
}

void RestTemplate::getAsync(const std::string& url, Callback callback) {
    std::thread([this, url, callback]() {
        callback(get(url));
    }).detach();
}

void RestTemplate::postAsync(const std::string& url, const std::string& body,
                            Callback callback) {
    std::thread([this, url, body, callback]() {
        callback(post(url, body));
    }).detach();
}

// JSON 便捷方法
HttpResponse RestTemplate::postForJson(const std::string& url,
                                       const std::string& jsonBody) {
    HttpEntity entity;
    entity.body = jsonBody;
    entity.setContentType("application/json");
    return impl_->request(HttpMethod::POST, url, &entity, nullptr);
}

HttpResponse RestTemplate::putForJson(const std::string& url,
                                      const std::string& jsonBody) {
    HttpEntity entity;
    entity.body = jsonBody;
    entity.setContentType("application/json");
    return impl_->request(HttpMethod::PUT, url, &entity, nullptr);
}

// 表单便捷方法
HttpResponse RestTemplate::postForForm(
    const std::string& url,
    const std::map<std::string, std::string>& formData) {

    std::string formBody = buildQueryString(formData);
    HttpEntity entity;
    entity.body = formBody;
    entity.setContentType("application/x-www-form-urlencoded");
    return impl_->request(HttpMethod::POST, url, &entity, nullptr);
}

// 配置方法
void RestTemplate::setTimeout(uint32_t timeoutMs) {
    impl_->setTimeout(timeoutMs);
}

void RestTemplate::setDefaultHeader(const std::string& name, const std::string& value) {
    impl_->setDefaultHeader(name, value);
}

void RestTemplate::setDefaultHeaders(const HttpHeaders& headers) {
    impl_->setDefaultHeaders(headers);
}

void RestTemplate::setBasicAuth(const std::string& username, const std::string& password) {
    impl_->setBasicAuth(username, password);
}

void RestTemplate::setBearerToken(const std::string& token) {
    impl_->setBearerToken(token);
}

void RestTemplate::setProxy(const std::string& proxy) {
    impl_->setProxy(proxy);
}

void RestTemplate::setVerifyPeer(bool verify) {
    impl_->setVerifyPeer(verify);
}

void RestTemplate::setVerifyHost(bool verify) {
    impl_->setVerifyHost(verify);
}

void RestTemplate::enableConnectionPool(bool enable, size_t maxConnections) {
    impl_->enableConnectionPool(enable, maxConnections);
}

void RestTemplate::setKeepAlive(bool enable, int idleTimeoutMs) {
    impl_->setKeepAlive(enable, idleTimeoutMs);
}

// 出站目标白名单（§5.10：判定面公开可预检，配置面仅 builder）
bool RestTemplate::destinationAllowed(const std::string& url) const {
    return impl_->destinationAllowed(url);
}

void RestTemplate::applyWhitelist(std::vector<std::string> hosts) {
    impl_->applyWhitelist(std::move(hosts));
}

std::string RestTemplate::hostOf(const std::string& url) {
    return detail::urlHost(url);
}

// 工具方法
std::string RestTemplate::encodeUrl(const std::string& url) {
    return detail::urlEncode(url);
}

std::string RestTemplate::encodeParam(const std::string& param) {
    return detail::urlEncode(param);
}

std::string RestTemplate::buildQueryString(
    const std::map<std::string, std::string>& params) {

    std::ostringstream oss;
    bool first = true;

    for (const auto& [key, value] : params) {
        if (!first) oss << "&";
        oss << detail::urlEncode(key) << "=" << detail::urlEncode(value);
        first = false;
    }

    return oss.str();
}

//==============================================================================
// RestTemplateBuilder 实现
//==============================================================================

RestTemplateBuilder::RestTemplateBuilder() = default;

RestTemplateBuilder& RestTemplateBuilder::timeout(uint32_t timeoutMs) {
    config_.timeoutMs = timeoutMs;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::connectTimeout(uint32_t timeoutMs) {
    config_.connectTimeoutMs = timeoutMs;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::followRedirects(bool follow) {
    config_.followRedirects = follow;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::maxRedirects(int max) {
    config_.maxRedirects = max;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::verifyPeer(bool verify) {
    config_.verifyPeer = verify;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::verifyHost(bool verify) {
    config_.verifyHost = verify;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::proxy(const std::string& proxy) {
    config_.proxy = proxy;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::basicAuth(const std::string& username,
                                                     const std::string& password) {
    config_.basicAuthUsername = username;
    config_.basicAuthPassword = password;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::bearerToken(const std::string& token) {
    config_.bearerToken = token;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::defaultHeader(const std::string& name,
                                                        const std::string& value) {
    config_.defaultHeaders[name] = value;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::connectionPool(size_t maxConnections) {
    config_.enableConnectionPool = true;
    config_.maxConnections = maxConnections;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::keepAlive(bool enable, int idleTimeoutMs) {
    config_.keepAlive = enable;
    config_.keepAliveIdleTimeoutMs = idleTimeoutMs;
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::allowHost(const std::string& entry) {
    config_.allowedHosts.push_back(entry);
    return *this;
}

RestTemplateBuilder& RestTemplateBuilder::allowHosts(
    const std::vector<std::string>& entries) {
    for (const auto& entry : entries) {
        config_.allowedHosts.push_back(entry);
    }
    return *this;
}

RestTemplate RestTemplateBuilder::build() const {
    RestTemplate client;

    // 白名单先于其余配置（§5.10 SSRF 面：构建期封闭，运行期不可加）
    client.applyWhitelist(config_.allowedHosts);

    // 应用配置
    client.setTimeout(config_.timeoutMs);

    for (const auto& [name, value] : config_.defaultHeaders) {
        client.setDefaultHeader(name, value);
    }

    if (!config_.basicAuthUsername.empty()) {
        client.setBasicAuth(config_.basicAuthUsername, config_.basicAuthPassword);
    }

    if (!config_.bearerToken.empty()) {
        client.setBearerToken(config_.bearerToken);
    }

    if (!config_.proxy.empty()) {
        client.setProxy(config_.proxy);
    }

    client.setVerifyPeer(config_.verifyPeer);
    client.setVerifyHost(config_.verifyHost);

    if (config_.enableConnectionPool) {
        client.enableConnectionPool(true, config_.maxConnections);
    }

    client.setKeepAlive(config_.keepAlive, config_.keepAliveIdleTimeoutMs);

    return client;
}

} // namespace http
} // namespace net
} // namespace apollo
