/**
 * @file test_serialization.cpp
 * @brief 统一序列化系统单元测试
 */

#include <iostream>
#include <cassert>
#include <vector>
#include <string>

#include "apollo/serialization/serializer.h"
#include "apollo/serialization/formatters/json_formatter.h"
#include "apollo/serialization/formatters/ini_formatter.h"
#include "apollo/serialization/formatters/csv_formatter.h"
#include "apollo/serialization/reflect.h"

using namespace apollo::serialization;
using namespace apollo::serialization::formatters;
using namespace apollo::serialization::reflect;

//==============================================================================
// 测试辅助宏
//==============================================================================

#define TEST(name) \
    do { \
        std::cout << "[TEST] " << #name << "..."; \
        test_##name(); \
        std::cout << " PASS\n"; \
    } while(0)

#define ASSERT_TRUE(cond) \
    do { \
        if (!(cond)) { \
            std::cout << " FAILED: " << #cond << "\n"; \
            std::exit(1); \
        } \
    } while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))
#define ASSERT_NEAR(a, b, eps) ASSERT_TRUE(std::abs((a) - (b)) < (eps))

//==============================================================================
// 测试结构体
//==============================================================================

struct TestStruct {
    int intValue = 0;
    std::string stringValue;
    double doubleValue = 0.0;
    bool boolValue = false;
};

APOLLO_REFLECT(TestStruct,
    APOLLO_FIELD(intValue),
    APOLLO_FIELD(stringValue),
    APOLLO_FIELD(doubleValue),
    APOLLO_FIELD(boolValue)
)

struct NestedStruct {
    std::string name;
    TestStruct inner;
    int count = 0;
};

APOLLO_REFLECT(NestedStruct,
    APOLLO_FIELD(name),
    APOLLO_FIELD(inner),
    APOLLO_FIELD(count)
)

struct ItemData {
    uint32_t id = 0;
    std::string name;
    int price = 0;
    std::string type;
};

APOLLO_REFLECT(ItemData,
    APOLLO_PRIMARY_KEY(id),
    APOLLO_FIELD(name),
    APOLLO_FIELD(price),
    APOLLO_FIELD(type)
)

//==============================================================================
// JSON 测试
//==============================================================================

void test_json_serialize_simple() {
    TestStruct obj{42, "hello", 3.14, true};

    std::string json = toJson(obj);

    // 验证包含关键字段
    ASSERT_TRUE(json.find("\"intValue\"") != std::string::npos);
    ASSERT_TRUE(json.find("42") != std::string::npos);
    ASSERT_TRUE(json.find("\"stringValue\"") != std::string::npos);
    ASSERT_TRUE(json.find("hello") != std::string::npos);
    ASSERT_TRUE(json.find("\"doubleValue\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"boolValue\"") != std::string::npos);
    ASSERT_TRUE(json.find("true") != std::string::npos);
}

void test_json_deserialize_simple() {
    std::string json = R"({
        "intValue": 100,
        "stringValue": "world",
        "doubleValue": 2.718,
        "boolValue": false
    })";

    TestStruct obj = fromJson<TestStruct>(json);

    ASSERT_EQ(obj.intValue, 100);
    ASSERT_EQ(obj.stringValue, "world");
    ASSERT_NEAR(obj.doubleValue, 2.718, 0.001);
    ASSERT_FALSE(obj.boolValue);
}

void test_json_roundtrip() {
    TestStruct original{-123, "test\r\n", -0.5, false};

    std::string json = toJson(original);
    TestStruct restored = fromJson<TestStruct>(json);

    ASSERT_EQ(restored.intValue, original.intValue);
    ASSERT_EQ(restored.stringValue, original.stringValue);
    ASSERT_NEAR(restored.doubleValue, original.doubleValue, 0.001);
    ASSERT_EQ(restored.boolValue, original.boolValue);
}

