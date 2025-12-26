/**
 * @file serialization_demo.cpp
 * @brief 统一序列化系统示例
 *
 * 演示内容：
 * 1. JSON 序列化/反序列化
 * 2. YAML 序列化/反序列化
 * 3. INI 序列化/反序列化
 * 4. CSV 配置表加载
 * 5. 结构体反射自动映射
 */

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/formatters/json_formatter.h"
#include "apollo/serialization/formatters/yaml_formatter.h"
#include "apollo/serialization/formatters/ini_formatter.h"
#include "apollo/serialization/formatters/csv_formatter.h"
#include "apollo/serialization/reflect.h"

#include <iostream>
#include <iomanip>

using namespace apollo::serialization;
using namespace apollo::serialization::formatters;
using namespace apollo::serialization::reflect;

//==============================================================================
// 示例结构体 - 服务器配置
//==============================================================================

/**
 * @brief 服务器配置结构
 */
struct ServerConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
    int threads = 4;
    bool enableLog = true;
    double maxConnections = 1000.0;
};

// 定义反射信息
APOLLO_REFLECT(ServerConfig,
    APOLLO_FIELD(host, "服务器监听地址"),
    APOLLO_FIELD(port, "服务器端口"),
    APOLLO_FIELD(threads, "工作线程数"),
    APOLLO_FIELD(enableLog, "是否启用日志"),
    APOLLO_FIELD(maxConnections, "最大连接数")
)

//==============================================================================
// 示例结构体 - 数据库配置
//==============================================================================

/**
 * @brief 数据库配置结构
 */
struct DatabaseConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string username = "root";
    std::string password = "";
    std::string database = "game_db";
    int poolSize = 10;
    bool autoReconnect = true;
};

APOLLO_REFLECT(DatabaseConfig,
    APOLLO_FIELD(host, "数据库主机"),
    APOLLO_FIELD(port, "数据库端口"),
    APOLLO_FIELD(username, "用户名"),
    APOLLO_FIELD(password, "密码"),
    APOLLO_FIELD(database, "数据库名"),
    APOLLO_FIELD(poolSize, "连接池大小"),
    APOLLO_FIELD(autoReconnect, "自动重连")
)

//==============================================================================
// 示例结构体 - 游戏物品配置
//==============================================================================

/**
 * @brief 物品配置（用于 CSV 表格）
 */
struct ItemConfig {
    uint32_t id = 0;              // 物品 ID
    std::string name;             // 物品名称
    int price = 0;                // 售价
    int sellPrice = 0;            // 回收价格
    std::string type;             // 类型
    int rarity = 1;               // 稀有度
    bool stackable = true;        // 是否可堆叠
    int maxStack = 99;            // 最大堆叠数
};

APOLLO_REFLECT(ItemConfig,
    APOLLO_PRIMARY_KEY(id),
    APOLLO_FIELD(name, "物品名称"),
    APOLLO_FIELD(price, "售价"),
    APOLLO_FIELD(sellPrice, "回收价"),
    APOLLO_FIELD(type, "物品类型"),
    APOLLO_FIELD(rarity, "稀有度"),
    APOLLO_FIELD(stackable, "可堆叠"),
    APOLLO_FIELD(maxStack, "最大堆叠")
)

//==============================================================================
// 示例结构体 - 嵌套配置
//==============================================================================

/**
 * @brief 完整游戏服务器配置
 */
struct GameServerConfig {
    std::string serverName = "GameServer";
    int serverId = 1;

    ServerConfig network;
    DatabaseConfig database;

    int maxPlayers = 1000;
    std::string region = "CN";
    bool enablePVP = true;
};

APOLLO_REFLECT(GameServerConfig,
    APOLLO_FIELD(serverName, "服务器名称"),
    APOLLO_FIELD(serverId, "服务器 ID"),
    APOLLO_FIELD(network, "网络配置"),
    APOLLO_FIELD(database, "数据库配置"),
    APOLLO_FIELD(maxPlayers, "最大玩家数"),
    APOLLO_FIELD(region, "服务器区域"),
    APOLLO_FIELD(enablePVP, "启用 PVP")
)

