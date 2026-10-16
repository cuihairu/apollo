---
title: Base API
icon: cube
prev: /api/README.md
---

[English](base.en.md) | [中文](base.md)

# Base API

> 2026-10-10 audit: this page was rewritten against the actual state of HEAD
> (eight `modules/base` headers: time/thread_pool/id_pool/memory/string/rng/
> terminal/timer_wheel). The old draft's `Time::nowMillis()`,
> `IdPool::acquire()`, the standalone `<apollo/base/object_pool.hpp>` header
> etc. do not exist in the repository.

## apollo::base::Time（time.hpp）

```cpp
namespace apollo::base {
class Time {
public:
    using Timestamp = int64_t;
    static Timestamp now();                 // steady_clock milliseconds
    static int64_t now_microseconds();
    static int64_t now_nanoseconds();
    static time_t unix_time();
    static std::string format(time_t timestamp,
                              const char* pattern = "%Y-%m-%d %H:%M:%S");
};
}
```

The same file also provides:

- `Timer`——`reset()` / `elapsed()` (seconds, `double`).
- `ScopeTimer`——RAII timing (starts on construction, logs on destruction).
- `FpsCalculator`——`update()` feeds per frame, `fps()` reads the sliding frame rate (refresh interval injected at construction, default 500ms).

**Thread safety**: safe (all static, stateless)

---

## apollo::base::ThreadPool（thread_pool.hpp）

```cpp
namespace apollo::base {
class ThreadPool {
public:
    explicit ThreadPool(size_t threads);
    ~ThreadPool();                          // destruction stops it

    template<class F, class... Args>
    auto submit(F&& f, Args&&... args)
        -> std::future<std::invoke_result_t<F, Args...>>;
        // submit after stop throws std::runtime_error

    size_t thread_count() const;            // thread count
    size_t pending_task_count() const;      // queued task count
    void wait_for_all();                    // wait for all tasks to finish
    void stop();                            // stop (unexecuted tasks are dropped)
};
}
```

**Thread safety**: safe (internal locks)

---

## apollo::base::IdPool（id_pool.hpp）

```cpp
namespace apollo::base {
class IdPool {
public:
    IdPool() = default;
    IdPool(uint32_t min_id, uint32_t max_id);   // range [min_id, max_id]
    explicit IdPool(uint32_t size);             // [0, size)

    uint32_t allocate();                    // allocate (UINT32_MAX when exhausted)
    void release(uint32_t id);
    bool reserve(uint32_t id);              // pre-reserve a specific id
    void reset();

    bool is_valid(uint32_t id) const;
    bool is_allocated(uint32_t id) const;
    bool is_full() const;
    bool is_empty() const;
    uint32_t allocated_count() const;
    uint32_t capacity() const;
    uint32_t usage_percent() const;
};
}
```

**Thread safety**: unsafe (belongs to the host thread; no internal locks)

---

## apollo::base::ObjectPool（id_pool.hpp, slot-reuse IdPool mechanism）

```cpp
namespace apollo::base {
template <typename T>
class ObjectPool {
public:
    explicit ObjectPool(uint32_t capacity);

    uint32_t allocate(T object);            // slot-based holding, returns id (UINT32_MAX when exhausted)
    T* get(uint32_t id);                    // out-of-range/free returns nullptr
    std::unique_ptr<T> release(uint32_t id);// take out and free the slot
};
}
```

Note: unlike the old draft, this is not `acquire()/release(shared_ptr)`
semantics — the caller constructs the object, `allocate(std::move(obj))`
puts it in the pool, `get(id)` accesses it, and `release(id)` takes it out.

**Thread safety**: unsafe (no internal locks)

---

## apollo::base::String（string.hpp）

