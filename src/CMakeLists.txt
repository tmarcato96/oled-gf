# Glob all sources
file(GLOB sources "*.cpp")


add_library(OLEDFunctions ${sources})

target_include_directories(OLEDFunctions PUBLIC  "${PROJECT_SOURCE_DIR}/include"
                                         PRIVATE "${PROJECT_SOURCE_DIR}/gui/include")


target_link_libraries(OLEDFunctions PUBLIC oledgf_options 
                                           oledgf_warnings
                                           jsonsimplecpp
                                           Eigen)