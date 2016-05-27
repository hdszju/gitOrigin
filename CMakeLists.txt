cmake_minimum_required(VERSION 3.2)

project(champion)

if(UNIX)
SET(CMAKE_CXX_FLAGS "-std=c++11")
endif(UNIX)

if(UNIX)
set(PB_DIR /usr/local)
set(GRPC_DIR /usr/local)
set(OPENSSL_DIR /usr/local/ssl)
elseif(WIN32)
set(PB_DIR ${champion_SOURCE_DIR}/external_libs/protobuf-3.0.0-beta-1)
set(GRPC_DIR ${champion_SOURCE_DIR}/external_libs/grpc-0_11_1)
endif(UNIX)


set(PROTOBUF_INCLUDE_DIR ${PB_DIR}/include)


if(WIN32)
set(PROTOBUF_LIBRARY ${PB_DIR}/lib/Release/libprotobuf.lib)
set(PROTOBUF_LIBRARY_DEBUG ${PB_DIR}/lib/Debug/libprotobuf.lib)
set(PROTOBUF_PROTOC_EXECUTABLE ${PB_DIR}/bin/protoc.exe)
set(GRPC_CPP_PLUGIN_EXECUTABLE ${GRPC_DIR}/bin/grpc_cpp_plugin.exe)
set(GRPC_LIBRARIES   gpr.lib    grpc.lib    grpc++.lib    libeay32.lib    ssleay32.lib    zlib.lib)
elseif(UNIX)
set(PROTOBUF_LIBRARY ${PB_DIR}/lib/libprotobuf.a)
set(PROTOBUF_LIBRARY_DEBUG ${PB_DIR}/lib/libprotobuf.a)
set(PROTOBUF_PROTOC_EXECUTABLE ${PB_DIR}/bin/protoc)
set(GRPC_CPP_PLUGIN_EXECUTABLE ${GRPC_DIR}/bin/grpc_cpp_plugin)
set(GRPC_LIBRARIES libgrpc++.a libgrpc.a libgpr.a libssl.a libcrypto.a libz.a ${CMAKE_DL_LIBS})
endif(WIN32)

include_directories(
	${PB_DIR}/include
	${GRPC_DIR}/include
	${champion_SOURCE_DIR}/src
    ${NLOG_DIR}/include
)

link_directories(
	${GRPC_DIR}/lib
    ${OPENSSL_DIR}/lib
)

find_package(Protobuf REQUIRED)

#add_subdirectory(src)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(CHAMPION_GRPC_PROTO
	${champion_SOURCE_DIR}/src/champion/champion_rpc.proto
)

set(CHAMPION_SRC
	${champion_SOURCE_DIR}/src/champion/error.h
	${champion_SOURCE_DIR}/src/champion/node.h
	${champion_SOURCE_DIR}/src/champion/publisher_interface.h
	${champion_SOURCE_DIR}/src/champion/subscriber_interface.h
	${champion_SOURCE_DIR}/src/champion/thread_pool.h
	${champion_SOURCE_DIR}/src/champion/physical_node.h
	${champion_SOURCE_DIR}/src/champion/physical_node.cpp
	${champion_SOURCE_DIR}/src/champion/serialization_helper.h
	${champion_SOURCE_DIR}/src/champion/publisher.h
	${champion_SOURCE_DIR}/src/champion/subscriber.h
	${champion_SOURCE_DIR}/src/champion/champion.h
	${champion_SOURCE_DIR}/src/champion/champion.cpp
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${CHAMPION_GRPC_PROTO})

# grpc generate cpp
foreach(FIL ${CHAMPION_GRPC_PROTO})
  get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
  get_filename_component(FIL_WE ${FIL} NAME_WE)

  list(APPEND PROTO_SRCS "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc")
  list(APPEND PROTO_HDRS "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h")

  add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h"
      COMMAND  ${PROTOBUF_PROTOC_EXECUTABLE}
      ARGS -I ${champion_SOURCE_DIR}/src/champion
	       --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
	       --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN_EXECUTABLE}
		   ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${PROTOBUF_PROTOC_EXECUTABLE} ${GRPC_CPP_PLUGIN_EXECUTABLE}
      COMMENT "Running C++ protocol buffer compiler with grpc plugin on ${FIL}"
      VERBATIM )
