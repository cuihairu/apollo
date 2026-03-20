# Q66: SIMD 在游戏中有哪些应用？

## 问题分析

本题考察对 SIMD (Single Instruction Multiple Data) 的理解：
- SIMD 指令集介绍
- 游戏中的应用场景
- 性能优化效果
- 跨平台处理

---

## 一、SIMD 基础

### 1.1 SIMD 概念

```
┌─────────────────────────────────────────────────────────────┐
│                    SIMD vs 标量                              │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  标量处理 (Scalar):                                         │
│  ┌─────────────────────────────────────────────────┐       │
│  │  a = b + c                                        │       │
│  │  d = e + f                                        │       │
│  │  g = h + i                                        │       │
│  │  j = k + l                                        │       │
│  │                                                  │       │
│  │  需要 4 条指令                                     │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
│  SIMD 处理:                                                 │
│  ┌─────────────────────────────────────────────────┐       │
│  │  [a,d,g,j] = [b,e,h,k] + [c,f,i,l]               │       │
│  │                                                  │       │
│  │  只需 1 条指令！                                   │       │
│  └─────────────────────────────────────────────────┘       │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

### 1.2 SIMD 指令集

| 指令集 | 宽度 | 平台 | 说明 |
|--------|------|------|------|
| **SSE** | 128-bit | x86 | 4 × float / 2 × double |
| **SSE2** | 128-bit | x86 | 整数运算 |
| **AVX** | 256-bit | x86 | 8 × float / 4 × double |
| **AVX-512** | 512-bit | x86 | 16 × float / 8 × double |
| **NEON** | 128-bit | ARM | 4 × float / 2 × double |
| **VMX** | 128-bit | PowerPC | 游戏机 |

---

## 二、游戏应用场景

### 2.1 向量运算

```cpp
// SIMD 向量加法

#include <immintrin.h>

