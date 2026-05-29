cmake_minimum_required(VERSION 3.16)

project(plotjuggler_unitree_sdk2
  VERSION 0.1.0
  LANGUAGES CXX)

option(BUILD_PLUGIN "Build the PlotJuggler Unitree SDK2 Plugin shared libraries" ON)
option(BUILD_TESTING "Build smoke tests" ON)
option(PJ_UNITREE_BUILD_BUNDLE "Copy plugin runtime files into PJ_UNITREE_BUNDLE_DIRECTORY after build" ON)
option(PJ_UNITREE_COPY_ROBOT_ASSETS "Copy Unitree robot URDF and mesh assets into the runtime bundle" ON)
set(UNITREE_SDK2_ROOT "" CACHE PATH "Path to a unitree_sdk2 checkout")
set(PJ_PLUGIN_INSTALL_DIRECTORY "" CACHE PATH "Directory where PlotJuggler loads plugins from")
set(PJ_UNITREE_BUNDLE_DIRECTORY
  "${CMAKE_CURRENT_SOURCE_DIR}/bundle/plotjuggler_unitree_sdk2"
  CACHE PATH "Directory where the self-contained plugin bundle is generated")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)

function(resolve_unitree_sdk2_dependency)
  if(TARGET unitree_sdk2)
    return()
  endif()

  set(_unitree_root "")
  if(UNITREE_SDK2_ROOT)
    set(_unitree_root "${UNITREE_SDK2_ROOT}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_sdk2/CMakeLists.txt")
    set(_unitree_root "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_sdk2")
  endif()

  if(_unitree_root)
    if(NOT EXISTS "${_unitree_root}/CMakeLists.txt")
      message(FATAL_ERROR "UNITREE_SDK2_ROOT does not contain a CMakeLists.txt: ${_unitree_root}")
    endif()
    set(UNITREE_SDK2_RESOLVED_ROOT "${_unitree_root}" PARENT_SCOPE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "Build Unitree SDK examples" FORCE)
    add_subdirectory("${_unitree_root}" "${CMAKE_CURRENT_BINARY_DIR}/unitree_sdk2")
  else()
    find_package(unitree_sdk2 CONFIG REQUIRED)
  endif()
endfunction()

resolve_unitree_sdk2_dependency()

function(resolve_plotjuggler_dependency out_link_target)
  find_package(plotjuggler QUIET)
  if(NOT plotjuggler_FOUND)
    find_package(PlotJuggler QUIET)
  endif()

  if(PlotJuggler_FOUND AND NOT plotjuggler_FOUND)
    set(plotjuggler_INCLUDE_DIRS ${PlotJuggler_INCLUDE_DIRS})
    set(plotjuggler_INCLUDE_DIR ${PlotJuggler_INCLUDE_DIR})
    set(plotjuggler_LIBRARIES ${PlotJuggler_LIBRARIES})
  endif()

  if(NOT plotjuggler_FOUND AND NOT PlotJuggler_FOUND)
    message(FATAL_ERROR
      "Could not find PlotJuggler development files.\n"
      "Set CMAKE_PREFIX_PATH or PLOTJUGGLER_PREFIX to a prefix containing "
      "lib/cmake/plotjuggler/plotjugglerConfig.cmake.\n"
      "Local example:\n"
      "  export PLOTJUGGLER_PREFIX=/snap/plotjuggler/current/usr/local\n"
      "  cmake --preset dev")
  endif()

  if(TARGET plotjuggler::plotjuggler_base)
    set(${out_link_target} plotjuggler::plotjuggler_base PARENT_SCOPE)
  elseif(TARGET PlotJuggler::plotjuggler_base)
    set(${out_link_target} PlotJuggler::plotjuggler_base PARENT_SCOPE)
  elseif(plotjuggler_LIBRARIES)
    set(${out_link_target} ${plotjuggler_LIBRARIES} PARENT_SCOPE)
  else()
    message(FATAL_ERROR
      "PlotJuggler was found, but no plotjuggler_base target or library was exported.")
  endif()

  foreach(_plotjuggler_var
          plotjuggler_INCLUDE_DIRS
          plotjuggler_INCLUDE_DIR
          plotjuggler_LIBRARIES)
    if(DEFINED ${_plotjuggler_var})
      set(${_plotjuggler_var} "${${_plotjuggler_var}}" PARENT_SCOPE)
    endif()
  endforeach()