//==============================================================================
// 辅助函数
//==============================================================================

void printSeparator(const std::string& title = "") {
    std::cout << "\n========================================";
    if (!title.empty()) {
        std::cout << "\n  " << title;
    }
    std::cout << "\n========================================\n";
}

//==============================================================================
// 演示函数
//==============================================================================

/**
 * @brief JSON 序列化演示
 */
void demoJson() {
    printSeparator("JSON 序列化");

    ServerConfig config{
        .host = "127.0.0.1",
        .port = 8888,
        .threads = 8,
        .enableLog = true,
        .maxConnections = 5000.0
    };

    // 序列化为 JSON
    std::string jsonStr = toJson(config);
    std::cout << "序列化结果:\n" << jsonStr << "\n";

    // 从 JSON 反序列化
    ServerConfig parsed = fromJson<ServerConfig>(jsonStr);
    std::cout << "反序列化结果:\n";
    std::cout << "  Host: " << parsed.host << "\n";
    std::cout << "  Port: " << parsed.port << "\n";
    std::cout << "  Threads: " << parsed.threads << "\n";

    // 使用 JsonFormatter
    std::string formatted = JsonFormatter::serialize(config, {true, false, 4});
    std::cout << "\n格式化输出 (indent=4):\n" << formatted << "\n";
}

/**
 * @brief YAML 序列化演示
 */
void demoYaml() {
    printSeparator("YAML 序列化");

    // 1. 简单结构
    std::cout << "1. 简单结构:\n";
    DatabaseConfig dbConfig{
        .host = "192.168.1.100",
        .port = 5432,
        .username = "admin",
        .password = "secret123",
        .database = "mygame",
        .poolSize = 20,
        .autoReconnect = true
    };

    std::string yamlStr = toYaml(dbConfig);
    std::cout << yamlStr << "\n";

    // 2. 嵌套结构
    std::cout << "2. 嵌套结构:\n";
    GameServerConfig serverConfig{
        .serverName = "TestServer",
        .serverId = 999,
        .network = {"0.0.0.0", 7777, 8},
        .database = {"db.local", 3306, "root", "", "test_db", 5, true},
        .maxPlayers = 100,
        .region = "CN",
        .enablePVP = false
    };

    std::string nestedYaml = toYaml(serverConfig);
    std::cout << nestedYaml << "\n";

#ifdef APOLLO_YAML_ENABLED
    // 3. 反序列化测试
    std::cout << "3. 反序列化 (yaml-cpp 已启用):\n";
    std::string testYaml = R"(host: 127.0.0.1
port: 3306
username: testuser
password: testpass
database: testdb
poolSize: 10
autoReconnect: false
)";

    try {
        DatabaseConfig parsed = fromYaml<DatabaseConfig>(testYaml);
        std::cout << "  Host: " << parsed.host << ", Port: " << parsed.port << "\n";
        std::cout << "  Database: " << parsed.database << ", PoolSize: " << parsed.poolSize << "\n";
    } catch (const std::exception& e) {
        std::cout << "  反序列化失败: " << e.what() << "\n";
    }
#else
    std::cout << "3. 反序列化 (需要 yaml-cpp 库)\n";
#endif

    std::cout << "\n使用说明:\n";
    std::cout << "  - 适合配置文件，可读性好\n";
    std::cout << "  - 使用 .yml 或 .yaml 扩展名\n";
    std::cout << "  - 自动格式: loadFile<Config>(\"config.yaml\")\n";
}

/**
 * @brief INI 序列化演示
 */
void demoIni() {
    printSeparator("INI 序列化");

    ServerConfig config{
        .host = "0.0.0.0",
        .port = 7777,
        .threads = 16,
        .enableLog = false,
        .maxConnections = 10000.0
    };

    // 序列化为 INI
    std::string iniStr = toIni(config);
    std::cout << "INI 输出:\n" << iniStr << "\n";

    // 从 INI 反序列化
    ServerConfig parsed = fromIni<ServerConfig>(iniStr);
    std::cout << "反序列化结果:\n";
    std::cout << "  Host: " << parsed.host << "\n";
    std::cout << "  Port: " << parsed.port << "\n";
    std::cout << "  Threads: " << parsed.threads << "\n";
    std::cout << "  EnableLog: " << (parsed.enableLog ? "true" : "false") << "\n";
}

