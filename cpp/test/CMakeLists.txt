add_executable(hello_publisher pub_test.cpp)
target_link_libraries(hello_publisher
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_subscriber sub_test.cpp)
target_link_libraries(hello_subscriber
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_service_client svc_client.cpp)
target_link_libraries(hello_service_client
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_service_server svc_server.cpp)
target_link_libraries(hello_service_server
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_param param_test.cpp)
target_link_libraries(hello_param
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_remote_param remote_param_test.cpp)
target_link_libraries(hello_remote_param
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_transform_broadcast hello_transform_broadcast.cpp)
target_link_libraries(hello_transform_broadcast
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)

add_executable(hello_transform_listen hello_transform_listen.cpp)
target_link_libraries(hello_transform_listen
glog::glog 
${_REFLECTION} 
${_GRPC_GRPCPP} 
${_PROTOBUF_LIBPROTOBUF} 
registrar_grpc_proto 
std_proto
rscl)