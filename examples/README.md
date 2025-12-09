# Apollo IoC Framework Examples

This directory contains example applications demonstrating the usage of the Apollo IoC framework.

## Building the Examples

```bash
mkdir build
cd build
cmake -DBUILD_EXAMPLES=ON ..
make
```

## Running the Examples

### 1. Basic Example (`ioc_example`)

Demonstrates basic component registration, lifecycle management, and dependency injection.

```bash
./ioc_example
```

Features shown:
- Automatic component registration with `REGISTER_COMPONENT` macro
- Component lifecycle: Initialize → Start → Stop → Destroy
- Dependency injection between components
- Component retrieval from the IoC container

### 2. Configuration Example (`config_example`)

Shows how to use configuration management with automatic reload.

```bash
./config_example
```

Features shown:
- `CONFIG_PROPERTY` macro for type-safe configuration
- Configuration change listeners
- JSON-based configuration files
- Automatic configuration file reloading

### 3. Dependency Management Example (`dependency_example`)

Demonstrates advanced dependency resolution and validation.

```bash
./dependency_example
```

Features shown:
- Topological sorting for dependency order
- Circular dependency detection
- Transitive dependency resolution
- Dependency validation

## Key Concepts Illustrated

1. **Component Lifecycle**: All components follow the lifecycle states:
   - UNINITIALIZED → INITIALIZING → INITIALIZED
   - → STARTING → STARTED
   - → STOPPING → STOPPED
   - → DESTROYING → DESTROYED

2. **Dependency Management**:
   - Components declare dependencies using `addDependency()`
   - Framework automatically resolves initialization order
   - Circular dependencies are detected and reported

3. **Configuration Integration**:
   - Type-safe configuration properties
   - Automatic reloading of configuration changes
   - Configuration change notifications

4. **IoC Container Features**:
   - Automatic component registration
   - Type-safe component retrieval
   - Singleton pattern implementation