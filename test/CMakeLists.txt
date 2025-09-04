qt_add_executable(test_sweep_fit test_sweep_fit.cpp)
target_link_libraries(test_sweep_fit PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_fit PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sweep_fit
         COMMAND test_sweep_fit)

qt_add_executable(test_sweep_sim test_sweep_sim.cpp)
target_link_libraries(test_sweep_sim PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_sim PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sweep_sim
         COMMAND test_sweep_sim)


qt_add_executable(test_sim_barnes test_sim_barnes.cpp)
target_link_libraries(test_sim_barnes PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sim_barnes PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sim_barnes
         COMMAND test_sim_barnes)

qt_add_executable(test_sweep_config test_sweep_config.cpp)
target_link_libraries(test_sweep_config PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_config PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sweep_config
         COMMAND test_sweep_config)

qt_add_executable(test_sweep_config_fit test_sweep_config_fit.cpp)
target_link_libraries(test_sweep_config_fit PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_config_fit PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sweep_config_fit
         COMMAND test_sweep_config_fit)

qt_add_executable(test_sweep_config_angle test_sweep_config_angle.cpp)
target_link_libraries(test_sweep_config_angle PUBLIC OLEDFunctions oledgf_options oledgf_warnings
                                    PRIVATE Qt::Qwt)
target_compile_definitions(test_sweep_config_angle PRIVATE PROJECT_ROOT="${CMAKE_SOURCE_DIR}")

add_test(NAME test_sweep_config_angle
         COMMAND test_sweep_config_angle)
