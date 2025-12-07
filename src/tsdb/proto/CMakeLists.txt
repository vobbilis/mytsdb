# Protobuf generation for TSDB

find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# Find the gRPC C++ plugin
if(NOT GRPC_CPP_PLUGIN)
    find_program(GRPC_CPP_PLUGIN grpc_cpp_plugin)
endif()

if(NOT GRPC_CPP_PLUGIN)
    message(FATAL_ERROR "gRPC C++ plugin not found")
endif()

set(PROTO_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(PROTO_GEN_DIR "${CMAKE_CURRENT_SOURCE_DIR}/gen")

file(MAKE_DIRECTORY ${PROTO_GEN_DIR})

# TSDB Proto
set(TSDB_PROTO "${PROTO_SRC_DIR}/tsdb.proto")
set(TSDB_PROTO_SRCS "${PROTO_GEN_DIR}/tsdb.pb.cc" "${PROTO_GEN_DIR}/tsdb.grpc.pb.cc")
set(TSDB_PROTO_HDRS "${PROTO_GEN_DIR}/tsdb.pb.h" "${PROTO_GEN_DIR}/tsdb.grpc.pb.h")

add_custom_command(
    OUTPUT ${TSDB_PROTO_SRCS} ${TSDB_PROTO_HDRS}
    COMMAND ${Protobuf_PROTOC_EXECUTABLE}
    ARGS --grpc_out=${PROTO_GEN_DIR}
         --cpp_out=${PROTO_GEN_DIR}
         --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
         -I ${PROTO_SRC_DIR}
         ${TSDB_PROTO}
    DEPENDS ${TSDB_PROTO}
    COMMENT "Generating C++ code for tsdb.proto"
    VERBATIM
)

# OTEL Protos
# We need to find all OTEL protos and generate them
# Assuming they are in ../../otel-proto (submodule) or similar, but based on previous exploration
# the user has `otel-proto` at root.
# However, the `src/tsdb/otel/CMakeLists.txt` referenced `../proto/gen/opentelemetry/...`
# which implies they were generated INTO `src/tsdb/proto/gen`.
# The source protos seem to be missing from `src/tsdb/proto` except `tsdb.proto`.
# Wait, I need to know where the OTEL .proto files are to generate them.
# Earlier `list_dir otel-proto` showed the root `otel-proto` directory.
# So I should reference that.

set(OTEL_PROTO_ROOT "${CMAKE_SOURCE_DIR}/otel-proto")

# List of OTEL protos we need
set(OTEL_PROTOS
    "${OTEL_PROTO_ROOT}/opentelemetry/proto/common/v1/common.proto"
    "${OTEL_PROTO_ROOT}/opentelemetry/proto/resource/v1/resource.proto"
    "${OTEL_PROTO_ROOT}/opentelemetry/proto/metrics/v1/metrics.proto"
    "${OTEL_PROTO_ROOT}/opentelemetry/proto/collector/metrics/v1/metrics_service.proto"
)

set(OTEL_PROTO_SRCS)
set(OTEL_PROTO_HDRS)

foreach(PROTO_FILE ${OTEL_PROTOS})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
    
    # Calculate relative path to preserve structure in gen dir
    file(RELATIVE_PATH REL_PATH "${OTEL_PROTO_ROOT}" "${PROTO_DIR}")
    set(GEN_OUT_DIR "${PROTO_GEN_DIR}/${REL_PATH}")
    file(MAKE_DIRECTORY ${GEN_OUT_DIR})

    set(PROTO_SRC "${GEN_OUT_DIR}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${GEN_OUT_DIR}/${PROTO_NAME}.pb.h")
    set(GRPC_SRC "${GEN_OUT_DIR}/${PROTO_NAME}.grpc.pb.cc")
    set(GRPC_HDR "${GEN_OUT_DIR}/${PROTO_NAME}.grpc.pb.h")

    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR} ${GRPC_SRC} ${GRPC_HDR}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --grpc_out=${PROTO_GEN_DIR}
             --cpp_out=${PROTO_GEN_DIR}
             --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
             -I ${OTEL_PROTO_ROOT}
             ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Generating C++ code for ${PROTO_NAME}.proto"
        VERBATIM
    )

    list(APPEND OTEL_PROTO_SRCS ${PROTO_SRC} ${GRPC_SRC})
    list(APPEND OTEL_PROTO_HDRS ${PROTO_HDR} ${GRPC_HDR})
endforeach()

add_library(tsdb_proto_lib STATIC
    ${TSDB_PROTO_SRCS}
    ${TSDB_PROTO_HDRS}
    ${OTEL_PROTO_SRCS}
    ${OTEL_PROTO_HDRS}
)

target_link_libraries(tsdb_proto_lib
    PUBLIC
        protobuf::libprotobuf
        gRPC::grpc++
)

target_include_directories(tsdb_proto_lib
    PUBLIC
        $<BUILD_INTERFACE:${PROTO_GEN_DIR}>
        $<INSTALL_INTERFACE:include>
)

# Disable warnings for generated code
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(tsdb_proto_lib PRIVATE -w)
endif()

# Install library
install(TARGETS tsdb_proto_lib
    EXPORT tsdb-targets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
