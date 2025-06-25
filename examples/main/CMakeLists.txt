add_executable(main main.cpp)

target_link_libraries(main PUBLIC 
                      OLEDFunctions 
                      oledgf_options 
                      oledgf_warnings
                      PRIVATE Eigen)