#pragma once

#include <atomic>
#include <cassert>
#include <thread>

namespace apollo::game::core {

// 轻量线程归属标注（P0-1「执行上下文」基础件）。
//
// 用途：对象声明「由哪个线程创建并拥有、只允许 owner 线程修改」，配合
// concurrency 收敛把「mutex 保护一切」逐步降级为「单写者 + 断言校验」。
// 约定：对象构造时（owner 线程）调用 bind()；check() 在修改路径上调用，
// Debug 构建下断言调用线程 == owner；Release 构建下 check() 编译为空。
//
// 线程安全：bind() 只允许在对象生命周期早期（owner 线程，此时对象尚未跨线程可见）
// 调用一次；之后 bound_ 以 acquire/release 语义保护 owner_ 的单次写入/多次读取。
class ThreadAffinity {
public:
    // 绑定到当前调用线程。同一对象只允许绑定一次（再次 bind 触发断言/忽略）。
    void bind() {
        if (bound_.exchange(true, std::memory_order_acq_rel)) {
            assert(false && "ThreadAffinity: double bind");
            return;
        }
        owner_ = std::this_thread::get_id();
    }

    bool is_bound() const {
        return bound_.load(std::memory_order_acquire);
    }

    bool is_owner() const {
        if (!is_bound()) {
            return true; // 未绑定：允许任意线程（保守过渡期）
        }
        return owner_ == std::this_thread::get_id();
    }

    std::thread::id owner() const {
        return owner_;
    }

    // 修改/访问路径上的校验；Release 下为空操作（靠纪律，不靠运行时开销）。
    void check() const {
#ifndef NDEBUG
        assert(is_owner() && "ThreadAffinity violation: mutation from non-owner thread");
#endif
    }

private:
    std::thread::id owner_{};
    std::atomic<bool> bound_{false};
};

} // namespace apollo::game::core