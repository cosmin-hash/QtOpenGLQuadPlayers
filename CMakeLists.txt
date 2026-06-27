cmake_minimum_required(VERSION 3.21)
project(QtOpenGLQuadPlayers LANGUAGES CXX)

# Qt location is supplied by the builder, not hardcoded, so the project is
# portable. Provide it at configure time via either:
#   cmake -S . -B build -DCMAKE_PREFIX_PATH="<path>/Qt/<ver>/<compiler>"
# or by setting the CMAKE_PREFIX_PATH environment variable. Honour an existing
# environment value if one is set.
if(DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT DEFINED CMAKE_PREFIX_PATH)
  set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets)

add_executable(${PROJECT_NAME}
  main.cpp
  MainWindow.h MainWindow.cpp
  ViewportPane.h ViewportPane.cpp
  GLViewport.h GLViewport.cpp
  MediaController.h MediaController.cpp
  DecodeWorker.h DecodeWorker.cpp
  FrameQueue.h
  MediaIcons.h MediaIcons.cpp
)

target_link_libraries(QtOpenGLQuadPlayers PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  Qt6::OpenGL
  Qt6::OpenGLWidgets
)

# On Windows/MinGW, ensure correct subsystem for GUI apps
if(WIN32)
  set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

  # Deploy the Qt runtime (DLLs, plugins, MinGW runtime) next to the executable
  # after every build so it can be launched directly without Qt on PATH.
  get_target_property(_qmake_exe Qt6::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${_qmake_exe}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

  if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
      COMMAND "${WINDEPLOYQT_EXECUTABLE}"
              --$<IF:$<CONFIG:Debug>,debug,release>
              --compiler-runtime
              --no-translations
              "$<TARGET_FILE:${PROJECT_NAME}>"
      COMMENT "Deploying Qt runtime with windeployqt"
      VERBATIM)
  else()
    message(WARNING "windeployqt not found; Qt DLLs will not be deployed automatically.")
  endif()
endif()
