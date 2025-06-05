qt_add_library(GUIFunctions mainwindow.cpp ../include/mainwindow.h 
                            previewtab.cpp ../include/previewtab.h
                            threadhelper.cpp ../include/threadhelper.h)

qt_add_executable(qtmain qtmain.cpp)

target_include_directories(GUIFunctions PRIVATE  "${PROJECT_SOURCE_DIR}/include"
                                        PUBLIC   "${PROJECT_SOURCE_DIR}/gui/include")


target_link_libraries(GUIFunctions PRIVATE OLEDFunctions
                                           Qt::Qwt
                                           Qt6::Widgets
                                           Qt6::Svg
                                           Qt6::Concurrent
                                           Qt6::OpenGL
                                           Qt6::PrintSupport)


target_include_directories(qtmain PRIVATE  "${PROJECT_SOURCE_DIR}/include"
                                          "${PROJECT_SOURCE_DIR}/gui/include")

target_link_libraries(qtmain PRIVATE GUIFunctions)