# CompilerWarnings.cmake
# Apply strict-but-reasonable warnings to a target.
#
# Usage:
#   include(CompilerWarnings)
#   target_apply_warnings(my_target)

function(target_apply_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /wd4996   # deprecated CRT functions (we handle this ourselves)
            /wd4100   # unreferenced formal parameter
        )
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-unused-parameter
            -Wno-missing-field-initializers
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
        target_link_options(${target} PRIVATE
            $<$<CONFIG:Debug>:-fsanitize=address,undefined>
        )
    endif()
endfunction()
