cmake_minimum_required(VERSION 3.28)
project(obs-ccu VERSION 0.2.4 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets)

if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
  get_target_property(_opengl_links WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
  if(_opengl_links)
    list(FILTER _opengl_links EXCLUDE REGEX "AGL")
    set_property(TARGET WrapOpenGL::WrapOpenGL
                 PROPERTY INTERFACE_LINK_LIBRARIES "${_opengl_links}")
  endif()
endif()

add_library(obs-ccu MODULE
  src/ccu-filter.cpp
  src/ccu-window.cpp
  src/ccu-window.hpp
  src/obs-display-widget.cpp
  src/obs-display-widget.hpp
  src/scope-data.cpp
  src/scope-data.hpp
  src/scope-widget.cpp
  src/scope-widget.hpp
)

target_link_libraries(obs-ccu PRIVATE OBS::libobs OBS::obs-frontend-api Qt6::Widgets)
target_compile_definitions(obs-ccu PRIVATE CCU_VERSION="${PROJECT_VERSION}")

if(APPLE)
  set_target_properties(obs-ccu PROPERTIES
    BUNDLE TRUE
    BUNDLE_EXTENSION plugin
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/packaging/Info.plist.in"
    OUTPUT_NAME "obs-ccu"
  )
elseif(WIN32)
  set_target_properties(obs-ccu PROPERTIES PREFIX "" OUTPUT_NAME "obs-ccu")
else()
  message(FATAL_ERROR "The first CCU build supports macOS and Windows")
endif()

if(APPLE)
  # $<TARGET_BUNDLE_CONTENT_DIR:...> is only meaningful for a BUNDLE target;
  # on Windows the data files are staged by the packaging step instead.
  add_custom_command(TARGET obs-ccu POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_CONTENT_DIR:obs-ccu>/Resources"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/data/ccu-color.effect"
            "$<TARGET_BUNDLE_CONTENT_DIR:obs-ccu>/Resources/ccu-color.effect"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/data/locale"
            "$<TARGET_BUNDLE_CONTENT_DIR:obs-ccu>/Resources/locale"
    VERBATIM
  )
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(scope-data-tests tests/scope-data.test.cpp src/scope-data.cpp)
  target_include_directories(scope-data-tests PRIVATE src)
  target_compile_features(scope-data-tests PRIVATE cxx_std_17)
  add_test(NAME scope-data-tests COMMAND scope-data-tests)
endif()

if(BUILD_TESTING AND APPLE)
  add_executable(plugin-smoke tests/plugin-smoke.mm)
  target_compile_options(plugin-smoke PRIVATE -fobjc-arc)
  target_link_libraries(plugin-smoke PRIVATE OBS::libobs Qt6::Widgets
                                             "-framework AppKit")
  add_test(NAME plugin-smoke
    COMMAND plugin-smoke
      "/Applications/OBS.app/Contents/Frameworks/libobs-opengl.dylib"
      "$<TARGET_BUNDLE_DIR:obs-ccu>/Contents/MacOS/obs-ccu"
      "$<TARGET_BUNDLE_DIR:obs-ccu>/Contents/Resources")
  set_tests_properties(plugin-smoke PROPERTIES
    ENVIRONMENT
      "DYLD_FRAMEWORK_PATH=/Applications/OBS.app/Contents/Frameworks;DYLD_LIBRARY_PATH=/Applications/OBS.app/Contents/Frameworks")
endif()
