[English](q38-data-integrity.en.md) | [中文](q38-data-integrity.md)

# Q38: How do you prevent data tampering?

## Core Conclusion

Anti-tampering is not just about encryption, and not just about defending against external attacks. What really needs to be defended against simultaneously is:

- The client forging or modifying requests
- Erroneous writes inside the service
- Operations or scripts misoperating
- The storage layer being directly modified

So a more effective approach is usually a multi-layer defense:

- Authoritative service validation
- Least privilege
- Idempotency and auditing
- Key data validation and reconciliation

Encryption and signatures are only one layer of it.

## One: First Distinguish "Anti-Disclosure" from "Anti-Tampering"

These two problems are often mixed together.

- Encryption leans more toward anti-disclosure
- Validation, signatures, auditing, and permission control lean more toward anti-tampering

If you only encrypt sensitive fields and store them in the database, that does not prevent an illegal business operation from being legitimately written.

## Two: The Most Important Defense Is Usually at the Application Layer

In online games, most high-value data should not allow the client to directly supply the result — only the request.

For example:

- The client can only request "use item"
- It cannot directly upload "I now have 9999 gold"

This is the most basic principle of the authoritative service.

## Three: Key Data Needs a Traceable Modification Chain

High-value data should at least record:

- Who changed it
- When it was changed
- What business reason changed it
- What it was before and after the change

Otherwise once a problem occurs, it is very hard to distinguish whether it was:

- Cheat-tool forgery
- A code bug
- Operations misoperation
- A compensation script executing twice

## Four: Where Signatures and Validation Fit

### 1. Transport-Layer or Interface-Layer Signatures

Suitable for:

- Payment callbacks
- GM interfaces
- Internal management interfaces
- High-value external requests

It solves "is the request source trustworthy, and has the content been altered".

### 2. Data Checksums

Suitable for:

- Configuration files
- Offline packages
- Certain save blocks

It is better at detecting "the content has been touched"; it does not directly equal "the business cannot be illegally rewritten".

## Five: Permission Control Is More Common and More Effective Than Encryption

Many data tampering incidents are not hackers directly entering the database, but:

- Backend permissions being too broad
- Script accounts able to write too many tables
- Service accounts being shared

So a very important layer of anti-tampering is:

- Least-privilege accounts
- Environment isolation
- Auditing of backend operations
- Secondary confirmation for high-risk operations

## Six: Asset-Class Data Needs Reconciliation and Auditing

For data like currency, items, transactions, and mail attachments, online validation alone is not enough.

A more solid approach is usually:

- Main state
- Change ledger
- Periodic reconciliation
- Anomaly alerting

That way even if an error occurs, it can be traced and remediated.

## Seven: Both the Client and Internal Services Need Protection

Do not only watch out for cheat tools.

Many real incidents come from:

- Internal interfaces lacking authentication
- Compensation tasks executing twice
- Canary code writing wrong data
- Operations scripts mistakenly modifying master data

So anti-tampering must include internal systems in the threat model.

## Eight: A More Solid Combination in Engineering

Common multi-layer defenses include:

- Key logic computed authoritatively by the server
- High-value interfaces get signatures, authentication, and idempotency
- Database accounts with least privilege
- Key asset writes retain a ledger
- Periodic reconciliation and anomaly detection
- High-risk backend operations keep audit logs

This is more effective than simply emphasizing "field encryption".

## Nine: Common Misconceptions

### 1. Encrypting Database Fields Means Tampering Is Prevented

No. Encryption is more about anti-disclosure; it does not prevent erroneous writes on legitimate paths.

### 2. Defending Only Against the Client Is Enough

Many incidents have their roots inside the service or in backend pipelines.

### 3. Anti-Tampering Is Just Doing Hashes

Hashes can only help validate part of the content; they cannot replace permissions, auditing, and authoritative computation.

## References

- Various online game asset auditing, backend least-privilege, and payment callback security practice materials
