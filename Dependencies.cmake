include(cmake/CPM.cmake)

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
    NAME matplotplusplus
    GITHUB_REPOSITORY alandefreitas/matplotplusplus
    GIT_TAG origin/master
)

CPMAddPackage(
  NAME jsonsimplecpp
  GITHUB_REPOSITORY tmarcato96/jsonsimplecpp
  GIT_TAG main
)
    
endfunction()
