add_executable(test1 test1.cpp)
target_include_directories(test1 PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test1 PUBLIC 
                     OLEDFunctions 
                     oledgf_options 
                     oledgf_warnings
                     PRIVATE Eigen)

add_test(NAME test1
         COMMAND test1)