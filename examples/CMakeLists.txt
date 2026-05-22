cmake_minimum_required(VERSION 3.16)

set(CM_EXAMPLES
    basic_usage
    server_config
    multi_format
    config_merge
    config_watcher
)

foreach(ex ${CM_EXAMPLES})
    add_executable(${ex} ${ex}.c)
    target_link_libraries(${ex} PRIVATE config_manager)
    target_include_directories(${ex} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
endforeach()
