
# Mirror Architecture Analysis

## 1. Positioning

Mirror is a very common open-source networking framework in the Unity ecosystem.  
It is not an MMO framework, but it comes up often in discussions of multiplayer, AOI, and interest management.

## 2. Core Architecture

Mirror's core is:

- NetworkIdentity
- NetworkBehaviour
- Server-authoritative replication
- Interest Management

What is most relevant to MMOs is that it explicitly provides multiple Interest Management solutions.

## 3. MMO Core Capability Analysis

### 3.1 World Model

Mirror itself does not provide an MMO world model.  
It is more like a network layer and a synchronization layer.

### 3.2 Synchronization

What deserves the most attention in Mirror is Interest Management.  
The official docs use MMOs directly as an example, pointing out that broadcasting the entire world state to all players would be completely unscalable.

So what it provides is:

- synchronizing only relevant objects to relevant clients
- controlling synchronization scale by interest range

### 3.3 Persistence and Business Logic

Mirror basically does not care about:

- persistence
- accounts
- social features
- MMORPG world logic

These all need to be filled in by outer layers.

## 4. Distinctive Traits

- lightweight
- common in the Unity community
- interest management exists as an official capability

## 5. Strengths

- easy to integrate into Unity projects
- network object synchronization is simple and direct
- suited to building upper MMO logic yourself

## 6. Weaknesses

- only a networking framework, not an MMO backend
- lacking persistent worlds, orchestration, and online infrastructure capabilities
- large worlds still require extensive in-house work

## 7. Applicable Scenarios

Suitable for:

- the networking layer of Unity teams building their own backend solution
- small-to-medium real-time multiplayer
- teams that need a customized synchronization model

## 8. Implications for Apollo

The most valuable things to absorb are:

- Interest Management must be a first-class design concern
- do not treat AOI as an optimization to bolt on after launch

## 9. References

- Documentation home: https://mirror-networking.gitbook.io/docs
- Interest Management: https://mirror-networking.gitbook.io/docs/manual/interest-management