// ❌ 标量版本
void addVectors_scalar(const float* a, const float* b, float* result, int count) {
    for (int i = 0; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

// ✅ SSE 版本 (4 floats 并行)
void addVectors_sse(const float* a, const float* b, float* result, int count) {
    int i = 0;
    // 处理 4 的倍数部分
    for (; i + 4 <= count; i += 4) {
        __m128 va = _mm_load_ps(&a[i]);      // 加载 4 个 float
        __m128 vb = _mm_load_ps(&b[i]);
        __m128 vr = _mm_add_ps(va, vb);      // 并行加法
        _mm_store_ps(&result[i], vr);        // 存储
    }

    // 处理剩余元素
    for (; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}

// ✅ AVX 版本 (8 floats 并行)
void addVectors_avx(const float* a, const float* b, float* result, int count) {
    int i = 0;
    for (; i + 8 <= count; i += 8) {
        __m256 va = _mm256_load_ps(&a[i]);    // 加载 8 个 float
        __m256 vb = _mm256_load_ps(&b[i]);
        __m256 vr = _mm256_add_ps(va, vb);
        _mm256_store_ps(&result[i], vr);
    }

    for (; i < count; ++i) {
        result[i] = a[i] + b[i];
    }
}
```

### 2.2 矩阵运算

```cpp
// SIMD 矩阵乘法优化

struct Matrix4x4 {
    float m[16];

    // SIMD 矩阵乘法
    static Matrix4x4 multiply(const Matrix4x4& a, const Matrix4x4& b) {
        Matrix4x4 result;

        // 转置矩阵 B 以便 SIMD 计算
        __m128 b0 = _mm_load_ps(&b.m[0]);
        __m128 b1 = _mm_load_ps(&b.m[4]);
        __m128 b2 = _mm_load_ps(&b.m[8]);
        __m128 b3 = _mm_load_ps(&b.m[12]);

        _MM_TRANSPOSE4_PS(b0, b1, b2, b3);

        // 计算每一行
        for (int i = 0; i < 4; ++i) {
            __m128 row = _mm_set1_ps(0);

            __m128 a0 = _mm_set1_ps(a.m[i * 4 + 0]);
            row = _mm_add_ps(row, _mm_mul_ps(a0, b0));

            __m128 a1 = _mm_set1_ps(a.m[i * 4 + 1]);
            row = _mm_add_ps(row, _mm_mul_ps(a1, b1));

            __m128 a2 = _mm_set1_ps(a.m[i * 4 + 2]);
            row = _mm_add_ps(row, _mm_mul_ps(a2, b2));

            __m128 a3 = _mm_set1_ps(a.m[i * 4 + 3]);
            row = _mm_add_ps(row, _mm_mul_ps(a3, b3));

            _mm_store_ps(&result.m[i * 4], row);
        }

        return result;
    }
};
```

### 2.3 物理计算

```cpp
// SIMD 物理碰撞检测

struct BoundingBox {
    Vector3 min;
    Vector3 max;

    // SIMD 包围盒检测
    static bool intersect(const BoundingBox& a, const BoundingBox& b) {
        // 加载坐标
        __m128 aMin = _mm_set_ps(0, a.min.z, a.min.y, a.min.x);
        __m128 aMax = _mm_set_ps(0, a.max.z, a.max.y, a.max.x);
        __m128 bMin = _mm_set_ps(0, b.min.z, b.min.y, b.min.x);
        __m128 bMax = _mm_set_ps(0, b.max.z, b.max.y, b.max.x);

        // 比较: a.min <= b.max && a.max >= b.min
        __m128 cmp1 = _mm_cmple_ps(aMin, bMax);
        __m128 cmp2 = _mm_cmpge_ps(aMax, bMin);

        // 合并结果
        __m128 result = _mm_and_ps(cmp1, cmp2);

        // 检查所有维度都满足
        return (_mm_movemask_ps(result) & 0x7) == 0x7;
    }
};

// 批量距离计算
void calculateDistancesSIMD(
    const Vector3& origin,
    const Vector3* positions,
    float* distances,
    int count
) {
    __m128 ox = _mm_set1_ps(origin.x);
    __m128 oy = _mm_set1_ps(origin.y);
    __m128 oz = _mm_set1_ps(origin.z);

    int i = 0;
    for (; i + 4 <= count; i += 4) {
        // 加载 4 个位置
        __m128 px = _mm_set_ps(positions[i+3].x, positions[i+2].x,
                               positions[i+1].x, positions[i].x);
        __m128 py = _mm_set_ps(positions[i+3].y, positions[i+2].y,
                               positions[i+1].y, positions[i].y);
        __m128 pz = _mm_set_ps(positions[i+3].z, positions[i+2].z,
                               positions[i+1].z, positions[i].z);

        // 计算差值
        __m128 dx = _mm_sub_ps(px, ox);
        __m128 dy = _mm_sub_ps(py, oy);
        __m128 dz = _mm_sub_ps(pz, oz);

        // 平方和
        __m128 distSq = _mm_add_ps(_mm_add_ps(
            _mm_mul_ps(dx, dx),
            _mm_mul_ps(dy, dy)),
            _mm_mul_ps(dz, dz)
        );

        // 平方根
        __m128 dist = _mm_sqrt_ps(distSq);

        // 存储
        _mm_store_ps(&distances[i], dist);
    }

    // 处理剩余
    for (; i < count; ++i) {
        distances[i] = (positions[i] - origin).length();
    }
}
```

### 2.4 粒子系统

```cpp
// SIMD 粒子更新

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    float life;
    float padding;
};

class ParticleSystem {
public:
    void updateSIMD(float dt, int count) {
        __m128 dtVec = _mm_set1_ps(dt);
        __m128 gravity = _mm_set1_ps(-9.8f);

        int i = 0;
        for (; i + 4 <= count; i += 4) {
            // 加载位置
            __m128 px = _mm_load_ps(&particles_[i].x);
            __m128 py = _mm_load_ps(&particles_[i].y);
            __m128 pz = _mm_load_ps(&particles_[i].z);

            // 加载速度
            __m128 vx = _mm_load_ps(&particles_[i].vx);
            __m128 vy = _mm_load_ps(&particles_[i].vy);
            __m128 vz = _mm_load_ps(&particles_[i].vz);

            // 应用重力
            vy = _mm_add_ps(vy, _mm_mul_ps(gravity, dtVec));

            // 更新位置
            px = _mm_add_ps(px, _mm_mul_ps(vx, dtVec));
            py = _mm_add_ps(py, _mm_mul_ps(vy, dtVec));
            pz = _mm_add_ps(pz, _mm_mul_ps(vz, dtVec));

            // 存储结果
            _mm_store_ps(&particles_[i].x, px);
            _mm_store_ps(&particles_[i].y, py);
            _mm_store_ps(&particles_[i].z, pz);
            _mm_store_ps(&particles_[i].vy, vy);

            // 更新生命值
            __m128 life = _mm_load_ps(&particles_[i].life);
            life = _mm_sub_ps(life, dtVec);
            _mm_store_ps(&particles_[i].life, life);
        }
    }

private:
    Particle* particles_;
};
```

---

## 三、跨平台 SIMD

### 3.1 抽象层设计

```cpp
// 跨平台 SIMD 抽象

#if defined(__AVX2__)
    #define SIMD_WIDTH 8
    #define SIMD_FLOAT __m256
    #define SIMD_SET1(x) _mm256_set1_ps(x)
    #define SIMD_ADD(a, b) _mm256_add_ps(a, b)
    #define SIMD_STORE(p, a) _mm256_store_ps(p, a)
#elif defined(__SSE__)
    #define SIMD_WIDTH 4
    #define SIMD_FLOAT __m128
    #define SIMD_SET1(x) _mm_set1_ps(x)
    #define SIMD_ADD(a, b) _mm_add_ps(a, b)
    #define SIMD_STORE(p, a) _mm_store_ps(p, a)
#elif defined(__ARM_NEON)
    #define SIMD_WIDTH 4
    #define SIMD_FLOAT float32x4_t
    #define SIMD_SET1(x) vdupq_n_f32(x)
    #define SIMD_ADD(a, b) vaddq_f32(a, b)
    #define SIMD_STORE(p, a) vst1q_f32(p, a)
#endif

// 使用抽象层
void processArray(float* data, int count) {
    int i = 0;
    for (; i + SIMD_WIDTH <= count; i += SIMD_WIDTH) {
        SIMD_FLOAT a = SIMD_SET1(1.0f);
        SIMD_FLOAT b = SIMD_SET1(2.0f);
        SIMD_FLOAT r = SIMD_ADD(a, b);
        SIMD_STORE(&data[i], r);
    }
}
```

### 3.2 使用库

```cpp
// 使用 SIMD 库: sse2neon, xsimd

#include <xsimd/xsimd.hpp>

using namespace xsimd;

template <class T, std::size_t N>
void batch_add(const T* a, const T* b, T* result) {
    using batch_type = simd_batch<T, N>;

    batch_type ba = batch_type::load_aligned(a);
    batch_type bb = batch_type::load_aligned(b);
    batch_type br = ba + bb;
    br.store_aligned(result);
}

// 自动选择最佳 SIMD 宽度
void process(float* a, float* b, float* c, size_t size) {
    size_t simd_size = xsimd::simd_batch<float>::size;
    size_t i = 0;

    for (; i + simd_size <= size; i += simd_size) {
        batch_add<float, 4>(&a[i], &b[i], &c[i]);
    }

    for (; i < size; ++i) {
        c[i] = a[i] + b[i];
    }
}
```

---

## 四、性能对比

### 4.1 优化效果

| 操作 | 标量 | SSE | AVX2 | AVX-512 |
|------|------|-----|------|---------|
| **向量加法** | 1x | 3.8x | 7.2x | 14x |
| **点积** | 1x | 4x | 7.8x | 15x |
| **矩阵乘** | 1x | 3.5x | 6.5x | 12x |
| **粒子更新** | 1x | 3.2x | 6x | 11x |

### 4.2 性能分析

```
性能提升受以下因素影响:
1. 数据对齐 (aligned vs unaligned)
2. 数据规模 (太小不值得)
3. 内存带宽 (可能是瓶颈)
4. 指令依赖 (无法并行)

一般规则:
- 数据量 > 1000 元素时使用 SIMD
- 避免频繁标量/SIMD 切换
- 预取数据减少内存延迟
```

---

## 五、最佳实践

| 实践 | 说明 |
|------|------|
| **对齐分配** | 使用 aligned_alloc |
| **批量处理** | 聚合小操作 |
| **避免分支** | 使用 select |
| **预取数据** | 减少缓存未命中 |
| **性能测试** | 验证优化效果 |

---

## 六、总结

```
SIMD 优化 = 并行计算 + 数据对齐 + 批量处理 + 跨平台抽象
- 向量运算加速
- 物理计算优化
- 粒子系统提升
- 跨平台考虑
```

---

## 参考资料

- [Intel Intrinsics Guide](https://www.intel.com/content/www/us/en/docs/intrinsics-guide/index.html)
- [ARM NEON Intrinsics](https://developer.arm.com/architectures/instruction-sets/intrinsics/)
- [xsimd Library](https://github.com/xtensor-stack/xsimd)
