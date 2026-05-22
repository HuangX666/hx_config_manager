cmake_minimum_required(VERSION 3.16)

# ─── Simple test runner (no external framework needed) ───────────────

set(CM_TEST_SOURCES
    test_core.c
    test_json.c
    test_yaml.c
    test_xml.c
    test_ini.c
    test_toml.c
    test_env.c
    test_merge.c
)

foreach(src ${CM_TEST_SOURCES})
    get_filename_component(test_name ${src} NAME_WE)
    add_executable(${test_name} ${src})
    target_link_libraries(${test_name} PRIVATE config_manager)
    target_include_directories(${test_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endforeach()
