/**
 * @file config_serialization_demo.cpp
 * @brief 统一配置序列化系统演示
 *
 * 演示 JSON、YAML、INI、CSV 四种格式的序列化/反序列化
 * 以及结构体自动映射功能
 */

#include <iostream>
#include <iomanip>
#include "apollo/serialization/serializer.h"
#include "apollo/serialization/formatters/json_formatter.h"
#include "apollo/serialization/formatters/yaml_formatter.h"
#include "apollo/serialization/formatters/ini_formatter.h"
#include "apollo/serialization/formatters/csv_formatter.h"

using namespace apollo::serialization;

//==============================================================================
// 示例结构体
//==============================================================================

// 服务器配置
struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
    int threads = 4;
    bool enableLog = true;
    double timeout = 30.0;

    // 使用反射定义
    APOLLO_REFLECT(ServerConfig,
        APOLLO_FIELD(host, "服务器监听地址"),
        APOLLO_FIELD(port, "服务器端口"),
        APOLLO_FIELD(threads, "工作线程数"),
        APOLLO_FIELD(enableLog, "是否启用日志"),
        APOLLO_FIELD(timeout, "连接超时时间(秒)")
    )
};

// 数据库配置
struct DatabaseConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string username = "root";
    std::string password = "";
    std::string database = "game_db";
    int poolSize = 10;
    bool autoReconnect = true;

    APOLLO_REFLECT(DatabaseConfig,
        APOLLO_FIELD(host, "数据库主机"),
        APOLLO_FIELD(port, "数据库端口"),
        APOLLO_FIELD(username, "用户名"),
        APOLLO_FIELD(password, "密码"),
        APOLLO_FIELD(database, "数据库名"),
        APOLLO_FIELD(poolSize, "连接池大小"),
        APOLLO_FIELD(autoReconnect, "自动重连")
    )
};

// 游戏物品配置
struct ItemConfig {
    uint32_t id = 0;        // 物品ID
    std::string name;       // 物品名称
    int price = 0;          // 售价
    int sellPrice = 0;      // 回收价格
    std::string type;       // 类型
    bool stackable = true;  // 是否可堆叠

    APOLLO_REFLECT(ItemConfig,
        APOLLO_PRIMARY_KEY(id),
        APOLLO_FIELD(name, "物品名称"),
        APOLLO_FIELD(price, "售价"),
        APOLLO_FIELD(sellPrice, "回收价"),
        APOLLO_FIELD(type, "物品类型"),
        APOLLO_FIELD(stackable, "可堆叠")
    )
};

// 嵌套结构体
struct GameConfig {
    ServerConfig server;
    DatabaseConfig database;
    int maxPlayers = 1000;
    std::string version = "1.0.0";

    APOLLO_REFLECT(GameConfig,
        APOLLO_FIELD(server, "服务器配置"),
        APOLLO_FIELD(database, "数据库配置"),
        APOLLO_FIELD(maxPlayers, "最大玩家数"),
        APOLLO_FIELD(version, "版本号")
    )
};

//==============================================================================
// 示例 1: JSON 序列化
//==============================================================================

void example1_Json() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 1: JSON Serialization ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    ServerConfig config;
    config.host = "192.168.1.100";
    config.port = 9000;
    config.threads = 8;
    config.enableLog = true;
    config.timeout = 60.0;

    // 序列化为 JSON
    std::string json = toJson(config);
    std::cout << "JSON Output:" << std::endl;
    std::cout << json << std::endl;

    // 从 JSON 反序列化
    ServerConfig parsed = fromJson<ServerConfig>(json);
    std::cout << "\nParsed values:" << std::endl;
    std::cout << "  host: " << parsed.host << std::endl;
    std::cout << "  port: " << parsed.port << std::endl;
    std::cout << "  threads: " << parsed.threads << std::endl;
    std::cout << "  enableLog: " << (parsed.enableLog ? "true" : "false") << std::endl;
    std::cout << "  timeout: " << parsed.timeout << std::endl;
}

