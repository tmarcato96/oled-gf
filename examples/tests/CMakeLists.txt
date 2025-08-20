qt_add_executable(test_sweep_fit test_sweep_fit.cpp)
target_link_libraries(test_sweep_fit PUBLIC OLEDFunctions
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_fit PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

qt_add_executable(test_sweep_sim test_sweep_sim.cpp)
target_link_libraries(test_sweep_sim PUBLIC OLEDFunctions
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_sim PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")
