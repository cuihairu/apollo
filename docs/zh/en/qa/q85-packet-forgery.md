
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q85: How Do You Prevent Packet Forgery?

## Core Conclusion

The key to preventing packet forgery is not "hiding the protocol", but making the server validate identity, timeliness, ordering, and business legitimacy for every request.

Truly effective protection usually includes:

- Session authentication
- Message integrity protection
- Replay and out-of-order control
- Server-side business validation

If the last layer is missing, even if signatures and encryption were done earlier, requests that are "legitimate but should not hold" can still be forged.

## I. What Packet Forgery Really Refers To

It is not simply packet sniffing, but the attacker constructing or tampering with messages themselves, making the server believe the request is legitimate.

Common targets include:

- Illegally calling unopened interfaces
- Modifying message fields
- Replaying old requests
- Bypassing client UI restrictions and sending business packets directly

## II. Protocol Privatization Itself Is Not a Security Boundary

Custom protocols, binary protocols, and obfuscated packet headers can all raise the bar, but they cannot serve as the core defense line.

Because as long as the client can send packets normally, the protocol will eventually be reverse-engineered.

The real security boundary is still server-side validation.

## III. Session Identity Must Be Established First

The server must at least know:

- Which session this message belongs to
- Whether this session is still valid
- Whether it is bound to the current connection

Otherwise, even if the message format is correct, the sender's identity cannot be confirmed.

## IV. Both Integrity and Timeliness Control Are Needed

Common approaches include:

- Message signature or MAC
- Timestamp
- Sequence number
- nonce

They respectively solve:

- Whether the content was modified
- Whether the message is expired
- Whether it was sent repeatedly

But these are only "transport-layer legitimacy", not yet "legitimate in business terms".

## V. Server-Side Business Validation Is the Layer That Really Decides the Outcome

For example, if the client sends:

- Use a skill
- Buy an item
- Claim a reward

The server must still validate:

- Whether the skill is on cooldown
- Whether the purchase conditions are met
- Whether the reward has already been claimed

In other words, the client can only submit request intent, not the final conclusion.

## VI. Replay and Forgery Often Appear Together

A historical legitimate request being resent is essentially also a form of forgery exploitation.

So packet forgery protection usually needs to be looked at together with:

- Replay attack protection
- Idempotency design

rather than being handled in isolation.

## VII. A Safer Combination in Engineering

Common practice is:

- Session-level authentication
- Message-level integrity protection
- Sequence number or nonce for replay prevention
- Server-side re-validation by business rules

This way, even if the protocol is reverse-engineered, it is still hard for an attacker to directly forge valid high-value requests.

## VIII. Common Misconceptions

### 1. A Custom Binary Protocol Prevents Forgery

No. It can only raise the reverse-engineering bar.

### 2. Once TLS Is Used, Message Validation Is Unnecessary

TLS protects the channel; it does not replace business legitimacy validation and idempotency.

### 3. As Long as the Signature Is Correct, the Request Should Execute

No. A signature only says "this message was not tampered with", not "this business should hold now".

## References

- Materials on session authentication, message signing, and business idempotency practices
