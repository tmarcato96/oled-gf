qt_add_library(GUIFunctions mainwindow.cpp ../include/mainwindow.h 
                            previewtab.cpp ../include/previewtab.h)

qt_add_executable(qtmain qtmain.cpp)

target_include_directories(GUIFunctions PUBLIC  "${PROJECT_SOURCE_DIR}/include"
                                                "${PROJECT_SOURCE_DIR}/gui/include")

target_link_libraries(GUIFunctions PUBLIC  oledgf_warnings
                                           PRIVATE Qt::Qwt
                                           PRIVATE Qt6::Widgets)


target_include_directories(qtmain PUBLIC  "${PROJECT_SOURCE_DIR}/include"
                                          "${PROJECT_SOURCE_DIR}/gui/include")

target_link_libraries(qtmain PUBLIC        oledgf_warnings
                                           PUBLIC GUIFunctions                                        
                                           PRIVATE Qt::Qwt
                                           PRIVATE Qt6::Widgets
                                           PRIVATE Qt6::Svg
                                           PRIVATE Qt6::Concurrent
                                           PRIVATE Qt6::OpenGL
                                           PRIVATE Qt6::PrintSupport)