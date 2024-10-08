add_library(rscl rscl.cpp NodeRegist.cpp AsyncSocket.cpp TCPServer.cpp TCPClient.cpp ParamRPC.cpp TransformTree.cpp)

target_link_libraries(rscl 
glog::glog 
${_PROTOBUF_LIBPROTOBUF} 
${_REFLECTION} 
${_GRPC_GRPCPP} 
rscl_grpc_proto 
registrar_grpc_proto 
std_proto
service_grpc_proto
param_grpc_proto
${orocos_kdl_LIBRARIES})

add_executable(NodeCore Registrar.cpp)
target_link_libraries(NodeCore 
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto)