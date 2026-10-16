[English](q61-log-optimization.en.md) | [中文](q61-log-optimization.md)

# Q61: How Do You Optimize a Logging System?

## Core Conclusion

The goal of optimizing a logging system is not "printing fewer logs", but making logs both able to support troubleshooting and not dragging down the main chain in return.

What truly needs to be balanced is:

- Observability
- Write overhead
- Storage cost
- Query usability

If the logging system has no layering, there are usually only two outcomes: either the information is insufficient for troubleshooting, or the moment the system gets busy the logs slow the system down first.

## I. What the Logging System Is Really Solving

It carries at least three kinds of responsibilities:

- Online troubleshooting
- Audit and tracing
- Statistics and observability supplement

These three kinds of logs usually differ in how they are written, how long they are retained, and how they are consumed; they should not all be stuffed into the same output chain.

## II. Distinguish Log Types First

### 1. Debug Logs

Characterized by high volume and short-term value, usually only needed during problem investigation.

### 2. Business Logs

For example:

- Login
- Trade
- Reward granting
- State transitions

These logs usually carry a traceability responsibility.

### 3. Runtime Logs

For example:

- Errors
- Alerts
- System state changes

### 4. Audit Logs

For example:

- GM operations
- Payment callbacks
- Asset modifications

These logs usually need to be more stable and reliable than ordinary debug logs.

## III. Optimizing a Logging System Usually Does Not Start With Swapping the Library

The root of many logging problems is not the library, but how it is used:

- Too much string concatenation in hot paths
- Printing worthless detail inside high-frequency loops
- The error level being used as ordinary tracing
- The same event printed repeatedly

Reducing useless logs first is often more effective than introducing a more complex framework.

## IV. Asynchronous Writing Is Common, but Not Without Cost

Asynchronous logging reduces main-thread blocking, but it also brings:

- Queue backlog
- A window for lost logs
- Rising memory occupancy during peaks

So an asynchronous logging system must answer:

- What happens when the queue is full
- Whether dropping low-priority logs is allowed
- Whether to block the caller

These are engineering trade-offs, not default right answers.

## V. Formatting Cost Is Often Underestimated

Many hot-path logs are not slow at writing to disk, but at:

- Heavy string concatenation
- Heavy JSON serialization
- Dumping large objects too frequently

So common optimizations include:

- Deferred formatting
- Structured fields instead of large concatenated strings
- Conditional filtering for high-frequency logs

## VI. "Print to File" Should Not Be the Only Outlet for Logs

In real engineering it is common to split into:

- Local files
- Standard output
- Centralized collection
- Metrics or tracing

Not all information is suitable for long-text logs. Many high-frequency statistics are better as metrics than as thousands of lines of logs.

## VII. A Safer Design in Engineering

Common practice is:

- Log level classification
- Structured fields
- Asynchronous batch writing
- Conservative drop policy for high-priority logs
- Dynamic switches for debug logs

This balances troubleshooting capability and performance cost.

## VIII. Common Misconceptions

### 1. The More Detailed the Logs, the Better

Excessive worthless logs obscure the truly important information and also slow the system down.

### 2. Asynchronous Logging Has No Performance Problems

Queues, formatting, and peak backlog all still cause problems.

### 3. All Logs Should Be Unified into JSON

Structuring has value, but you also need to consider write cost and consumption scenarios.

## References

- Materials on structured logging, asynchronous logging, and observability system practices