endfunction()

add_library(unitree_message_flatten STATIC
  src/unitree_message_flatten.cpp)
target_include_directories(unitree_message_flatten
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(unitree_message_flatten PUBLIC unitree_sdk2)

if(BUILD_PLUGIN)
  set(CMAKE_AUTOMOC ON)

  find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Xml)
  find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Xml)
  if(QT_VERSION_MAJOR EQUAL 6)
    find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    set(_qt_opengl_widgets_target Qt6::OpenGLWidgets)
  else()
    set(_qt_opengl_widgets_target "")
  endif()
  resolve_plotjuggler_dependency(_plotjuggler_link_target)

  set(_unitree_go2_assets_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_ros_assets/robots/go2_description")
  set(_unitree_robots_assets_dir
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_ros_assets/robots")

  add_library(plotjuggler_unitree_sdk2 SHARED
    src/unitree_datastreamer.cpp
    src/unitree_datastreamer.h)

  target_link_libraries(plotjuggler_unitree_sdk2
    PRIVATE
      unitree_message_flatten
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::Xml
      ${_plotjuggler_link_target})
  target_include_directories(plotjuggler_unitree_sdk2
    PRIVATE
      ${plotjuggler_INCLUDE_DIRS}
      ${plotjuggler_INCLUDE_DIR})

  add_library(plotjuggler_unitree_robot_view SHARED
    src/unitree_robot_view_toolbox.cpp
    src/unitree_robot_view_toolbox.h)
  target_link_libraries(plotjuggler_unitree_robot_view
    PRIVATE
      Qt${QT_VERSION_MAJOR}::Core
      Qt${QT_VERSION_MAJOR}::Widgets
      Qt${QT_VERSION_MAJOR}::Xml
      ${_qt_opengl_widgets_target}
      ${_plotjuggler_link_target}
      ${CMAKE_DL_LIBS})
  target_include_directories(plotjuggler_unitree_robot_view
    PRIVATE
      ${plotjuggler_INCLUDE_DIRS}
      ${plotjuggler_INCLUDE_DIR})
  target_compile_definitions(plotjuggler_unitree_robot_view
    PRIVATE
      PJ_UNITREE_GO2_SOURCE_DIR="${_unitree_go2_assets_dir}")

  set(_unitree_runtime_library_dir "")
  if(TARGET ddsc)
    get_target_property(_ddsc_location ddsc IMPORTED_LOCATION)
    if(_ddsc_location)
      get_filename_component(_unitree_runtime_library_dir "${_ddsc_location}" DIRECTORY)
    endif()
  endif()
  if(_unitree_runtime_library_dir)
    foreach(_plotjuggler_unitree_plugin
            plotjuggler_unitree_sdk2
            plotjuggler_unitree_robot_view)
      set_target_properties(${_plotjuggler_unitree_plugin} PROPERTIES
        BUILD_WITH_INSTALL_RPATH TRUE
        INSTALL_RPATH "$ORIGIN")
      target_link_options(${_plotjuggler_unitree_plugin}
        PRIVATE
          "-Wl,--disable-new-dtags")
    endforeach()
  endif()

  if(PJ_UNITREE_BUILD_BUNDLE)
    add_custom_command(TARGET plotjuggler_unitree_sdk2 POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PJ_UNITREE_BUNDLE_DIRECTORY}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "$<TARGET_FILE:plotjuggler_unitree_sdk2>"
              "${PJ_UNITREE_BUNDLE_DIRECTORY}/$<TARGET_FILE_NAME:plotjuggler_unitree_sdk2>")

    add_custom_command(TARGET plotjuggler_unitree_robot_view POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${PJ_UNITREE_BUNDLE_DIRECTORY}"
      COMMAND "${CMAKE_COMMAND}" -E copy_if_different
              "$<TARGET_FILE:plotjuggler_unitree_robot_view>"
              "${PJ_UNITREE_BUNDLE_DIRECTORY}/$<TARGET_FILE_NAME:plotjuggler_unitree_robot_view>")

    if(PJ_UNITREE_COPY_ROBOT_ASSETS)
      foreach(_unitree_robot_asset_dir
              a1_description
              aliengo_description
              b1_description
              b2_description
              b2w_description
              go1_description
              go2_description
              go2w_description
              g1_description
              g1_d_description
              h1_description
              h1_2_description
              h2_description
              r1_description
              r1_air_description)
        if(EXISTS "${_unitree_robots_assets_dir}/${_unitree_robot_asset_dir}")
          add_custom_command(TARGET plotjuggler_unitree_robot_view POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory
                    "${PJ_UNITREE_BUNDLE_DIRECTORY}/assets/robots"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${_unitree_robots_assets_dir}/${_unitree_robot_asset_dir}"
                    "${PJ_UNITREE_BUNDLE_DIRECTORY}/assets/robots/${_unitree_robot_asset_dir}")
        endif()
      endforeach()
    endif()

    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_ros_assets/LICENSE")
      add_custom_command(TARGET plotjuggler_unitree_robot_view POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory
                "${PJ_UNITREE_BUNDLE_DIRECTORY}/licenses"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/third_party/unitree_ros_assets/LICENSE"
                "${PJ_UNITREE_BUNDLE_DIRECTORY}/licenses/unitree_ros_LICENSE")
    endif()

    if(TARGET ddsc AND TARGET ddscxx)
      get_target_property(_ddsc_location ddsc IMPORTED_LOCATION)
      get_target_property(_ddscxx_location ddscxx IMPORTED_LOCATION)
      if(_ddsc_location AND _ddscxx_location)
        add_custom_command(TARGET plotjuggler_unitree_sdk2 POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_ddsc_location}"
                  "${PJ_UNITREE_BUNDLE_DIRECTORY}/libddsc.so.0"
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_ddsc_location}"
                  "${PJ_UNITREE_BUNDLE_DIRECTORY}/libddsc.so"
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_ddscxx_location}"
                  "${PJ_UNITREE_BUNDLE_DIRECTORY}/libddscxx.so.0"
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_ddscxx_location}"
                  "${PJ_UNITREE_BUNDLE_DIRECTORY}/libddscxx.so")

        if(UNITREE_SDK2_RESOLVED_ROOT)
          add_custom_command(TARGET plotjuggler_unitree_sdk2 POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${PJ_UNITREE_BUNDLE_DIRECTORY}/licenses"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${UNITREE_SDK2_RESOLVED_ROOT}/LICENSE"
                    "${PJ_UNITREE_BUNDLE_DIRECTORY}/licenses/unitree_sdk2_LICENSE"
            COMMAND "${CMAKE_COMMAND}" -E copy_directory
                    "${UNITREE_SDK2_RESOLVED_ROOT}/licenses"
                    "${PJ_UNITREE_BUNDLE_DIRECTORY}/licenses/unitree_sdk2_thirdparty")
        endif()
      endif()
    endif()
  endif()

  if(PJ_PLUGIN_INSTALL_DIRECTORY)
    set(_plugin_install_dir "${PJ_PLUGIN_INSTALL_DIRECTORY}")
  else()
    set(_plugin_install_dir "${CMAKE_INSTALL_LIBDIR}/plotjuggler/plugins")
  endif()

  install(TARGETS plotjuggler_unitree_sdk2 plotjuggler_unitree_robot_view
    LIBRARY DESTINATION "${_plugin_install_dir}")
endif()

if(BUILD_TESTING)
  include(CTest)
  if(BUILD_TESTING)
    add_executable(unitree_message_flatten_smoke
      tests/unitree_message_flatten_smoke.cpp)
    target_link_libraries(unitree_message_flatten_smoke
      PRIVATE unitree_message_flatten)
    add_test(NAME unitree_message_flatten_smoke
      COMMAND unitree_message_flatten_smoke)
  endif()
endif()

install(TARGETS unitree_message_flatten
  EXPORT plotjuggler_unitree_sdk2Targets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")

install(DIRECTORY include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
