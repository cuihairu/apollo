
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q32: How Do You Design a Mail System?

## Core Conclusion

A game mail system is essentially not "internal mail", but an "offline delivery plus attachment settlement system".

The real difficulty is usually not the mailbox interface, but:

- Whether attachments can be claimed repeatedly
- Whether mail can be delivered in batches
- How expiration and deletion are handled
- Whether system mail and player mail share one link

Whenever attachments are involved, the mail system has already entered the scope of the asset system.

## 1. First Distinguish Mail Types

Common types include at least:

- System mail
- Event reward mail
- GM compensation mail
- Player interaction mail
- Battle report or settlement mail

Their scale, audit requirements, and risk control requirements are all different; they should not completely share one simple logic.

## 2. The Real Core Capabilities of a Mail System

They usually include:

- Delivery
- List query
- Mark as read
- Attachment claim
- Expiration cleanup

The most critical of these is attachment claiming, because this step directly affects asset correctness.

## 3. Attachment Claiming Must Be Idempotent

As long as client retries, disconnect reconnects, repeated clicks, and timeout retransmits exist, attachment claiming must prevent duplicates.

The safer approach is usually:

- Mail state and attachment state are managed separately
- The claim action has explicit idempotent protection
- Asset grants have transaction records

Otherwise duplicate claim incidents easily occur.

## 4. Why System Mail and Player Mail Should Be Treated Differently

### 1. System Mail

Its characteristics are usually:

- Huge send volume
- Templated content
- Standardized attachments

More suited to batch delivery or logical delivery.

### 2. Player Mail

Its characteristics are usually:

- Small quantity
- Stronger interaction
- Higher risk control requirements

For example, harassment, frequency, and illegal transaction risks need to be restricted.

## 5. Mail Storage Does Not Have to Be a Full Copy per Letter

Especially for mass system mail sends, if every player gets a full copy of the body and attachments, it is very wasteful.

A more reasonable approach is usually:

- Templates and body are separated
- Delivery records are associated with templates
- The player mailbox only saves ownership and state

This makes it easier to support large-volume event mail.

## 6. Expiration and deletion Rules Must Be Defined First

The place most prone to getting out of control long-term in a mail system is lifecycle management.

You need to define first:

- How long until unclaimed attachments expire
- How long claimed mail is retained
- Whether unread mail can be deleted
- Whether deletion is logical or physical cleanup

These rules directly affect cleanup jobs and user experience.

## 7. Offline Notification and Real-Time Notification Must Be Separated

When the player is online, you can notify in real time that "new mail has arrived"; when the player is offline, you only need to guarantee the delivery result is correct.

Do not mistake "reminder" for the definition of "delivery success".

What is truly authoritative is:

- Whether the mail has entered a queryable state
- Whether the attachment has been successfully claimed and booked

## 8. The More Robust Combination in Engineering

A common approach is:

- A mail master table or template table saves mail metadata
- A recipient relationship table saves each player's mail state
- Attachment claiming goes through an idempotent asset grant link
- Scheduled tasks clean up expired mail

This way display, delivery, and asset settlement are separated.

## 9. Common Misconceptions

### 1. The Mail System Is Just the Offline Version of Chat

No. As long as it has attachments, it is already part of the asset system.

### 2. Deleting a Mail Is the End of It

If there are unclaimed attachments, audit requirements, or batch template delivery, the deletion logic is not simple.

### 3. Batch Mail Sending Is a Loop of inserts

Once scale grows, this puts great pressure on the database and the task system; it usually needs templating and batching design.

## References

- Various online game system mail, compensation mail, and attachment idempotency practice materials