//==============================================================================
// 示例 2: YAML 序列化
//==============================================================================

void example2_Yaml() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 2: YAML Serialization ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    DatabaseConfig config;
    config.host = "db.example.com";
    config.port = 5432;
    config.username = "gameuser";
    config.password = "secret123";
    config.database = "production";
    config.poolSize = 20;
    config.autoReconnect = true;

    // 序列化为 YAML
    std::string yaml = toYaml(config);
    std::cout << "YAML Output:" << std::endl;
    std::cout << yaml << std::endl;

    // 嵌套结构
    std::cout << "\n--- Nested Structure ---" << std::endl;
    GameConfig gameConfig;
    gameConfig.server.host = "0.0.0.0";
    gameConfig.server.port = 8080;
    gameConfig.database.host = "localhost";
    gameConfig.database.port = 3306;
    gameConfig.maxPlayers = 5000;

    std::string gameYaml = toYaml(gameConfig);
    std::cout << gameYaml << std::endl;
}

//==============================================================================
// 示例 3: INI 序列化
//==============================================================================

void example3_Ini() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 3: INI Serialization ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    ServerConfig config;
    config.host = "game-server.local";
    config.port = 7777;
    config.threads = 16;
    config.enableLog = false;

    // 序列化为 INI
    std::string ini = toIni(config);
    std::cout << "INI Output:" << std::endl;
    std::cout << ini << std::endl;

    // 从 INI 解析
    std::string testIni = R"(
        # Server Configuration
        host = 127.0.0.1
        port = 8888
        threads = 32
        enableLog = yes
        timeout = 120
    )";

    ServerConfig parsed = fromIni<ServerConfig>(testIni);
    std::cout << "Parsed from INI:" << std::endl;
    std::cout << "  host = " << parsed.host << std::endl;
    std::cout << "  port = " << parsed.port << std::endl;
    std::cout << "  threads = " << parsed.threads << std::endl;
    std::cout << "  enableLog = " << (parsed.enableLog ? "yes" : "no") << std::endl;
}

//==============================================================================
// 示例 4: CSV 序列化（游戏配置表）
//==============================================================================

void example4_Csv() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 4: CSV Serialization ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 物品列表
    std::vector<ItemConfig> items = {
        {1001, "生命药水", 10, 5, "consumable", true},
        {1002, "魔法药水", 15, 7, "consumable", true},
        {2001, "铁剑", 100, 50, "weapon", false},
        {2002, "钢盾", 80, 40, "armor", false},
        {3001, "复活币", 500, 0, "special", false}
    };

    // 序列化为 CSV
    std::string csv = toCsv(items);
    std::cout << "CSV Output:" << std::endl;
    std::cout << csv << std::endl;

    // 从 CSV 解析
    auto parsed = fromCsv<ItemConfig>(csv);
    std::cout << "Parsed " << parsed.size() << " items:" << std::endl;
    for (const auto& item : parsed) {
        std::cout << "  [" << item.id << "] " << item.name
                  << " - Price: " << item.price << "g" << std::endl;
    }
}

//==============================================================================
// 示例 5: DataTable 类型安全访问
//==============================================================================

void example5_DataTable() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 5: DataTable Access ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 模拟 CSV 文件内容
    std::string csvContent = R"(id,name,price,sellPrice,type,stackable
