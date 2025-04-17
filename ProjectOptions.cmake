macro(oledgf_supports_sanitizers)
  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND NOT WIN32)
    set(SUPPORTS_UBSAN OFF)
  else()
    set(SUPPORTS_UBSAN OFF)
  endif()

  if((CMAKE_CXX_COMPILER_ID MATCHES ".*Clang.*" OR CMAKE_CXX_COMPILER_ID MATCHES ".*GNU.*") AND WIN32)
    set(SUPPORTS_ASAN OFF)
  else()
    set(SUPPORTS_ASAN OFF)
  endif()
endmacro()

macro(oledgf_local_options)

    oledgf_supports_sanitizers()

    option(oledgf_WARNINGS_AS_ERRORS "Treat Warnings As Errors" OFF)
    option(oledgf_ENABLE_CPPCHECK "Enable cpp-check analysis" OFF)
    option(oledgf_ENABLE_SANITIZER_ADDRESS "Enable address sanitizer" ${SUPPORTS_ASAN})
    option(oledgf_ENABLE_SANITIZER_LEAK "Enable leak sanitizer" OFF)
    option(oledgf_ENABLE_SANITIZER_UNDEFINED "Enable undefined sanitizer" ${SUPPORTS_UBSAN})
    option(oledgf_ENABLE_SANITIZER_THREAD "Enable thread sanitizer" OFF)
    option(oledgf_ENABLE_SANITIZER_MEMORY "Enable memory sanitizer" OFF)

    if(PROJECT_IS_TOP_LEVEL)
     include(cmake/StandardProjectSettings.cmake)
    endif()

    add_library(oledgf_warnings INTERFACE)
    add_library(oledgf_options INTERFACE)

    include(cmake/CompilerWarnings.cmake)
    oledgf_set_project_warnings(
        oledgf_warnings
        ${oledgf_WARNINGS_AS_ERRORS}
        ""
        ""
        ""
        "")
    
    include(cmake/StaticAnalyzers.cmake)
    if (oledgf_ENABLE_CPPCHECK)
      oledgf_enable_cppcheck(${oledgf_WARNINGS_AS_ERRORS}
      "") 
    endif()

    include(cmake/Sanitizers.cmake)
    oledgf_enable_sanitizers(
        oledgf_options
        ${oledgf_ENABLE_SANITIZER_ADDRESS}
        ${oledgf_ENABLE_SANITIZER_LEAK}
        ${oledgf_ENABLE_SANITIZER_UNDEFINED}
        ${oledgf_ENABLE_SANITIZER_THREAD}
        ${oledgf_ENABLE_SANITIZER_MEMORY})

    include(cmake/EnableSIMD.cmake)
    oledgf_enable_simd(
      oledgf_options
    )

endmacro()
