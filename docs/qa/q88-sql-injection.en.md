[English](q88-sql-injection.en.md) | [中文](q88-sql-injection.md)

# Q88: How to Prevent SQL Injection?

## Core Conclusion

The single most core principle for preventing SQL injection is simple:

- SQL structure and user input must be separated

In other words, the structure of the query statement is defined by the program; external input can only serve as a parameter and must not be spliced into the SQL structure.

The remaining input validation, ORM, and permission controls are supplementary layers.

## 1. What Is the Real Root Cause of SQL Injection

The root cause is usually not "dirty user input", but the program splicing external input into the SQL syntax as part of it.

As long as structure and data are mixed together, injection risk always exists.

## 2. Parameterized Queries Are the Main Solution, Not a Suggestion

Whether using:

- Native drivers
- ORM
- Query builders

The core should always be parameter binding, not string concatenation.

This is not a style question, but a boundary question.

## 3. ORM Does Not Automatically Guarantee Safety Either

ORM can reduce hand-written SQL, but that does not mean injection risk is inherently gone.

Once there is:

- Concatenated raw SQL
- Dynamic table names or sort fields spliced directly in
- Spliced where condition fragments

The risk still exists.

So what really needs preventing is "uncontrolled concatenation", not just checking whether it is ORM.

## 4. Whitelist Validation Is Especially Important for Dynamic Fields

Some scenarios truly need dynamic control, for example:

- Sort fields
- Sort direction
- Optional query columns

These parts that cannot be simply parameterized should usually use whitelist mapping, rather than splicing client input verbatim.

## 5. Permissions and a Minimal Exposure Surface Also Matter

Even with injection risk, if the database account's permissions are strictly limited, the damage surface is much smaller.

A more sound approach is usually:

- Least privilege for the application account
- Separate read and write accounts
- Isolation of high-risk backend databases

This reduces the scope of incident impact.

## 6. Also Watch the Injection Surface in Logs and Monitoring

Many systems only watch the main query path, ignoring:

- Report queries
- Backend search
- Operations scripts
- Ad-hoc SQL tools

These places often more easily fall back to string concatenation for "temporary convenience".

## 7. A More Sound Approach in Engineering

Common practices are:

- All queries parameterized by default
- The structural parts of dynamic SQL go through whitelists
- Least privilege for database accounts
- Separate review of high-risk backend interfaces

## 8. Common Misconceptions

### 8.1 Using ORM Means No SQL Injection

Wrong. As long as there is raw concatenation, the risk remains.

### 8.2 Just Filtering Special Characters Is Enough

Filtering can only assist; it cannot replace parameterization.

### 8.3 Only the Login Interface Needs Injection Protection

Backend queries, reports, searches, and scripts are equally high-risk points.

## References

- Parameterized queries, whitelisted dynamic SQL, and least privilege practice materials
