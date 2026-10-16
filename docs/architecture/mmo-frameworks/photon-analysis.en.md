[English](photon-analysis.en.md) | [中文](photon-analysis.md)

# Photon Architecture Analysis

## 1. Positioning

Photon is not a single product but a family of multiplayer networking product lines.  
For MMO architecture discussions, the two most relevant lines are:

- Photon Server, the traditional server-side model
- Photon Fusion, the modern authoritative sync model

So it is better understood as a "real-time multiplayer networking stack" rather than a single MMO engine.

## 2. Core Architecture

### 2.1 Photon Server

The official Photon Server basic applications docs have long emphasized:

- `Master Server`
- `Game Server`

This is the typical:

- Access / room allocation
- Room / session hosting

two-layer model.

In addition, the official docs once kept an interest management page for an MMO Demo, but they explicitly state that the MMO demo is no longer supported.

### 2.2 Photon Fusion

Fusion focuses on network topology and state sync.  
The official docs emphasize:

- Dedicated Server
- State Authority
- Interest Management

In Dedicated Server mode:

- The server holds full state authority
- Clients send inputs or requests
- The server decides object state

## 3. MMO Core Capability Analysis

### 3.1 World Model

Photon does not hand you a complete MMO world model.  
What it excels at:

- Sessions
- State sync
- interest management
- dedicated server topology

To do an MMO, you usually have to add yourself:

- World partitioning
- Cross-server routing
- AOI lifecycle
- Persistence

### 3.2 Interest Management

What deserves the most attention in MMO discussions about Photon is interest management.  
The old Photon Server MMO docs explicitly emphasized:

- region-based interest management

Fusion also keeps interest management as an advanced capability.

### 3.3 Deployment Model

Fusion Dedicated Server follows the "one headless Unity server per session" idea.  
This fits MMO instances, battlefields, and local worlds well, but the cost is high for an extra-large persistent world.

## 4. Distinctive Traits

- Mature network sync and interest management
- Good integration with the Unity ecosystem
- Multiple topology choices: Dedicated Server / Host / Shared

## 5. Strengths

- Strong real-time sync capability
- Friendly onboarding for Unity teams
- Clear authoritative-server model
- Mature AOI / interest management capability

## 6. Weaknesses

- Not a complete MMO world engine
- The MMO demo is no longer an officially pushed direction
- Persistent worlds and operations backends are not its core strength

## 7. Applicable Scenarios

Good fit:

- Unity online games
- Room-based multiplayer combat
- The local instances / battlefields / battle servers of an MMO

Not a good fit:

- Building a complete large-world MMO backend on Photon alone

## 8. Lessons for Apollo

What is most worth absorbing:

- The interest management abstraction
- Topology design
- Layering state authority, object sync, and world logic into three separate concerns

## 9. References

- Fusion network topologies: https://doc.photonengine.com/fusion/current/manual/network-topologies
- Fusion Interest Management: https://doc.photonengine.com/fusion/current/manual/advanced/interest-management
- Photon Server basic applications: https://doc.photonengine.com/server/v4/applications/base-applications
- Photon Server MMO Interest Management: https://doc.photonengine.com/server/v4/applications/mmo/mmo-basics