```cpp
namespace apollo::base {
class String {
public:
    // trimming (incl. ltrim/rtrim and *_in_place variants)
    static std::string trim(std::string_view str);
    static void trim_in_place(std::string& str);

    // case
    static std::string to_lower(std::string str);
    static std::string to_upper(std::string str);

    // comparison
    static bool iequals(std::string_view a, std::string_view b);
    static bool starts_with(std::string_view str, std::string_view prefix);
    static bool ends_with(std::string_view str, std::string_view suffix);
    static bool istarts_with(std::string_view str, std::string_view prefix);
    static bool iends_with(std::string_view str, std::string_view suffix);

    // splitting (string_view delimiter overload + out-parameter overload)
    static std::vector<std::string> split(std::string_view str, char delimiter);

    // joining (two overloads: vector<string> and vector<string_view>)
    static std::string join(const std::vector<std::string>& strs, std::string_view delimiter);

    // formatting
    template <typename... Args>
    static std::string format(const char* fmt, Args... args);
    static std::string format_bytes(uint64_t bytes);   // humanize a byte count
};
}
```

**Thread safety**: safe (all static, stateless)

---

## apollo::base::FixedMemoryPool（memory.hpp）

```cpp
namespace apollo::base {
template <size_t BlockSize, size_t BlockCount = 1024>
class FixedMemoryPool {
public:
    FixedMemoryPool();                      // capacity fixed at compile time by BlockCount

    void* allocate();                       // nullptr when exhausted
    void deallocate(void* ptr);

    size_t allocated_count() const;
    size_t capacity() const;                // = BlockCount
    size_t block_size() const;              // = BlockSize (>= sizeof(void*))
};
}
```

**Thread safety**: safe (internal mutex)

The same file also provides:

- `ByteBuffer`——`ByteBuffer(capacity = 1024)`, `write(const void*, len)` /
  read-pointer surface / `data()` / `size()` / `capacity()` / `empty()` (byte buffer).
- `ArenaAllocator`——`ArenaAllocator(initial_capacity = 4096)`, arena allocation +
  `construct<T>(args...)` placement construction (batches of small objects, released together).

---

## apollo::base::Pcg32（rng.hpp）

A minimal PCG-XSH-RR 64/32 implementation (deterministic substreams — the same
source as the combat random stream battle-determinism):

```cpp
namespace apollo::base {
class Pcg32 {
public:
    Pcg32() = default;
    void seed(std::uint64_t initstate, std::uint64_t stream);  // substream oddification built in
    std::uint32_t next_uint32() noexcept;
    double next_double() noexcept;          // [0, 1)
};
}
```

**Thread safety**: unsafe (single stream, single writer; cross-stream isolation via the stream parameter)

---

## apollo::base::Terminal（terminal.hpp）

A static terminal utility surface: `is_tty()` / `supports_color()` / `enable_color()` /
`get_size()` (`Size{rows, cols}`) / `clear()` / `clear_line()` /
`move_cursor(row, col)` / `cursor_up/down/left/right(n)` / `save_cursor()` etc.

**Thread safety**: unsafe (global terminal state)

---

## apollo::base::TimerWheel（timer_wheel.hpp, G-4 timer wheel library component）

A hierarchical hashed timing wheel (single-writer lock-free; the driving right
belongs to the game loop — `advance(now_ms)` feeds the clock):

```cpp
namespace apollo::base {
class TimerWheel {
public:
    using TimerId = std::uint64_t;          // (generation << 32) | slot number; 0 = invalid
    using Callback = std::function<void(TimerId)>;

    explicit TimerWheel(std::uint32_t tick_ms = 10, std::uint32_t slots = 64,
                        std::uint32_t levels = 5, std::uint64_t start_ms = 0);

    TimerId schedule_after(std::uint64_t delay_ms, Callback cb);      // one-shot
    TimerId schedule_interval(std::uint64_t interval_ms, Callback cb); // periodic
    bool cancel(TimerId id);                // already-fired/stale id = false

    void advance(std::uint64_t now_ms);     // clock feed: fires due callbacks per tick
};
}
```

**Thread safety**: unsafe (single-writer discipline — only the game loop thread may call it)

---

**The base module does not include**: logging, configuration, network, or
framework semantics — those belong to core and above (base is pure
infrastructure, usable standalone by any C++ project).
