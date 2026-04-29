cmake_minimum_required(VERSION 3.8)
project(peripherals_misc_io_node)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/MiscIoCommand.msg"
  "msg/MiscIoEvent.msg"
  "msg/MiscIoState.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(misc_io_node src/misc_io_node.cpp)
ament_target_dependencies(misc_io_node rclcpp std_msgs)

find_path(MISC_IO_INCLUDE_DIR misc_io.h PATH_SUFFIXES include)
find_library(MISC_IO_LIB misc_io)
if(NOT MISC_IO_INCLUDE_DIR OR NOT MISC_IO_LIB)
  message(FATAL_ERROR "misc_io.h or libmisc_io not found. Build/install components/peripherals/misc_io first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(misc_io_node PRIVATE ${MISC_IO_INCLUDE_DIR})
target_link_libraries(misc_io_node ${MISC_IO_LIB} pthread "${cpp_typesupport_target}")

install(TARGETS misc_io_node
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
