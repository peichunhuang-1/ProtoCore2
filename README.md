# ProtoCore2

ProtoCore2 is a simple communication protocol based on Protocol Buffers (protobuf) and gRPC, supporting communication methods such as topics, services, parameters, and coordinate trees.

## Features

- **Topics**: Publish/subscribe communication model.
- **Services**: Remote procedure calls (RPC) for client-server communication. 
- **Parameters**: Dynamic parameters for node configuration.
- **Coordinate Trees**: Manage transformations and coordinate frames.

## Prerequisites

Before you begin, ensure you have met the following requirements:

- Docker (optional)
- gRPC
- glog
- KDL (Kinematics and Dynamics Library)
- CMake
- Protobuf

## Installation

### Option 1: Build Docker Image

```bash
git clone https://github.com/peichunhuang-1/ProtoCore2.git
```
```bash
cd ProtoCore2 && mkdir build
```
```bash
cd build && cmake .. -DCMAKE_PREFIX_PATH=${HOME}/.local
```
```bash
make -j8
```

### Option 2: Setup locally


## Testing

The executable files are located in `build/cpp`, including:

1. **Core Server for Registering Node Services:** <br>
   This server must run before all nodes, similar to the ROS Master. However, its functionality is limited to node discovery and deregistration notifications.
   ```bash
   ./cpp/src/NodeCore
   ```
2. **Testing Topics:** <br>
   The communication of topics can be divided into publishers and subscribers, similar to ROS. Open two terminals to run the following commands.
   ```bash
   ./cpp/test/hello_publisher publisher hello
   ```
   ```bash
   ./cpp/test/hello_subscriber subscriber hello
   ```

