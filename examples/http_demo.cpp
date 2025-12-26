/**
 * @file http_demo.cpp
 * @brief HTTP 服务器和客户端使用示例
 */

#include "apollo/net/http.h"
#include "apollo/net/listener.h"
#include "apollo/net/connector.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace apollo;
using namespace apollo::net;
using namespace apollo::net::http;

//==============================================================================
// 示例 1: 基本 HTTP 服务器
//==============================================================================

void basicHttpServer() {
    std::cout << "\n========== 示例 1: 基本 HTTP 服务器 ==========\n" << std::endl;

    // 注意: HttpServer 是 http.cpp 中的内部类
    // 这里演示如何使用路由系统

    Router router;

    // 添加路由
    router.get("/", [](const Request& req) -> Response {
        std::string html = R"(
            <!DOCTYPE html>
            <html>
            <head><title>Apollo HTTP Server</title></head>
            <body>
                <h1>Welcome to Apollo HTTP Server!</h1>
                <p>This is a lightweight HTTP server built with Apollo.</p>
                <ul>
                    <li><a href="/api/status">Status API</a></li>
                    <li><a href="/api/time">Current Time</a></li>
                    <li><a href="/api/user/123">User Info</a></li>
                </ul>
            </body>
            </html>
        )";
        Response response;
        response.setHtml(html);
        return response;
    });

    router.get("/api/status", [](const Request& req) -> Response {
        Response response;
        response.setJson(R"({"status":"ok","version":"1.0.0","uptime":12345})");
        return response;
    });

    router.get("/api/time", [](const Request& req) -> Response {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);

        Response response;
        response.setJson("{\"timestamp\":" + std::to_string(time) + "}");
        return response;
    });

    // 带路径参数的路由 (通配符匹配)
    router.get("/api/user/*", [](const Request& req) -> Response {
        std::string userId = req.uri.substr(10); // 去掉 "/api/user/"
        Response response;
        response.setJson("{\"id\":" + userId + ",\"name\":\"User " + userId + "\"}");
        return response;
    });

    // POST 请求处理
    router.post("/api/data", [](const Request& req) -> Response {
        Response response;
        response.setJson("{\"received\":" + req.body + "}");
        return response;
    });

    // 测试路由
    {
        Request testReq;
        testReq.method = Method::Get;
        testReq.uri = "/";

        Response resp = router.route(testReq);
        std::cout << "GET / -> " << static_cast<int>(resp.status) << std::endl;
    }

    {
        Request testReq;
        testReq.method = Method::Get;
        testReq.uri = "/api/user/123";

        Response resp = router.route(testReq);
        std::cout << "GET /api/user/123 -> " << resp.body << std::endl;
    }
}

//==============================================================================
// 示例 2: HTTP 请求构建
//==============================================================================

void httpRequestBuilding() {
    std::cout << "\n========== 示例 2: HTTP 请求构建 ==========\n" << std::endl;

    // 构建请求
    Request request;
    request.method = Method::Post;
    request.uri = "/api/users";
    request.version = Version::HTTP_1_1();

    // 设置头部
    request.headers.setContentType("application/json");
    request.headers.setContentLength(25);
    request.headers.set("Authorization", "Bearer token123");
    request.headers.setUserAgent("ApolloHTTP/1.0");

    // 设置请求体
    request.body = R"({"name":"test","age":18})";

    // 打印请求
    std::cout << "Method: " << toString(request.method) << std::endl;
    std::cout << "URI: " << request.uri << std::endl;
    std::cout << "Content-Type: " << request.headers.contentType() << std::endl;
    std::cout << "Authorization: " << request.headers.get("Authorization") << std::endl;
    std::cout << "Body: " << request.body << std::endl;
}

//==============================================================================
// 示例 3: HTTP 响应构建
//==============================================================================

void httpResponseBuilding() {
    std::cout << "\n========== 示例 3: HTTP 响应构建 ==========\n" << std::endl;

    // 使用工厂方法创建响应
    {
        auto resp = Response::ok("Hello, World!");
        std::cout << "OK Response:\n" << resp.toString() << std::endl;
    }

    {
        auto resp = Response::notFound("Resource not found");
        std::cout << "404 Response:\n" << resp.toString() << std::endl;
    }

    {
        auto resp = Response::json(R"({"status":"success","data":123})");
        std::cout << "JSON Response:\n" << resp.toString() << std::endl;
    }

    // 自定义响应
    {
        Response resp;
        resp.status = StatusCode::Created;
        resp.headers.setContentType("application/json");
        resp.headers.set("X-Custom-Header", "CustomValue");
        resp.setJson(R"({"id":123,"name":"new resource"})");

        std::cout << "Custom Response (201 Created):\n" << resp.toString() << std::endl;
    }
}

//==============================================================================
// 示例 4: 请求解析
//==============================================================================

void requestParsing() {
    std::cout << "\n========== 示例 4: 请求解析 ==========\n" << std::endl;

    // 模拟 HTTP 请求字符串
    std::string rawRequest =
        "POST /api/users?debug=true HTTP/1.1\r\n"
        "Host: localhost:8080\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 27\r\n"
        "Authorization: Bearer token123\r\n"
        "\r\n"
        "{\"name\":\"Alice\",\"age\":25}";

    Request request;
    size_t bytesConsumed = 0;

    bool success = RequestParser::parse(rawRequest, request, bytesConsumed);

    if (success) {
        std::cout << "Parse successful!" << std::endl;
        std::cout << "Method: " << toString(request.method) << std::endl;
        std::cout << "URI: " << request.uri << std::endl;
        std::cout << "Query Params: " << request.queryParams.size() << std::endl;
        std::cout << "  debug = " << request.getQuery("debug") << std::endl;
        std::cout << "Headers: " << request.headers.size() << std::endl;
        std::cout << "Body: " << request.body << std::endl;
    } else {
        std::cout << "Parse failed!" << std::endl;
    }
}