/**
 * @brief CSV 配置表演示
 */
void demoCsv() {
    printSeparator("CSV 配置表");

    // 创建示例物品数据
    std::vector<ItemConfig> items = {
        {1001, "生命药水", 50, 25, "consumable", 1, true, 99},
        {1002, "魔法药水", 50, 25, "consumable", 1, true, 99},
        {1003, "铁剑", 200, 100, "weapon", 2, true, 1},
        {1004, "钢盾", 150, 75, "armor", 2, true, 1},
        {2001, "传奇之剑", 5000, 2500, "weapon", 5, false, 1},
        {2002, "龙鳞甲", 8000, 4000, "armor", 5, false, 1},
    };

    // 序列化为 CSV
    std::string csvStr = toCsv(items, true);
    std::cout << "CSV 输出:\n" << csvStr << "\n";

    // 从 CSV 反序列化
    std::vector<ItemConfig> parsed = fromCsv<ItemConfig>(csvStr);
    std::cout << "解析了 " << parsed.size() << " 个物品:\n";
    for (const auto& item : parsed) {
        std::cout << "  [" << item.id << "] " << item.name
                  << " - " << item.type << " (稀有度:" << item.rarity << ")\n";
    }
}

/**
 * @brief DataTable 类型安全访问演示
 */
void demoDataTable() {
    printSeparator("DataTable 类型安全访问");

    // CSV 内容
    std::string csvContent = R"(id,name,price,sellPrice,type,rarity,stackable,maxStack
1001,生命药水,50,25,consumable,1,1,99
1002,魔法药水,50,25,consumable,1,1,99
1003,铁剑,200,100,weapon,2,1,1
1004,钢盾,150,75,armor,2,1,1
2001,传奇之剑,5000,2500,weapon,5,0,1
2002,龙鳞甲,8000,4000,armor,5,0,1
3001,经验书,100,50,consumable,2,1,99
)";

    // 加载到 DataTable
    DataTable table;
    if (table.loadFromCsvString(csvContent)) {
        std::cout << "成功加载 CSV 表\n";
        std::cout << "  行数: " << table.rowCount() << "\n";
        std::cout << "  列数: " << table.columnCount() << "\n";
        std::cout << "  表头: ";
        for (const auto& h : table.headers()) {
            std::cout << h << " ";
        }
        std::cout << "\n";

        // 使用类型安全的 Table 视图
        auto itemTable = table.table<ItemConfig>();

        std::cout << "\n按 ID 查找:\n";
        if (auto* item = itemTable.getById(1003)) {
            std::cout << "  [1003] " << item->name << " - 价格:" << item->price << "\n";
        }

        if (auto* item = itemTable.getById(2001)) {
            std::cout << "  [2001] " << item->name << " - 价格:" << item->price << "\n";
        }

        std::cout << "\n遍历所有武器:\n";
        for (const auto& item : itemTable.all()) {
            if (item.type == "weapon") {
                std::cout << "  [" << item.id << "] " << item.name
                          << " - 稀有度:" << item.rarity << "\n";
            }
        }
    }
}

/**
 * @brief 嵌套结构体演示
 */