endforeach()
set_source_files_properties(${PROTO_SRCS} ${PROTO_HDRS} PROPERTIES GENERATED TRUE)

# champion
add_library(champion ${CHAMPION_SRC} ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(champion ${GRPC_LIBRARIES} ${PROTOBUF_LIBRARIES})
target_include_directories(champion PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
if(WIN32)
add_custom_command(TARGET champion
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory ${champion_BINARY_DIR}/Debug
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Debug/libeay32.dll ${champion_BINARY_DIR}/Debug
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Debug/ssleay32.dll ${champion_BINARY_DIR}/Debug
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Debug/zlib.dll ${champion_BINARY_DIR}/Debug

    COMMAND ${CMAKE_COMMAND} -E make_directory ${champion_BINARY_DIR}/Release
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Release/libeay32.dll ${champion_BINARY_DIR}/Release
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Release/ssleay32.dll ${champion_BINARY_DIR}/Release
    COMMAND ${CMAKE_COMMAND} -E copy ${GRPC_DIR}/lib/Release/zlib.dll ${champion_BINARY_DIR}/Release
    )
endif(WIN32)

# champion_master
set(CHAMPION_MASTER_SRC
	${champion_SOURCE_DIR}/src/champion_master/job_queue.h
	${champion_SOURCE_DIR}/src/champion_master/job_queue.cpp
	${champion_SOURCE_DIR}/src/champion_master/master.h
	${champion_SOURCE_DIR}/src/champion_master/master.cpp
	${champion_SOURCE_DIR}/src/champion_master/message_manager.h
	${champion_SOURCE_DIR}/src/champion_master/message_manager.cpp
	${champion_SOURCE_DIR}/src/champion_master/node_manager.h
	${champion_SOURCE_DIR}/src/champion_master/node_manager.cpp

)

add_library(champion_master ${CHAMPION_MASTER_SRC})
target_link_libraries(champion_master champion)
add_executable(champion_master_exe ${champion_SOURCE_DIR}/src/champion_master/main.cpp)
target_link_libraries(champion_master_exe champion_master)
set_target_properties(champion_master_exe
    PROPERTIES
    DEBUG_POSTFIX "_D"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${champion_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${champion_BINARY_DIR}/Release"
)


# examples
protobuf_generate_cpp(EXAMPLE_PROTO_SRCS EXAMPLE_PROTO_HDRS ${champion_SOURCE_DIR}/src/examples/test_message.proto)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(example_publisher ${champion_SOURCE_DIR}/src/examples/example_publisher.cpp ${EXAMPLE_PROTO_SRCS} ${EXAMPLE_PROTO_HDRS})
if(WIN32)
target_link_libraries(example_publisher champion ws2_32.lib)
elseif(UNIX)
target_link_libraries(example_publisher champion)
endif(WIN32)

add_executable(example_subscriber ${champion_SOURCE_DIR}/src/examples/example_subscriber.cpp ${EXAMPLE_PROTO_SRCS} ${EXAMPLE_PROTO_HDRS})
if(WIN32)
target_link_libraries(example_subscriber champion ws2_32.lib)
elseif(UNIX)
target_link_libraries(example_subscriber champion)
endif(WIN32)

set_target_properties(example_publisher example_subscriber
    PROPERTIES
    DEBUG_POSTFIX "_D"
    RUNTIME_OUTPUT_DIRECTORY_DEBUG "${champion_BINARY_DIR}/Debug"
    RUNTIME_OUTPUT_DIRECTORY_RELEASE "${champion_BINARY_DIR}/Release"
)
