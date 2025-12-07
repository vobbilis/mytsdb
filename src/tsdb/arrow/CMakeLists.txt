# Arrow Flight Server Library
if(HAVE_ARROW_FLIGHT)
    add_library(tsdb_arrow_impl STATIC
        flight_server.cpp
    )
    add_library(tsdb::arrow ALIAS tsdb_arrow_impl)

    target_include_directories(tsdb_arrow_impl PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${Arrow_INCLUDE_DIRS}
    )

    target_link_libraries(tsdb_arrow_impl PUBLIC
        tsdb::storage_impl
        Arrow::arrow_shared
        ArrowFlight::arrow_flight_shared
    )

    if(HAVE_SPDLOG)
        target_link_libraries(tsdb_arrow_impl PRIVATE spdlog::spdlog)
    endif()

    message(STATUS "Arrow Flight server library enabled")
else()
    message(STATUS "Skipping Arrow Flight server (ArrowFlight not found)")
endif()