void demoNestedStruct() {
    printSeparator("嵌套结构体");

    GameServerConfig config{
        .serverName = "Wonderland",
        .serverId = 100,
        .network = {
            .host = "0.0.0.0",
            .port = 9999,
            .threads = 12
        },
        .database = {
            .host = "db.example.com",
            .port = 3306,
            .username = "gameuser",
            .database = "wonderland"
        },
        .maxPlayers = 5000,
        .region = "ASIA",
        .enablePVP = true
    };

    // JSON 序列化嵌套结构
    std::string jsonStr = toJson(config);
    std::cout << "嵌套配置 JSON:\n" << jsonStr << "\n";

    // 反序列化
    GameServerConfig parsed = fromJson<GameServerConfig>(jsonStr);
    std::cout << "\n反序列化结果:\n";
    std::cout << "  服务器: " << parsed.serverName << " (ID:" << parsed.serverId << ")\n";
    std::cout << "  网络: " << parsed.network.host << ":" << parsed.network.port << "\n";
    std::cout << "  数据库: " << parsed.database.username << "@" << parsed.database.host << "\n";
    std::cout << "  区域: " << parsed.region << " | 最大玩家: " << parsed.maxPlayers << "\n";
}

/**
 * @brief 反射信息演示
 */
void demoReflection() {
    printSeparator("反射信息");

    std::cout << "ItemConfig 反射信息:\n";
    std::cout << "  类型名: " << getTypeName<ItemConfig>() << "\n";
    std::cout << "  字段数: " << fieldCount<ItemConfig> << "\n";
    std::cout << "  字段列表:\n";

    ItemConfig item{1001, "测试物品", 100, 50, "test", 3, true, 99};

    forEachField(item, [](const auto& field, const auto& obj) {
        std::cout << "    - " << field.name;
        if (field.description && field.description[0] != '\0') {
            std::cout << " (" << field.description << ")";
        }
        std::cout << " = ";

        using FieldType = typename decltype(field)::FieldType;
        if constexpr (std::is_same_v<FieldType, std::string>) {
            std::cout << "\"" << field.get(obj) << "\"";
        } else if constexpr (std::is_same_v<FieldType, bool>) {
            std::cout << (field.get(obj) ? "true" : "false");
        } else {
            std::cout << field.get(obj);
        }
        std::cout << "\n";
    });

    std::cout << "\n通过字段名获取值:\n";
    if (auto val = getFieldValue(item, "name"); val.has_value()) {
        std::cout << "  name = " << val->get() << "\n";
    }
    if (auto val = getFieldValue(item, "price"); val.has_value()) {
        std::cout << "  price = " << val->get() << "\n";
    }

    std::cout << "\n通过字段名设置值:\n";
    bool success = setFieldValue(item, "name", "修改后的物品");
    success = setFieldValue(item, "price", "999");
    std::cout << "  修改后: " << item.name << " - 价格:" << item.price << "\n";
}

/**
 * @brief 文件 I/O 演示
 */
void demoFileIo() {
    printSeparator("文件 I/O");

    // 保存到文件
    ServerConfig config{
        .host = "localhost",
        .port = 8080,
        .threads = 4,
        .enableLog = true,
        .maxConnections = 1000.0
    };

    std::string testFile = "test_config.json";

    // 写入文件
    if (saveFile(config, testFile, true)) {
        std::cout << "已保存配置到 " << testFile << "\n";

        // 从文件读取
        try {
            ServerConfig loaded = loadFile<ServerConfig>(testFile);
            std::cout << "从文件加载配置:\n";
            std::cout << "  Host: " << loaded.host << "\n";
            std::cout << "  Port: " << loaded.port << "\n";
            std::cout << "  Threads: " << loaded.threads << "\n";
        } catch (const SerializationException& e) {
            std::cout << "加载失败: " << e.what() << "\n";
        }
    }

    std::cout << "\n(注意: 实际文件操作需要文件系统权限)\n";
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════╗\n";
    std::cout << "║     Apollo 统一序列化系统演示              ║\n";
    std::cout << "║     Unified Serialization System Demo      ║\n";
    std::cout << "╚════════════════════════════════════════════╝\n";

    try {
        demoJson();
        demoYaml();
        demoIni();
        demoCsv();
        demoDataTable();
        demoNestedStruct();
        demoReflection();
        demoFileIo();

    } catch (const std::exception& e) {
        std::cerr << "\n错误: " << e.what() << "\n";
        return 1;
    }

    printSeparator();
    std::cout << "  演示完成!\n";
    printSeparator();

    return 0;
}
