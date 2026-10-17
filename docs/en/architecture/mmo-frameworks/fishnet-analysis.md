
# FishNet Architecture Analysis

## 1. Positioning

FishNet is another very common multiplayer networking framework in the Unity ecosystem.  
Like Mirror, it is not a complete MMO framework, but it is often evaluated for MMO prototypes and real-time Unity projects.

## 2. Core Architecture

Key capabilities FishNet emphasizes include:

- observer system
- scene visibility
- server/client ownership
- componentized networking systems

The most MMO-relevant among them are:

- `NetworkObserver`
- custom observer conditions

## 3. MMO Core Capability Analysis

### 3.1 World Model

FishNet does not provide a complete world engine.  
It is more like:

- a synchronization framework
- an observability and visibility control framework

### 3.2 Synchronization

The Observer System is its most noteworthy point.  
The official docs explicitly allow:

- defining which clients become observers of an object through conditions
- customizing observer rules

This takes it a step further than some lightweight networking libraries on AOI / visibility problems.

### 3.3 Scenes and Visibility

Scene Visibility is another important capability.  
This is very helpful for:

- multiple scenes
- per-map loading
- instances

## 4. Distinctive Traits

- a more flexible Observer System
- Scene Visibility suits complex Unity projects
- well suited to Unity teams that want to build the upper MMO skeleton themselves

## 5. Strengths

- strong network visibility control
- better suited to complex real-time worlds than plain room synchronization
- suited to building MMO-like prototypes

## 6. Weaknesses

- still not a complete MMO backend
- persistence, social features, accounts, and orchestration still need to be solved by outer layers

## 7. Applicable Scenarios

Suitable for:

- Unity MMO-like prototypes
- building your own server-side logic while needing a mature synchronization layer
- real-time projects with multi-scene and visibility needs

## 8. Implications for Apollo

The most valuable things to absorb are:

- observer rules should ideally be configurable and extensible
- scene visibility is very helpful for partitioning and instance management

## 9. References

- NetworkObserver: https://fish-networking.gitbook.io/docs/fishnet-building-blocks/components/network-observer
- Custom Conditions: https://fish-networking.gitbook.io/docs/guides/features/observers/custom-conditions
- Scene Visibility: https://fish-networking.gitbook.io/docs/guides/features/scene-management/scene-visibility
