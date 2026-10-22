
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q47: How to Design a Guild System?

## Core Conclusion

A guild system is not "a large friend group", but a combined system of "long-term organizational relationships plus a permission system plus shared assets".

What really needs to be designed first is not the UI, but:

- Member relationships and role permissions
- Apply, invite, quit, and kick flows
- Guild public assets and logs
- Cross-server, offline, and online state synchronization

The moment features like the guild warehouse, benefits, and activities are connected in, it has already become a high-value organizational system.

## I. Core Responsibilities of the Guild System

Usually including:

- Organizing members
- Managing roles and permissions
- Supporting announcements, chat, applications, invitations
- Carrying extension features like guild technology, warehouse, events, and combat

So the foundation of the guild system is not chat, but "organizational state management".

## II. First Separate "Organizational Relationships" from "Online Interaction"

### 1. Organizational Relationships

More static:

- Guild master record
- Member affiliation
- Roles
- Announcements
- Contributions

### 2. Online Interaction

More dynamic:

- Online members
- Guild chat
- Real-time notifications
- Event registration

These two parts have completely different access patterns and should not be mixed into one simple structure.

## III. The Permission System Must Be Designed First

The point most easily out of control in a guild system is "who can do what".

At minimum you have to define:

- Guild master permissions
- Vice-master permissions
- Officer permissions
- Ordinary member permissions

Especially these high-risk operations:

- Kicking members
- Transferring the master role
- Using the warehouse
- Modifying announcements
- Issuing guild benefits

If permissions are only controlled on the frontend, problems will eventually arise.

## IV. Guild Public Assets Must Be Treated Separately

Once there is:

- Guild warehouse
- Guild funds
- Guild technology resources
- Territory revenue

These can no longer be treated as ordinary social features.

They need:

- Stricter permission control
- Change logs
- Auditable logs

Because these are multi-player shared assets, the risk is greater than personal inventory.

## V. Why Member Change Flows Must Be Rigorous

Typical high-frequency flows in a guild system include:

- Apply to join
- Invite to join
- Quit guild
- Kick member
- Transfer role

These flows look simple, but all involve:

- Permission judgment
- State change
- Notifications and logs
- Linkage with other systems

For example, after a member quits, you may also have to handle:

- Guild quest eligibility
- Guild war registration status
- Warehouse permission reclamation

## VI. Online State and Guild Broadcast Must Be Cost-controlled

Once the number of people grows, the guild system easily becomes a broadcast hotspot.

For example:

- One person comes online, all members get a notice
- One announcement edit, everyone refreshes
- Member list full-sync frequently

A more prudent approach is usually:

- Pull a summary when entering the guild interface
- Do incremental sync for online state
- Handle chat and member list on separate channels

## VII. Cross-server and Large-region Systems Significantly Increase Complexity

If there are cross-server guilds, guild wars, and guild leaderboards, you have to solve:

- Globally unique member IDs
- Cross-server online state
- Unified storage of organizational data
- Data synchronization between event servers and main servers

At this point the guild system is usually no longer a single-server accessory module, but closer to a social and organizational middle platform.

## VIII. A More Pragmatic Design in Engineering

Common practice is:

- Guild master record stored independently
- Member relationship table stored independently
- Online state read from the session layer
- Guild public assets keep an audit log
- Guild chat, notifications, and events each split into sub-modules

This way later feature extensions will not drag the core organizational relationships into chaos.

## IX. Common Misconceptions

### 1. The Guild System Is Just the Friend System Plus a Group Chat

Far from enough. Organizational permissions and shared assets are the real source of core complexity.

### 2. Role Is Just a Display Field

No. A role essentially determines operation permissions and responsibility boundaries.

### 3. The Guild Warehouse Has Roughly the Same Logic as a Personal Inventory

No. The guild warehouse involves multi-player shared assets and has higher audit requirements.

## References

- Various online-game materials on guilds, organizational permissions, and shared assets practices