1001,生命药水,10,5,consumable,true
1002,魔法药水,15,7,consumable,true
2001,铁剑,100,50,weapon,false
2002,钢盾,80,40,armor,false
3001,复活币,500,0,special,false
4001,黄金钥匙,200,100,key,true
5001,经验卷轴,50,0,consumable,false
6001,传说宝石,1000,500,gem,true)";

    // 创建 DataTable 并加载
    formatters::DataTable table;
    table.loadFromCsvString(csvContent);

    table.printInfo();

    // 创建类型化的表
    formatters::DataTable::Table<ItemConfig> items = table.table<ItemConfig>();

    std::cout << "\n按 ID 查询:" << std::endl;
    auto* item = items.getById(2001);
    if (item) {
        std::cout << "  [2001] " << item->name
                  << " - Type: " << item->type
                  << ", Stackable: " << (item->stackable ? "yes" : "no") << std::endl;
    }

    std::cout << "\n遍历所有物品:" << std::endl;
    for (const auto& i : items.all()) {
        std::cout << "  " << std::setw(4) << i.id << " | "
                  << std::setw(12) << i.name << " | "
                  << std::setw(8) << i.price << "g | "
                  << i.type << std::endl;
    }

    std::cout << "\n检查物品是否存在:" << std::endl;
    std::cout << "  Has 1001: " << (items.contains(1001) ? "yes" : "no") << std::endl;
    std::cout << "  Has 9999: " << (items.contains(9999) ? "yes" : "no") << std::endl;
}

//==============================================================================
// 示例 6: 反射系统
//==============================================================================

void example6_Reflection() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 6: Reflection System ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 获取类型信息
    std::cout << "Type: " << APOLLO_STRUCT_NAME(ServerConfig) << std::endl;
    std::cout << "Field count: " << APOLLO_FIELD_COUNT(ServerConfig) << std::endl;
    std::cout << "Has reflection: " << (APOLLO_HAS_REFLECT(ServerConfig) ? "yes" : "no") << std::endl;

    // 遍历字段
    std::cout << "\nFields:" << std::endl;
    ServerConfig config;
    APOLLO_FOR_EACH_FIELD_CONST(config, [&](const auto& field, const auto& obj) {
        std::cout << "  - " << field.name;
        if (field.description && field.description[0] != '\0') {
            std::cout << " (" << field.description << ")";
        }
        std::cout << " = ";

        using FieldType = typename decltype(field)::FieldType;
        if constexpr (std::is_same_v<FieldType, std::string>) {
            std::cout << "\"" << field.get(obj) << "\"";
        } else if constexpr (std::is_same_v<FieldType, bool>) {
            std::cout << (field.get(obj) ? "true" : "false");
        } else if constexpr (std::is_floating_point_v<FieldType>) {
            std::cout << field.get(obj);
        } else {
            std::cout << field.get(obj);
        }
        std::cout << std::endl;
    });

    // 动态设置字段
    std::cout << "\nDynamic field setting:" << std::endl;
    reflect::setFieldValue(config, "port", "9999");
    reflect::setFieldValue(config, "enableLog", "false");

    std::cout << "  port = " << config.port << std::endl;
    std::cout << "  enableLog = " << (config.enableLog ? "true" : "false") << std::endl;
}

//==============================================================================
// 示例 7: 格式转换
//==============================================================================

void example7_FormatConversion() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "=== Example 7: Format Conversion ===" << std::endl;
    std::cout << "========================================\n" << std::endl;

    // 原始数据
    std::vector<ItemConfig> items = {
        {7001, "龙鳞", 500, 250, "material", true},
        {7002, "魔法水晶", 300, 150, "material", true}
    };

    // CSV -> JSON 转换
    std::string csv = toCsv(items);
    std::cout << "Original CSV:" << std::endl;
    std::cout << csv << std::endl;

    auto parsedItems = fromCsv<ItemConfig>(csv);
    std::string json = toJson(parsedItems);

    std::cout << "Converted to JSON:" << std::endl;
    std::cout << json << std::endl;
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Config Serialization Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    try {
        example1_Json();
        example2_Yaml();
        example3_Ini();
        example4_Csv();
        example5_DataTable();
        example6_Reflection();
        example7_FormatConversion();

        std::cout << "\n========================================" << std::endl;
        std::cout << "=== All Examples Complete ===" << std::endl;
        std::cout << "========================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "\nError: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
