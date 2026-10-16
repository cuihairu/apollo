[English](q86-permission-system.en.md) | [中文](q86-permission-system.md)

# Q86: How do you design a permission system?

## Core Conclusion

A permission system is not "sticking a few role labels on users", but expressing clearly "who can perform what operations on what objects", and being verifiable stably by the server.

What really needs to be thought through first is:

- Which resources permissions act on
- Whether permissions are role-level, object-level, or context-level
- Who is responsible for the final check
- How high-risk operations are audited

If these boundaries are not clear, the permission system quickly becomes a scattered collection of `if` judgments.

## 1. First clarify the objects permission control acts on

Common objects include:

- The player's own character data
- Backend management capabilities
- GM operations
- Organizations or shared resources like guilds, teams, and auctions

The permission models of these objects differ greatly and are not suited to one-size-fits-all.

## 2. The role model is only the starting point, not the whole

RBAC is very common, but in games it is usually not enough.

Because many permissions are not as simple as "are you a GM", but:

- Are you the current guild leader
- Are you the recipient of this mail
- Are you the owner of this object

In other words, many permissions are object-context related.

## 3. The server must do the final authentication

The client presentation layer can decide whether a button is clickable, but cannot serve as the final permission judgment.

The server must at least confirm:

- Caller identity
- Operation target
- Whether the current context is legitimate

Otherwise privilege escalation only requires bypassing the client interface.

## 4. High-risk operations need extra constraints

For example:

- Account bans
- Granting rewards
- Modifying assets
- Batch command execution

Such operations should usually have:

- Least privilege
- Double confirmation or stricter approval
- Audit logs

Otherwise the risk of backend misoperation is very high.

## 5. The permission system should be designed together with the audit system

Permissions are not only about "whether you can do it", but also about "whether it can be traced after you do it".

A more robust approach usually records:

- Who executed it
- On whom it was executed
- What was executed
- When it was executed
- Whether it succeeded

This way the permission system can truly carry governance responsibility.

## 6. A more robust engineering organization

Common practice is:

- The identity authentication system confirms "who you are"
- The permission system judges "what you can do"
- The resource layer judges "whether you have rights to this object"
- The audit system records "what you actually did"

These four layers should not be mixed together.

## 7. Common misconceptions

### 1. A permission system is just RBAC

RBAC is common, but object-level and context-level permissions are equally important in games.

### 2. Hiding the button on the frontend means no permission

No. The final check must be on the server.

### 3. Only backend systems need permission design

Guilds, teams, trading, and shared resources equally have permission boundaries.

## References

- RBAC, ABAC, and high-risk operation audit practice materials
