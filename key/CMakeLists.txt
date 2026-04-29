cmake_minimum_required(VERSION 3.8)
project(peripherals_key_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Threads REQUIRED)
find_package(Components QUIET)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/KeyEvent.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

set(KEY_COMPONENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../components/peripherals/key")
set(KEY_INCLUDE_DIR "")
set(KEY_LIB "")
set(KEY_OBJECTS "")
set(KEY_EXTRA_LINK_LIBS "")

if(TARGET components::key)
  set(KEY_LIB components::key)
endif()

if(NOT KEY_LIB)
  set(KEY_SEARCH_PREFIXES ${CMAKE_PREFIX_PATH})
  if(DEFINED ENV{CMAKE_PREFIX_PATH})
    string(REPLACE ":" ";" KEY_ENV_PREFIXES "$ENV{CMAKE_PREFIX_PATH}")
    list(APPEND KEY_SEARCH_PREFIXES ${KEY_ENV_PREFIXES})
  endif()
  list(REMOVE_DUPLICATES KEY_SEARCH_PREFIXES)
  list(LENGTH KEY_SEARCH_PREFIXES KEY_SEARCH_PREFIXES_LEN)

  find_path(KEY_INCLUDE_DIR key.h PATH_SUFFIXES include)
  find_library(KEY_LIB key)

  if((NOT KEY_INCLUDE_DIR OR NOT KEY_LIB) AND KEY_SEARCH_PREFIXES_LEN GREATER 0)
    find_path(KEY_INCLUDE_DIR key.h PATHS ${KEY_SEARCH_PREFIXES} PATH_SUFFIXES include)
    find_library(KEY_LIB key PATHS ${KEY_SEARCH_PREFIXES} PATH_SUFFIXES lib)
  endif()
endif()

if(NOT KEY_LIB AND EXISTS "${KEY_COMPONENT_DIR}/include/key.h" AND EXISTS "${KEY_COMPONENT_DIR}/src/key.c")
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(KEY_LIBGPIOD REQUIRED libgpiod)

  add_library(key_component_fallback OBJECT
    "${KEY_COMPONENT_DIR}/src/key.c"
  )
  target_include_directories(key_component_fallback PRIVATE
    "${KEY_COMPONENT_DIR}/include"
    ${KEY_LIBGPIOD_INCLUDE_DIRS}
  )
  target_compile_options(key_component_fallback PRIVATE ${KEY_LIBGPIOD_CFLAGS_OTHER})
  if(KEY_LIBGPIOD_VERSION VERSION_LESS "2.0.0")
    target_compile_definitions(key_component_fallback PRIVATE LIBGPIOD_V1)
  else()
    target_compile_definitions(key_component_fallback PRIVATE LIBGPIOD_V2)
  endif()

  set(KEY_INCLUDE_DIR "${KEY_COMPONENT_DIR}/include")
  set(KEY_OBJECTS $<TARGET_OBJECTS:key_component_fallback>)
  list(APPEND KEY_EXTRA_LINK_LIBS ${KEY_LIBGPIOD_LIBRARIES})
endif()

if(NOT KEY_LIB AND NOT KEY_OBJECTS)
  message(FATAL_ERROR
    "key.h or libkey not found. Build/install components/peripherals/key first, "
    "or keep its sources under components/peripherals/key for local fallback builds.")
endif()

add_executable(key_node
  src/key_node.cpp
  ${KEY_OBJECTS}
)
ament_target_dependencies(key_node rclcpp std_msgs)
if(KEY_INCLUDE_DIR)
  target_include_directories(key_node PRIVATE ${KEY_INCLUDE_DIR})
endif()
target_link_libraries(key_node
  ${KEY_LIB}
  ${KEY_EXTRA_LINK_LIBS}
  Threads::Threads
  "${cpp_typesupport_target}"
)

install(TARGETS key_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY params
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

install(FILES README.md
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_export_dependencies(rosidl_default_runtime std_msgs)
ament_package()
