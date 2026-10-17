
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q19: How to prevent message replay attacks?

## Core Conclusion

Replay prevention cannot rely on the transport layer alone, nor on the business layer alone.

A complete scheme usually contains at least two layers:

- The transport or session layer prevents old packets and duplicate packets from being accepted again
- The business layer guarantees that critical operations do not take effect twice even if they arrive repeatedly

If you only do the former, operations like trading, reward issuance, and billing can still go wrong. If you only do the latter, the login, session, and authentication chain is left exposed to obvious risk.

## 1. What is a replay attack

A replay attack is not tampering with a message. It is sending a message that was once legitimate again.

Typical prerequisites include:

- The attacker can intercept the request
- The message itself is still format-legal
- The server lacks a judgment of "this message has expired or already been processed"

Its danger lies in the fact that the message content may be entirely genuine, so checking format alone, checking fields, or even checking signatures may not be enough.

## 2. What are the truly dangerous scenarios

### 1. Login or session recovery

If login tokens, reconnect tokens, or server-switch tickets can be reused, an attacker may forge a "legitimate reconnect" within the token's validity period.

### 2. Asset-type operations

For example:

- Purchasing
- Transfers
- Reward issuance
- Claiming mail attachments
- Redeeming codes

Once these operations are replayed, asset correctness is directly affected.

### 3. Combat or gameplay commands

High-frequency operations may not each be high-value, but if the same attack or cast command is accepted repeatedly, it can lead to:

- Duplicate damage
- Duplicate displacement
- Illegal status stacking

### 4. Backend or internal service calls

Do not only watch the client. Inter-service messages, GM commands, compensation scripts, and payment callbacks all carry replay risk, and when they go wrong the impact is usually greater.

## 3. Why "having signatures" is still not enough

Signatures solve "was the message tampered with," not "was this legitimate message sent once more."

For example a signed reward-issuance request:

- The first send is legitimate
- The second send, byte-for-byte identical, still has a valid signature

Without extra timeliness or uniqueness checks, the server may still execute it as normal.

So replay prevention has to answer at least two questions:

- Is this message new
- Has this operation already been processed

## 4. What problem does each common technique solve

### 1. Timestamps

Timestamps are suitable for solving the "expired message" problem.

A common practice is:

- The message carries a send time
- The server only accepts requests within a short time window

The advantage is simplicity, and the downside is obvious:

- Depends on clock synchronization
- Cannot prevent duplicates within the window

So timestamps usually only serve as a first-stage filter.

### 2. Sequence

Session sequence numbers are suitable for handling "duplicate packets and out-of-order packets within the same connection or the same session."

A common practice is:

- Each session maintains an incrementing sequence
- The server records the maximum sequence accepted so far, or a sliding window
- Duplicate sequences are dropped outright

It is better suited to:

- Real-time combat input
- Movement commands
- High-frequency low-value messages

But it is not good at solving duplicate business submissions across connections, across retries, or across service chains.

### 3. Nonce

`Nonce` is essentially a one-time random identifier for a message.

The server records nonces already seen within a short window, and rejects them when they appear again.

It suits:

- Login tickets
- Sensitive interface calls
- Short-cycle one-time messages

The downside is that you have to maintain storage or a cache window.

### 4. Idempotent operation ID

This is the most critical layer at the business level.

For operations like purchasing, claiming rewards, or transfers, the request should carry a business-unique ID, for example:

- `order_id`
- `request_id`
- `operation_id`

The server's handling principle is:

- The first time this ID is seen, execute the operation and record the result
- When the same ID is seen again, do not execute again; just return the existing result

This step is not a "security extra." It is a base capability of the asset system.

## 5. Transport-layer replay prevention and business idempotency are not the same thing

These two layers are often conflated, but the boundary must be clear.

### 1. Transport-layer replay prevention

Concerned with:

- Duplicate packets
- Out-of-order packets
- Expired packets
- Session legitimacy

Common means:

- Timestamps
- Sequence numbers
- Nonce
- Session tokens
- Message signatures

### 2. Business-layer idempotency

Concerned with:

- Has this business operation already executed successfully
- Will a retry charge, deliver, or issue rewards again
- Can the result stay consistent after a service restart or a timeout retry

Common means:

- Business-unique ID
- State machine constraints
- Dedup tables
- Unique indexes
- Result caches

Both layers are needed, especially on asset and payment paths.

## 6. Different message types should use different strategies

### 1. High-frequency real-time messages

For example movement, facing, ordinary combat input.

A more suitable combination is usually:

- Session sequence number
- Window dedup
- Authoritative recompute

These messages aim for low cost and low latency. Heavy persistent dedup per message is not appropriate.

### 2. Medium-value operations

For example skill casting, interaction, map switching, party confirmation.

Usually you can adopt:

- Timestamp or short-term nonce
- Session sequence
- Server-side state legality validation

### 3. High-value operations

For example purchasing, transfers, reward issuance, delivery, payment callbacks.

You must have at least:

- Strong signatures or trusted authentication
- Timeliness control
- Business-unique ID
- Idempotent persistence

If the last item is missing, all the layers before it are still not solid enough.

## 7. The disconnect-reconnect scenario is especially prone to problems

Many systems do replay prevention on the normal request path, but leave gaps in the reconnect flow.

Typical risks include:

- The reconnect token can be used multiple times
- Old and new sessions are valid at the same time
- When the client resends the last few requests, dedup is missing
- Gateway retries and business retries stack, causing double execution

So the reconnect system usually also needs:

- Single-use reconnect tickets
- Explicit session switching and invalidation of the old connection
- A designed flow for resubmitting requests during the disconnect
- Ultimate idempotency of business operations

## 8. A relatively solid combination in engineering

A common practical scheme is:

- Login and reconnect: short-term token + timestamp + nonce + signature
- Real-time commands: session sequence + window dedup + authoritative recompute
- Asset operations: business-unique ID + idempotent storage + signature verification
- Service callbacks: source authentication + replay window + unique serial number

Combined with:

- Anomaly logging
- Attack frequency monitoring
- Traceable audit records

Only then can you quickly limit losses when a problem is found.

## 9. A few common misconceptions

### 1. If you use HTTPS/TLS you do not need replay prevention

Wrong. TLS protects the transport channel, but it cannot replace business idempotency, and it cannot automatically handle application-layer duplicate requests.

### 2. Using sequence numbers makes you invulnerable

Wrong. Sequence numbers are usually only valid within a single connection or session. As soon as there is a reconnect, a cross-service retry, or business async compensation, this layer of protection can be bypassed.

### 3. Do database dedup for every message

Also unrealistic. For high-frequency real-time messages the cost is too high. You usually should handle in tiers according to message value.

### 4. Only block the client, not internal calls

This is very dangerous. Many incidents are not caused by external cheats, but by internal system timeout retries, compensation scripts running repeatedly, or third-party callbacks notifying repeatedly.

## References

- OWASP, Replay Attack
- RFC 8446, TLS 1.3
- Various payment callback and idempotency design practice materials
