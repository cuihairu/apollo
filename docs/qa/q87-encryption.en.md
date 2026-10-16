[English](q87-encryption.en.md) | [中文](q87-encryption.md)

# Q87: How to encrypt the transmission of sensitive data?

## Core Conclusion

The first choice for encrypting sensitive data in transit is usually not to develop your own protocol encryption, but a mature transport-layer security scheme.

A more pragmatic order is usually:

- Prioritize using TLS to protect the transport channel
- Add message-level signatures or application-layer protection where necessary
- Put more effort into key management, session binding, and business validation

Because the real problem in many systems is not "the algorithm is not strong enough," but:

- Poor key management
- Plaintext written into logs
- Session and replay control not done well

## 1. First distinguish transport encryption from business security

Transport encryption solves:

- Data not being eavesdropped on the link
- Man-in-the-middle having difficulty tampering

It does not automatically solve:

- Replay
- Business authorization violations
- The client forging legitimate requests

So "encrypted transmission" and "business trustworthiness" are not the same thing.

## 2. Why TLS is usually the priority

The reason is simple:

- The protocol is mature
- The implementation is mature
- The toolchain is mature
- Vulnerability exposure and repair paths are clearer

For most online systems, developing your own "message encryption protocol" is rarely more reliable than TLS.

## 3. When does extra application-layer protection have value

Some scenarios may still need supplements:

- Signatures on sensitive backend interfaces
- Payment callback authentication
- Replay control
- Integrity checks on key messages

But what is more common here is "supplementary validation," not replacing transport-layer security.

## 4. What actually goes wrong easily is key and session management

Even if the encryption algorithm is fine, if:

- Session tickets leak
- Keys are not rotated for a long time
- Debug logs contain plaintext
- Reconnect tickets can be reused

The system is still very dangerous.

So what matters more in engineering is:

- Key rotation
- Session binding
- Ticket expiry
- Minimal exposure surface

## 5. In games, which data is most worth protecting first

Usually includes:

- Account authentication information
- Payment-related information
- GM and backend operations
- High-value business requests

Not every message needs the same heavy protection, but key paths must have strong protection.

## 6. A more reliable engineering combination

A common approach is:

- External transmission uniformly goes over TLS
- High-value interfaces get supplementary signatures and timeliness control
- Session tickets are short-lived and bound to the connection or identity
- Logs and monitoring avoid outputting sensitive plaintext

## 7. Common misconceptions

### 1. Custom encryption is definitely more secure than TLS

Usually not. Most teams find it hard to make a self-developed protocol more reliable than mature TLS.

### 2. Once the link is encrypted, the business is secure

Replay, authorization violations, and idempotency still have to be handled separately.

### 3. Every message deserves heavy encryption

You have to weigh cost and value. Focus protection on key paths.

## References

- TLS, message signature, and ticket management practice materials
