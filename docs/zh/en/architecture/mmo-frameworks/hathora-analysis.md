
# Hathora Architecture Analysis

## 1. Positioning

Hathora is more like:

- a low-latency multiplayer hosting platform
- focused on real-time sessions and dedicated server hosting

It is not an MMO world engine, nor a complete game backend platform.

## 2. Core Architecture

Hathora's public positioning has long revolved around:

- multiplayer infrastructure
- low-latency session hosting
- server orchestration

By category, it is closer to:

- a lightweight cloud-hosted game server platform

## 3. MMO Core Capability Analysis

### 3.1 World Model

Hathora does not provide a world model.

### 3.2 Synchronization

Synchronization is decided by your game server.  
Hathora cares more about:

- where processes run
- how players access low-latency sessions

### 3.3 Operational Capabilities

Its value lies mainly in:

- hosting
- deployment across regions
- room/instance-style real-time session hosting

## 4. Distinctive Traits

- emphasizes low-latency real-time session hosting
- a fairly lightweight platform
- closer to a "multiplayer game hosting layer"

## 5. Strengths

- suited to quickly hosting real-time games
- lighter than building your own infrastructure

## 6. Weaknesses

- not an MMO core
- not a complete backend platform
- persistence, social features, and the world layer all have to be supplied from outside

## 7. Applicable Scenarios

Suitable for:

- real-time room servers
- instance servers
- teams that want to get dedicated servers online quickly

## 8. Implications for Apollo

The most valuable things to absorb are:

- separating the hosting layer from the logic layer
- the low-latency region-first deployment approach

## 9. References

- Official site: https://hathora.dev/
- Documentation entry point: https://hathora.dev/docs
