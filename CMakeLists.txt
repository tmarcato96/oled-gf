cmake_minimum_required(VERSION 3.25)

set(CMAKE_AUTOMOC ON)            #needed for Qt macros and preproc stuff
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

project(oledgf
        VERSION 0.1
        LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_TRY_COMPILE_TARGET_TYPE "STATIC_LIBRARY")

include(cmake/PreventInSourceBuilds.cmake)
include(Dependencies.cmake)
include(ProjectOptions.cmake)

oledgf_local_options()
oledgf_setup_dependencies()

# Set c++ standard
target_compile_features(oledgf_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_subdirectory(src)
add_subdirectory(gui/src)

# Adding tests:
include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()