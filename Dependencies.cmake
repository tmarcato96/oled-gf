include(cmake/CPM.cmake)

if(APPLE)
    # This is where Homebrew installs Qt6Config.cmake
    list(PREPEND CMAKE_PREFIX_PATH "/usr/local/lib/cmake")
endif()

# Look for any Qt 6 version installed in common Qt installation root
if(NOT DEFINED CMAKE_PREFIX_PATH)
    set(DETECTED_QT_PATHS "")

    if(WIN32)
        file(GLOB QT_DIRS "C:/Qt/6*/mingw_64")
        list(APPEND DETECTED_QT_PATHS ${QT_DIRS})
    elseif(APPLE)
        file(GLOB QT_DIRS
            "$ENV{HOME}/Qt/6*/clang_64"
        )
        list(APPEND DETECTED_QT_PATHS ${QT_DIRS})
    elseif(UNIX)
        file(GLOB QT_DIRS
            "$ENV{HOME}/Qt/6*/gcc_64"
            "/opt/Qt/6*/gcc_64"
            "/usr/local/Qt-6*/gcc_64"
            "/usr/local/lib/cmake/Qt6"
            "/usr/lib/cmake/Qt6"
        )
        list(APPEND DETECTED_QT_PATHS ${QT_DIRS})
    endif()

    foreach(QT_PATH IN LISTS DETECTED_QT_PATHS)
        if(EXISTS "${QT_PATH}/lib/cmake/Qt6")
            list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
        endif()
    endforeach()
endif()


find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg Concurrent OpenGL PrintSupport)

function(oledgf_setup_dependencies)

#option(CPM_USE_LOCAL_PACKAGES "Try `find_package` before downloading dependencies" ON)

CPMAddPackage(
  NAME Eigen
  VERSION 3.4
  URL https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.gz
  # Eigen's CMakelists are not intended for library use
  DOWNLOAD_ONLY YES 
)

if(Eigen_ADDED)
  add_library(Eigen INTERFACE IMPORTED)
  target_include_directories(Eigen INTERFACE ${Eigen_SOURCE_DIR})
endif()

CPMAddPackage(
    NAME jsonsimplecpp
    GITHUB_REPOSITORY tmarcato96/jsonsimplecpp
    GIT_TAG main
)

FetchContent_Declare(
  QtQwt
  GIT_REPOSITORY "https://github.com/ZIMO-Elektronik/QtQwt"
  GIT_TAG v6.3.0)


cpmaddpackage("gh:ZIMO-Elektronik/QtQwt@6.3.0")

endfunction()
