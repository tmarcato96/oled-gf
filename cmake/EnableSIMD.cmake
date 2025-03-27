function(
    oledgf_enable_simd
    project_name
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
target_compile_options(${project_name} INTERFACE "$<$<CONFIG:RELEASE>:-march=native>")    
endif()

endfunction()