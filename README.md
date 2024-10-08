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

### Option 1: Build Docker Image or Pull from Dockerhub
```bash
docker pull peichunhuang/proto_core_env
```
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

Install grpc, kdl, etc. and the remainder is same as Option 1.

## Testing

**Environment**

There are 3 environment variables in this protocol, is remain unset, the local ip and master address would set to localhost and a default port, and the log would be directly write to stdout.
```bash
export CORE_LOCAL_IP="127.0.0.1"
```
```bash
export CORE_MASTER_ADDR="127.0.0.1:10010"
```
```bash
export CORE_LOG_DIR=${HOME}/.local/log
```

**Executable File**

The executable files are located in `build/cpp`, including:

1. **Core Server for Registering Node Services:** <br>
   This server must run before all nodes, similar to the ROS Master. However, its functionality is limited to node discovery and deregistration notifications.
   ```bash
   ./cpp/src/NodeCore
   ```
2. **Testing Topics:** <br>
   The communication of topics can be divided into publishers and subscribers, similar to ROS. Open two terminals to run the following commands. The args follow executables is the name of node, and the name of namespace, the combination of ${namespace}/${name} should be unique in a system.
   ```bash
   ./cpp/test/hello_publisher publisher hello
   ```
   ```bash
   ./cpp/test/hello_subscriber subscriber hello
   ```
3. **Testing Services:** <br>
   The communication of services can be divided into clients and server, and in this protocol, the request and reply can be async. Open two terminals to run the following commands.
   ```bash
   ./cpp/test/hello_service_server server hello
   ```
   ```bash
   ./cpp/test/hello_service_client client hello
   ```
   And you should follow the printout instruction(blue), which may tell you to cancel the request(y/n), to accept the cancel request(y/n), etc.
4. **Testing Parameters:** <br>
   The parameters is owned by nodes, a name of parameter can be divided into 3 parts: ${namespace}/${node_name}/${parameter}. A parameter can be set locally, or from remote. Open two terminals to run the following commands.
   ```bash
   ./cpp/test/hello_param param hello
   ```
   ```bash
   ./cpp/test/hello_remote_param remote_param hello
   ```
   And you should follow the printout instruction(blue), the "hello_remote_param" may tell you to set the remote node name, in this case, input "hello/param". Now the both node can set the parameter of hello/param node.
5. **Testing TF:** <br>
   The tf tree of each node subscribe to /tf and /tf_static, and combine the data to form a transform tree. Open two terminals to run the following commands.
   ```bash
   ./cpp/test/hello_transform_broadcasr broadcast hello
   ```
   ```bash
   ./cpp/test/hello_transform_listen listen hello
   ```
