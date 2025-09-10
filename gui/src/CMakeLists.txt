qt_add_library(GUIFunctions STATIC 
                            mainwindow.cpp ../include/mainwindow.h 
                            previewtab.cpp ../include/previewtab.h
                            threadhelper.cpp ../include/threadhelper.h
                            configwindow.cpp ../include/configwindow.h)


target_include_directories(GUIFunctions PRIVATE  "${PROJECT_SOURCE_DIR}/include"
                                        PUBLIC   "${PROJECT_SOURCE_DIR}/gui/include")


target_link_libraries(GUIFunctions PRIVATE OLEDFunctions
                                   PRIVATE Eigen
                                   PRIVATE jsonsimplecpp
                                   PUBLIC Qt::Qwt
                                   PUBLIC Qt6::Widgets
                                   PUBLIC Qt6::Svg
                                   PUBLIC Qt6::Concurrent
                                   PUBLIC Qt6::OpenGL
                                   PUBLIC Qt6::PrintSupport
                                   PUBLIC Qt6::DBus)

qt_add_executable(qtmain MACOSX_BUNDLE qtmain.cpp)

set_target_properties(qtmain PROPERTIES 
OUTPUT_NAME "OLEDgf"
MACOSX_BUNDLE_BUNDLE_NAME "OLEDgf"
MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
MACOSX_BUNDLE_BUNDLE_VERSION      "${PROJECT_VERSION}"
)

target_include_directories(qtmain PRIVATE  "${PROJECT_SOURCE_DIR}/include"
                                           "${PROJECT_SOURCE_DIR}/gui/include")

target_link_libraries(qtmain PRIVATE GUIFunctions
                             PRIVATE Eigen
                             PRIVATE jsonsimplecpp)

install(TARGETS qtmain
BUNDLE DESTINATION .
RUNTIME DESTINATION bin
COMPONENT Runtime
)

install(DIRECTORY "${PROJECT_SOURCE_DIR}/mat/"
DESTINATION "OLEDgf.app/Contents/Resources/mat"
COMPONENT Runtime
)

if (APPLE)
find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
HINTS
      "$ENV{Qt6_DIR}/../../../bin"
      "$ENV{Qt5_DIR}/../../../bin"
      "/opt/homebrew/opt/qt/bin"
      "/usr/local/opt/qt/bin"
)

set(QWT_LIB_PATH "")
  if(TARGET Qt::Qwt)
    # Best-effort: imported targets often expose IMPORTED_LOCATION_* props
    get_target_property(_qwt_loc Qt::Qwt IMPORTED_LOCATION_RELEASE)
    if(NOT _qwt_loc)
      get_target_property(_qwt_loc Qt::Qwt IMPORTED_LOCATION) # fallback
    endif()
    if(_qwt_loc)
      set(QWT_LIB_PATH "${_qwt_loc}")
    else()
      # Fallback: find_library if above didn’t resolve
      find_library(QWT_LIB_PATH NAMES qwt qwt-qt6 qwt-qt5)
    endif()
  else()
    find_library(QWT_LIB_PATH NAMES qwt qwt-qt6 qwt-qt5)
  endif()

if(MACDEPLOYQT_EXECUTABLE)
    install(CODE
      "
      message(STATUS \"Running macdeployqt on \${CMAKE_INSTALL_PREFIX}/OLEDgf.app\")
      execute_process(
        COMMAND \"${MACDEPLOYQT_EXECUTABLE}\" \"\${CMAKE_INSTALL_PREFIX}/OLEDgf.app\" -verbose=2
        RESULT_VARIABLE _mdq_res
        OUTPUT_VARIABLE _mdq_out
        ERROR_VARIABLE  _mdq_err
      )
      if(NOT _mdq_res EQUAL 0)
        message(WARNING \"macdeployqt failed:\\n\${_mdq_out}\\n\${_mdq_err}\")
      endif()
      "
    )
  else()
    message(WARNING "macdeployqt not found; Qt frameworks/plugins may not be deployed.")
  endif()

  # Ensure Qwt (non-Qt dylib) is embedded (if macdeployqt didn’t do it)
  if(QWT_LIB_PATH)
    install(CODE
      "
      include(BundleUtilities)
      set(extra_libs \"${QWT_LIB_PATH}\")
      # fixup_bundle scans OLEDgf.app, copies extra libs and rewrites install names
      fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/OLEDgf.app\" \"\${extra_libs}\" \"\")
      "
    )
  endif()
endif()
