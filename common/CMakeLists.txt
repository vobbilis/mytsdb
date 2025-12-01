if(Protobuf_FOUND)
    # Generate protobuf files
    set(PROTO_FILES
        proto/remote.proto
        proto/metrics.proto
        proto/alerts.proto
    )
    
    foreach(PROTO_FILE ${PROTO_FILES})
        get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
        set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.cc")
        set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_NAME}.pb.h")
        
        add_custom_command(
            OUTPUT ${PROTO_SRC} ${PROTO_HDR}
            COMMAND ${Protobuf_PROTOC_EXECUTABLE}
            ARGS --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
                 --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
                 ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
            DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${PROTO_FILE}
            COMMENT "Generating ${PROTO_NAME} protobuf files"
        )
        
        list(APPEND PROTO_SRCS ${PROTO_SRC})
        list(APPEND PROTO_HDRS ${PROTO_HDR})
    endforeach()
    
    add_library(tsdb_proto STATIC ${PROTO_SRCS})
    target_link_libraries(tsdb_proto PUBLIC protobuf::libprotobuf)
    target_include_directories(tsdb_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
endif()
