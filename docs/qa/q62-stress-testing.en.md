[English](q62-stress-testing.en.md) | [中文](q62-stress-testing.md)

# Q62: How to do stress testing?

## Core Conclusion

Stress testing is not "turn the concurrency up and run it once." It is using a repeatable load model to force out the system's real bottlenecks.

A truly effective stress test must answer all of these at the same time:

- What scenario is being stressed
- Whether the load model is close to real player behavior
- How the system degrades when it reaches its limit
- Whether the problem is reproducible and comparable

If you only stare at a single "peak online count," the stress test result usually has little decision-making value.

## 1. First distinguish several kinds of test goals

Common ones at least include:

- Load testing: verify that the target scale can run stably
- Stress testing: actively push the system to its limit
- Soak testing: see whether long-running leaks and degrades
- Peak testing: simulate sudden logins, sudden broadcasts, event openings

Different goals have different load models and acceptance criteria.

## 2. The most important thing in stress testing is the load model

Online games cannot only do "empty connection stress testing."

What is usually more valuable is:

- Login floods
- Large-scale movement sync
- Same-screen combat
- Instance creation and recycling
- Backend paths like mail, trading, leaderboards

Because the real bottleneck is often not the connection count, but the behavior distribution.

## 3. Stress testing should follow real gameplay rhythm as closely as possible

For example:

- Not every player sends a message every frame
- Not every scene is full-screen AOI
- PvE, PvP, main city, and event servers have completely different loads

If the stress test script is too idealized, the final conclusion will very likely misdirect optimization.

## 4. Stress testing is not only about "whether it can hold"

You also have to look at how the system breaks.

The more critical questions usually include:

- Does CPU run out first, or memory
- Is it latency jitter or a direct crash
- Does queue buildup appear
- Does cascading timeout appear

These determine the direction of later architecture changes.

## 5. Stress testing must be done together with observation

A stress test without observation has very little value.

At minimum you should collect simultaneously:

- CPU and memory
- Threads and locks
- Network throughput
- Queue length
- Latency percentiles of key interfaces
- Error rate

Otherwise you only see "it died," not "why it died."

## 6. Baselines and regression matter a lot

Stress testing is not only for finding the upper limit. It should also be used to:

- Establish version baselines
- Compare gains before and after optimization
- Catch performance regressions

Without a stable baseline, the team will keep getting stuck in arguments about "this version feels slower."

## 7. A more reliable engineering stress test process

A common approach is:

1. Define the target scenario and metrics
2. Build a load model close to reality
3. Ramp up gradually from low load
4. Record bottleneck points and degradation paths
5. After optimization, run regression comparison

Only then can stress test results genuinely guide engineering decisions.

## 8. Common misconceptions

### 1. Connection count is capacity

No. Real capacity depends on the behavior model behind the connections.

### 2. Running one peak stress test counts as passing

Not enough. Long-running, sudden peaks, and limit degradation are all different problems.

### 3. The simpler the stress test script, the more stable

Simple is indeed easy to reproduce, but if it departs from real business, the conclusion will be distorted.

## References

- Various online game stress testing, replay, and capacity assessment practice materials