//==============================================================================
// 示例 5: 状态码使用
//==============================================================================

void statusCodeDemo() {
    std::cout << "\n========== 示例 5: 状态码使用 ==========\n" << std::endl;

    // 常用状态码
    std::cout << "200 OK: " << toString(StatusCode::OK) << std::endl;
    std::cout << "201 Created: " << toString(StatusCode::Created) << std::endl;
    std::cout << "204 No Content: " << toString(StatusCode::NoContent) << std::endl;
    std::cout << "301 Moved Permanently: " << toString(StatusCode::MovedPermanently) << std::endl;
    std::cout << "302 Found: " << toString(StatusCode::Found) << std::endl;
    std::cout << "400 Bad Request: " << toString(StatusCode::BadRequest) << std::endl;
    std::cout << "401 Unauthorized: " << toString(StatusCode::Unauthorized) << std::endl;
    std::cout << "403 Forbidden: " << toString(StatusCode::Forbidden) << std::endl;
    std::cout << "404 Not Found: " << toString(StatusCode::NotFound) << std::endl;
    std::cout << "500 Internal Server Error: " << toString(StatusCode::InternalServerError) << std::endl;
    std::cout << "502 Bad Gateway: " << toString(StatusCode::BadGateway) << std::endl;
    std::cout << "503 Service Unavailable: " << toString(StatusCode::ServiceUnavailable) << std::endl;
}

//==============================================================================
// 示例 6: 头部操作
//==============================================================================

void headersDemo() {
    std::cout << "\n========== 示例 6: 头部操作 ==========\n" << std::endl;

    Headers headers;

    // 设置头部 (不区分大小写)
    headers.setContentType("application/json");
    headers.setContentLength(100);
    headers.set("User-Agent", "ApolloClient/1.0");
    headers.set("Authorization", "Bearer token123");

    // 获取头部 (不区分大小写)
    std::cout << "Content-Type: " << headers.contentType() << std::endl;
    std::cout << "Content-Length: " << headers.contentLength() << std::endl;
    std::cout << "USER-AGENT: " << headers.get("USER-AGENT") << std::endl;
    std::cout << "authorization: " << headers.get("authorization") << std::endl;

    // 检查头部存在
    std::cout << "Has Authorization? " << (headers.has("Authorization") ? "Yes" : "No") << std::endl;

    // 遍历所有头部
    std::cout << "\nAll headers:" << std::endl;
    for (const auto& [key, value] : headers.headers) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
}

//==============================================================================
// 示例 7: 完整的 RESTful API 模拟
//==============================================================================

void restfulApiSimulation() {
    std::cout << "\n========== 示例 7: RESTful API 模拟 ==========\n" << std::endl;

    // 简单的内存存储
    struct User {
        int id;
        std::string name;
        int age;
    };
    std::vector<User> users = {
        {1, "Alice", 25},
        {2, "Bob", 30},
        {3, "Charlie", 28}
    };

    Router api;

    // GET /users - 获取所有用户
    api.get("/users", [&](const Request& req) -> Response {
        std::string json = "[";
        for (size_t i = 0; i < users.size(); ++i) {
            json += "{\"id\":" + std::to_string(users[i].id) +
                   ",\"name\":\"" + users[i].name + "\"" +
                   ",\"age\":" + std::to_string(users[i].age) + "}";
            if (i < users.size() - 1) json += ",";
        }
        json += "]";
        return Response::json(json);
    });

    // GET /users/{id} - 获取单个用户
    api.get("/users/*", [&](const Request& req) -> Response {
        std::string idStr = req.uri.substr(8); // 去掉 "/users/"
        int id = std::stoi(idStr);

        for (const auto& user : users) {
            if (user.id == id) {
                std::string json = "{\"id\":" + std::to_string(user.id) +
                       ",\"name\":\"" + user.name + "\"" +
                       ",\"age\":" + std::to_string(user.age) + "}";
                return Response::json(json);
            }
        }
        return Response::notFound("User not found");
    });

    // POST /users - 创建用户
    api.post("/users", [&](const Request& req) -> Response {
        // 简化处理: 实际需要解析 JSON
        User newUser;
        newUser.id = users.size() + 1;
        newUser.name = "New User";
        newUser.age = 20;
        users.push_back(newUser);

        return Response::json("{\"id\":" + std::to_string(newUser.id) +
                             ",\"message\":\"User created\"}");
    });

    // 模拟 API 调用
    {
        Request req;
        req.method = Method::Get;
        req.uri = "/users";
        Response resp = api.route(req);
        std::cout << "GET /users: " << resp.body << std::endl;
    }

    {
        Request req;
        req.method = Method::Get;
        req.uri = "/users/2";
        Response resp = api.route(req);
        std::cout << "GET /users/2: " << resp.body << std::endl;
    }

    {
        Request req;
        req.method = Method::Get;
        req.uri = "/users/999";
        Response resp = api.route(req);
        std::cout << "GET /users/999: " << static_cast<int>(resp.status) << std::endl;
    }
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "==============================================" << std::endl;
    std::cout << "     Apollo HTTP 服务器/客户端示例" << std::endl;
    std::cout << "==============================================" << std::endl;

    try {
        basicHttpServer();
        httpRequestBuilding();
        httpResponseBuilding();
        requestParsing();
        statusCodeDemo();
        headersDemo();
        restfulApiSimulation();

        std::cout << "\n==============================================" << std::endl;
        std::cout << "所有示例执行完毕!" << std::endl;
        std::cout << "==============================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