void test_json_nested() {
    NestedStruct obj{
        .name = "outer",
        .inner = {123, "inner", 1.23, true},
        .count = 5
    };

    std::string json = toJson(obj);

    ASSERT_TRUE(json.find("\"name\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"inner\"") != std::string::npos);
    ASSERT_TRUE(json.find("\"count\"") != std::string::npos);
}

//==============================================================================
// INI 测试
//==============================================================================

void test_ini_serialize() {
    TestStruct obj{42, "hello", 3.14, true};

    std::string ini = toIni(obj);

    ASSERT_TRUE(ini.find("intValue") != std::string::npos);
    ASSERT_TRUE(ini.find("42") != std::string::npos);
    ASSERT_TRUE(ini.find("stringValue") != std::string::npos);
    ASSERT_TRUE(ini.find("hello") != std::string::npos);
}

void test_ini_deserialize() {
    std::string ini = R"(intValue = 999
stringValue = test_ini
doubleValue = 1.414
boolValue = true
)";

    TestStruct obj = fromIni<TestStruct>(ini);

    ASSERT_EQ(obj.intValue, 999);
    ASSERT_EQ(obj.stringValue, "test_ini");
    ASSERT_NEAR(obj.doubleValue, 1.414, 0.001);
    ASSERT_TRUE(obj.boolValue);
}

void test_ini_roundtrip() {
    TestStruct original{777, "roundtrip", 6.28, false};

    std::string ini = toIni(original);
    TestStruct restored = fromIni<TestStruct>(ini);

    ASSERT_EQ(restored.intValue, original.intValue);
    ASSERT_EQ(restored.stringValue, original.stringValue);
    ASSERT_NEAR(restored.doubleValue, original.doubleValue, 0.01);
}

//==============================================================================
// CSV 测试
//==============================================================================

void test_csv_serialize() {
    std::vector<ItemData> items = {
        {1, "Sword", 100, "weapon"},
        {2, "Shield", 80, "armor"},
        {3, "Potion", 20, "consumable"}
    };

    std::string csv = toCsv(items, true);

    ASSERT_TRUE(csv.find("id") != std::string::npos);  // header
    ASSERT_TRUE(csv.find("name") != std::string::npos);
    ASSERT_TRUE(csv.find("Sword") != std::string::npos);
    ASSERT_TRUE(csv.find("Shield") != std::string::npos);
    ASSERT_TRUE(csv.find("Potion") != std::string::npos);
}

void test_csv_deserialize() {
    std::string csv = R"(id,name,price,type
1,Apple,10,food
2,Sword,100,weapon
3,Shield,80,armor
)";

    std::vector<ItemData> items = fromCsv<ItemData>(csv);

    ASSERT_EQ(items.size(), 3);
    ASSERT_EQ(items[0].id, 1);
    ASSERT_EQ(items[0].name, "Apple");
    ASSERT_EQ(items[0].price, 10);
    ASSERT_EQ(items[1].name, "Sword");
    ASSERT_EQ(items[2].name, "Shield");
}

void test_csv_with_quotes() {
    std::string csv = R"(id,name,price,type
1,"Sword of Destiny",150,weapon
2,"Simple Shield",50,armor
)";

    std::vector<ItemData> items = fromCsv<ItemData>(csv);

    ASSERT_EQ(items.size(), 2);
    ASSERT_EQ(items[0].name, "Sword of Destiny");
    ASSERT_EQ(items[0].price, 150);
}

void test_csv_roundtrip() {
    std::vector<ItemData> original = {
        {100, "Item1", 50, "type1"},
        {200, "Item2", 75, "type2"}
    };

    std::string csv = toCsv(original, true);
    std::vector<ItemData> restored = fromCsv<ItemData>(csv);

    ASSERT_EQ(restored.size(), original.size());
    ASSERT_EQ(restored[0].id, original[0].id);
    ASSERT_EQ(restored[0].name, original[0].name);
    ASSERT_EQ(restored[1].price, original[1].price);
}

