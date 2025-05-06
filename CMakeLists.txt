cmake_minimum_required(VERSION 3.15...3.25)

if (NOT DEFINED CMAKE_CXX_STANDARD)
    set(CMAKE_CXX_STANDARD 23)
endif()
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

project(oledgf
        VERSION 0.1
        LANGUAGES CXX)


include(cmake/PreventInSourceBuilds.cmake)
include(ProjectOptions.cmake)

include(Dependencies.cmake)
oledgf_local_options()
oledgf_setup_dependencies()

# Set c++ standard
target_compile_features(oledgf_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_subdirectory(src)

#add_subdirectory(examples/main)
#add_subdirectory(examples/tests)

# Adding tests:
include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()