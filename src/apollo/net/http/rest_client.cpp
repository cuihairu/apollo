/**
 * @file rest_client.cpp
 * @brief 基于 libcurl 的 REST 客户端实现 (类似 Spring RestTemplate)
 */

#include "apollo/net/http/rest_client.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <mutex>

// 检查是否启用了 libcurl
#ifdef APOLLO_HAS_CURL
    #include <curl/curl.h>
#else
    #define APOLLO_CURL_STUB 1
#endif

namespace apollo {
namespace net {
namespace http {

//==============================================================================
// libcurl 初始化
//==============================================================================

#ifdef APOLLO_HAS_CURL

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
}

#else

namespace detail {
    // 简单的 URL 编码实现 (备用)
    inline std::string urlEncode(const std::string& str) {
        std::ostringstream encoded;
        encoded.fill('0');
        encoded << std::hex;

        for (char c : str) {
            if (std::isalnum(static_cast<unsigned char>(c)) ||
                c == '-' || c == '_' || c == '.' || c == '~') {
                encoded << c;
            } else {
                encoded << std::uppercase;
                encoded << '%' << std::setw(2) <<
                    static_cast<int>(static_cast<unsigned char>(c));
                encoded << std::nouppercase;
            }
        }

        return encoded.str();
    }
}

#endif

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

#ifdef APOLLO_HAS_CURL

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

#endif

//==============================================================================
// RestTemplate 实现
//==============================================================================

class RestTemplate::Impl {
public:
    Impl() : curl_(nullptr) {
#ifdef APOLLO_HAS_CURL
        curl_ = curl_easy_init();
#endif
    }

    ~Impl() {
#ifdef APOLLO_HAS_CURL
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
#endif
    }

    HttpResponse request(HttpMethod method, const std::string& url,
                        const HttpEntity* entity,
                        const RequestOptions* options) {

        HttpResponse response;
        response.statusCode = -1;

#ifdef APOLLO_HAS_CURL
        if (!curl_) {
            response.error = "CURL not initialized";
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
            case HttpMethod::DELETE:
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

#else
        // 没有 libcurl 时的存根实现
        response.error = "libcurl not available. Compile with APOLLO_ENABLE_CURL=ON.";
        response.statusCode = -1;
#endif

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

#ifdef APOLLO_HAS_CURL
    CURL* curl_;
#else
    void* curl_ = nullptr;
#endif
    RequestOptions defaultOptions_;
    HttpHeaders defaultHeaders_;
    std::string basicAuthUsername_;
    std::string basicAuthPassword_;
    std::string bearerToken_;
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
    return impl_->request(HttpMethod::DELETE, url, nullptr, nullptr);
}

HttpResponse RestTemplate::delete_(const std::string& url, const std::string& body) {
    HttpEntity entity;
    entity.body = body;
    return impl_->request(HttpMethod::DELETE, url, &entity, nullptr);
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

RestTemplate RestTemplateBuilder::build() const {
    RestTemplate client;

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
