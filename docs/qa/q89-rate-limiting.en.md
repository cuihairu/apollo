[English](q89-rate-limiting.en.md) | [中文](q89-rate-limiting.md)

# Q89: How to Design Rate Limiting and Anti-Abuse Mechanisms?

## Core Conclusion

The goal of rate limiting and anti-abuse is not just to block "too many requests," but to distinguish:

- Normal high-frequency behavior
- Malicious or abnormal repetitive behavior
- Critical resource protection needs

A truly effective system usually doesn't just apply one token bucket algorithm, but works in layers:

- Access-layer rate limiting
- Session-layer rate limiting
- Business-layer anti-abuse

## I. First Distinguish Rate Limiting from Anti-Abuse

The two are related, but not exactly the same.

### 1. Rate Limiting

Focuses more on:

- Resource protection
- System stability

### 2. Anti-Abuse

Focuses more on:

- Malicious profit-seeking
- Automated abuse
- Abnormal repeated operations

Some requests have low volume but abnormally high profit, which also falls under anti-abuse.

## II. Rate Limiting Granularity Must Be Layered

Common granularities include:

- IP
- Account
- Character
- Session
- Interface or message type

Different layers solve different problems.

For example:

- IP is more about external attack and access protection
- Character and interface granularity is closer to business farming behavior

## III. Business-Layer Anti-Abuse Matters More Than the Algorithm

Many high-value vulnerabilities are not triggered by large traffic, but by:

- Repeated clicking
- Automated scripts
- Boundary-state retries

So the business layer usually also needs:

- Idempotency
- Cooldowns
- State machine validation
- Profit caps

These are more critical than a simple "how many times per second."

## IV. Different Interfaces Should Use Different Strategies

For example:

- The login interface suits strict access-layer rate limiting
- Chat suits dual frequency and content control
- Rewards and purchases suit idempotency plus frequency limits
- Movement messages cannot be handled with ordinary interface rate limiting thinking

If all requests use the same bucket parameters, neither experience nor security will be good.

## V. Post-Rate-Limiting Handling Must Also Be Designed

Common handling approaches include:

- Direct rejection
- Delayed response
- Degraded service
- CAPTCHA or manual review
- Recording into risk-control scores

Not every over-limit case should be dealt with a one-size-fits-all ban.

## VI. A More Robust Engineering Combination

Common practice is:

- Coarse rate limiting at the access layer
- Frequency control at the session layer
- Idempotency, state machines, and profit limits at the business layer
- Continuous anomaly statistics at the risk-control layer

Only this way can both the system and business profits be protected.

## VII. Common Misconceptions

### 1. Rate limiting is the token bucket

The token bucket is very common, but it is only a basic tool, not a complete anti-abuse solution.

### 2. Low request volume doesn't count as abuse

Many abusers exploit exactly the low-frequency, high-profit vulnerabilities.

### 3. Banning on over-limit is the safest

In many cases, gradual handling and continuous observation are more reasonable.

## References

- Rate limiting, idempotency, and risk-control scoring practice materials
