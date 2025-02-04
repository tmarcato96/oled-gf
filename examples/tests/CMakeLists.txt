add_executable(test_fitting test_fitting.cpp)
target_link_libraries(test_fitting
                      PUBLIC oledgf_warnings
                             oledgf_options
                             matplot
                             OLEDFunctions
                      PRIVATE Eigen)               

add_executable(test_barnes test_barnes.cpp)
target_include_directories(test_barnes PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_barnes PUBLIC 
                      OLEDFunctions 
                      oledgf_options 
                      oledgf_warnings
                      matplot
                      PRIVATE Eigen)

add_executable(test_config test_config.cpp)
target_include_directories(test_config PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_config PUBLIC 
                      OLEDFunctions 
                      oledgf_options 
                      oledgf_warnings
                      matplot
                      PRIVATE Eigen)