//==============================================================================
// DataTable 测试
//==============================================================================

void test_datatable_load() {
    std::string csv = R"(id,name,price,type
1,Apple,10,food
2,Banana,5,food
3,Sword,100,weapon
)";

    DataTable table;
    ASSERT_TRUE(table.loadFromCsvString(csv));

    ASSERT_EQ(table.rowCount(), 3);
    ASSERT_EQ(table.columnCount(), 4);
    ASSERT_TRUE(table.hasColumn("id"));
    ASSERT_TRUE(table.hasColumn("name"));
}

void test_datatable_get_cell() {
    std::string csv = R"(id,name,price,type
1,Apple,10,food
2,Sword,100,weapon
)";

    DataTable table;
    table.loadFromCsvString(csv);

    auto cell = table.getCell(0, 1);  // Row 0, Col 1 (name)
    ASSERT_TRUE(cell.has_value());
    ASSERT_EQ(cell.value(), "Apple");

    auto byName = table.getCell(1, "price");
    ASSERT_TRUE(byName.has_value());
    ASSERT_EQ(byName.value(), "100");
}

void test_datatable_get_row() {
    std::string csv = R"(id,name,price,type
1,Apple,10,food
2,Sword,100,weapon
)";

    DataTable table;
    table.loadFromCsvString(csv);

    auto row = table.getRow<ItemData>(0);
    ASSERT_TRUE(row.has_value());
    ASSERT_EQ(row->id, 1);
    ASSERT_EQ(row->name, "Apple");
    ASSERT_EQ(row->price, 10);

    auto row2 = table.getRow<ItemData>(1);
    ASSERT_TRUE(row2.has_value());
    ASSERT_EQ(row2->id, 2);
    ASSERT_EQ(row2->name, "Sword");
}

void test_datatable_table_view() {
    std::string csv = R"(id,name,price,type
1,Apple,10,food
2,Sword,100,weapon
3,Shield,80,armor
)";

    DataTable table;
    table.loadFromCsvString(csv);

    auto itemView = table.table<ItemData>();

    ASSERT_EQ(itemView.size(), 3);

    auto* item = itemView.getById(2);
    ASSERT_TRUE(item != nullptr);
    ASSERT_EQ(item->name, "Sword");

    auto* notFound = itemView.getById(999);
    ASSERT_TRUE(notFound == nullptr);
}

//==============================================================================
// 反射测试
//==============================================================================

void test_reflect_field_count() {
    ASSERT_EQ(fieldCount<TestStruct>, 4);
    ASSERT_EQ(fieldCount<NestedStruct>, 3);
    ASSERT_EQ(fieldCount<ItemData>, 4);
}

void test_reflect_field_names() {
    auto names = StructInfo<TestStruct>::getFieldNames();
    ASSERT_EQ(names.size(), 4);
    ASSERT_TRUE(names[0] == "intValue" ||
                names[1] == "intValue" ||
                names[2] == "intValue" ||
                names[3] == "intValue");
}

void test_reflect_for_each() {
    TestStruct obj{100, "test", 1.5, true};

    int count = 0;
    forEachField(obj, [&](const auto& field, const auto& o) {
        (void)field;
        (void)o;
        count++;
    });

    ASSERT_EQ(count, 4);
}

void test_reflect_get_field_value() {
    TestStruct obj{42, "hello", 3.14, true};

    auto intVal = getFieldValue(obj, "intValue");
    ASSERT_TRUE(intVal.has_value());

    auto strVal = getFieldValue(obj, "stringValue");
    ASSERT_TRUE(strVal.has_value());
    ASSERT_TRUE(strVal->get().find("hello") != std::string::npos);

    auto missing = getFieldValue(obj, "notExist");
    ASSERT_FALSE(missing.has_value());
}

