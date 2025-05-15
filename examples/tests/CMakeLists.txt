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

add_executable(test_test test_test.cpp)
target_include_directories(test_test PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_test PUBLIC 
                     OLEDFunctions 
                     oledgf_options 
                     oledgf_warnings
                     matplot
                     PRIVATE Eigen)

add_executable(test_sim_input test_sim_input.cpp)
target_include_directories(test_sim_input PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_sim_input PUBLIC 
                     OLEDFunctions 
                     oledgf_options 
                     oledgf_warnings
                     matplot
                     PRIVATE Eigen)

add_executable(test_fit_input test_fit_input.cpp)
target_include_directories(test_fit_input PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_fit_input PUBLIC 
                     OLEDFunctions 
                     oledgf_options 
                     oledgf_warnings
                     matplot
                     PRIVATE Eigen)