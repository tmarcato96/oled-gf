qt_add_executable(test_fitting test_fitting.cpp)

target_include_directories(test_fitting PUBLIC "${PROJECT_SOURCE_DIR}/include"
                                        PUBLIC "${PROJECT_SOURCE_DIR}/gui/include")

target_link_libraries(test_fitting PRIVATE OLEDFunctions
                                   PUBLIC GUIFunctions)

target_compile_definitions(test_fitting PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

qt_add_executable(test_barnes test_barnes.cpp)
target_include_directories(test_barnes PUBLIC "${PROJECT_SOURCE_DIR}/include")

target_link_libraries(test_barnes PUBLIC OLEDFunctions)

qt_add_executable(test_test test_test.cpp)
target_include_directories(test_test PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_test PUBLIC OLEDFunctions)

qt_add_executable(test_sim_input test_sim_input.cpp)
target_include_directories(test_sim_input PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_sim_input PUBLIC OLEDFunctions)

qt_add_executable(test_fit_input test_fit_input.cpp)
target_include_directories(test_fit_input PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(test_fit_input PUBLIC OLEDFunctions)