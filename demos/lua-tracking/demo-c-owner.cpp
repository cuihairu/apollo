// Demo C: BigWorld 式 PropertyOwner 冒泡最小模型（C++，不依赖 apollo 构建）
// 验证：容器值作为 PropertyOwner 挂 owner/index，深度变更沿链冒泡到根 Entity，
//       根端把 path（叶→根 index 序列）翻译成属性路径 —— 只发叶子，不整表重发。

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

// 一次变更：path 是叶→根的 index 序列
struct PropertyChange {
    std::vector<int> path;
};

// 所有参与冒泡的节点共同接口
struct Owner {
    Owner* parent = nullptr;
    int index = -1;
    virtual ~Owner() = default;

    // 收到子变更：把自己的 index 压入 path，向上冒泡；到根则上报
    void bubble(PropertyChange& ch) {
        if (index >= 0) ch.path.push_back(index);
        if (parent) parent->bubble(ch);
        else report(ch);
    }
    virtual void report(const PropertyChange&) {}
};

// 根 Entity：把 path 翻译成可读的属性路径
struct Entity : Owner {
    std::vector<std::string> changes;
    void report(const PropertyChange& ch) override {
        std::string s = "entity";
        for (auto it = ch.path.rbegin(); it != ch.path.rend(); ++it)
            s += ".slot[" + std::to_string(*it) + "]";
        changes.push_back(s);
        printf("  [Entity] 深度变更: %s\n", s.c_str());
    }
};

// 容器：持有 N 个 Vector3，attach 时绑定 parent + index（评审§5.1 Ownership Metadata）
struct Vector3 : Owner { float x=0, y=0, z=0; };

struct Container : Owner {
    std::vector<std::unique_ptr<Vector3>> slots;
    explicit Container(Owner* p) { parent = p; index = -1; }  // 容器自身不入 path
    void init(int n) {
        for (int i = 0; i < n; ++i) {
            auto v = std::make_unique<Vector3>();
            v->parent = this;   // 子值知道父 owner
            v->index = i;
            slots.push_back(std::move(v));
        }
    }
    // 玩法脚本等价: state.slot(3).z = 4 后由引擎钩子触发冒泡
    Vector3& slot(int i) { return *slots[i]; }
};

int main() {
    printf("===== Demo C: PropertyOwner 冒泡最小模型 =====\n\n");
    Entity entity;
    Container state(&entity);
    state.init(5);

    printf("--- 玩法脚本等价: state.slot(3).z = 4.0 ---\n");
    Vector3& v3 = state.slot(3);
    v3.z = 4.0f;
    PropertyChange ch3;
    v3.bubble(ch3);             // 引擎钩子：子变更 → 冒泡到 Entity

    printf("\n--- 玩法脚本等价: state.slot(0).x = 1.5 ---\n");
    Vector3& v0 = state.slot(0);
    v0.x = 1.5f;
    PropertyChange ch0;
    v0.bubble(ch0);

    printf("\n===== Demo C 结论 =====\n");
    printf("1. 容器值挂 parent/index，变更沿 owner 链冒泡到根 Entity\n");
    printf("2. path=叶→根 index 序列，根端翻译成属性路径（深度增量）\n");
    printf("3. 只发变更叶子，不整表重发 —— C 相对 B 的架构优势\n");
    printf("4. 代价：apollo 现无此 owner 链，要照搬 PropertyOwner/DataType attach 架构\n");
    return 0;
}