void test_reflect_set_field_value() {
    TestStruct obj;

    ASSERT_TRUE(setFieldValue(obj, "intValue", "123"));
    ASSERT_EQ(obj.intValue, 123);

    ASSERT_TRUE(setFieldValue(obj, "stringValue", "new value"));
    ASSERT_EQ(obj.stringValue, "new value");

    ASSERT_TRUE(setFieldValue(obj, "boolValue", "true"));
    ASSERT_TRUE(obj.boolValue);
}

//==============================================================================
// 格式检测测试
//==============================================================================

void test_detect_format() {
    ASSERT_EQ(detectFormat("config.json"), Format::Json);
    ASSERT_EQ(detectFormat("config.yaml"), Format::Yaml);
    ASSERT_EQ(detectFormat("config.yml"), Format::Yaml);
    ASSERT_EQ(detectFormat("config.ini"), Format::Ini);
    ASSERT_EQ(detectFormat("config.cfg"), Format::Ini);
    ASSERT_EQ(detectFormat("data.csv"), Format::Csv);
    ASSERT_EQ(detectFormat("unknown.txt"), Format::Json);  // 默认
}

//==============================================================================
// 字符串工具测试
//==============================================================================

void test_string_trim() {
    using namespace string_utils;

    ASSERT_EQ(trim("  hello  "), "hello");
    ASSERT_EQ(trim("\tworld\n"), "world");
    ASSERT_EQ(trim("nospace"), "nospace");
    ASSERT_EQ(trim(""), "");
    ASSERT_EQ(trim("   "), "");
}

void test_string_split() {
    using namespace string_utils;

    auto parts = split("a,b,c", ',');
    ASSERT_EQ(parts.size(), 3);
    ASSERT_EQ(parts[0], "a");
    ASSERT_EQ(parts[1], "b");
    ASSERT_EQ(parts[2], "c");
}

void test_string_to_lower() {
    using namespace string_utils;

    ASSERT_EQ(toLower("HelloWorld"), "helloworld");
    ASSERT_EQ(toLower("ABC123"), "abc123");
}

void test_string_starts_ends() {
    using namespace string_utils;

    ASSERT_TRUE(startsWith("hello world", "hello"));
    ASSERT_FALSE(startsWith("hello world", "world"));

    ASSERT_TRUE(endsWith("test.txt", ".txt"));
    ASSERT_FALSE(endsWith("test.txt", ".csv"));
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "  Serialization Tests\n";
    std::cout << "========================================\n\n";

    std::cout << "--- JSON Tests ---\n";
    TEST(json_serialize_simple);
    TEST(json_deserialize_simple);
    TEST(json_roundtrip);
    TEST(json_nested);

    std::cout << "\n--- INI Tests ---\n";
    TEST(ini_serialize);
    TEST(ini_deserialize);
    TEST(ini_roundtrip);

    std::cout << "\n--- CSV Tests ---\n";
    TEST(csv_serialize);
    TEST(csv_deserialize);
    TEST(csv_with_quotes);
    TEST(csv_roundtrip);

    std::cout << "\n--- DataTable Tests ---\n";
    TEST(datatable_load);
    TEST(datatable_get_cell);
    TEST(datatable_get_row);
    TEST(datatable_table_view);

    std::cout << "\n--- Reflection Tests ---\n";
    TEST(reflect_field_count);
    TEST(reflect_field_names);
    TEST(reflect_for_each);
    TEST(reflect_get_field_value);
    TEST(reflect_set_field_value);

    std::cout << "\n--- Format Detection Tests ---\n";
    TEST(detect_format);

    std::cout << "\n--- String Utils Tests ---\n";
    TEST(string_trim);
    TEST(string_split);
    TEST(string_to_lower);
    TEST(string_starts_ends);

    std::cout << "\n========================================\n";
    std::cout << "  All Tests Passed!\n";
    std::cout << "========================================\n";

    return 0;
}
