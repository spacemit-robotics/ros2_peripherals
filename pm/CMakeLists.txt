cmake_minimum_required(VERSION 3.8)
project(peripherals_pm_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/PowerStatus.msg"
  "srv/SetPowerSwitch.srv"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(pm_node src/pm_node.cpp)
ament_target_dependencies(pm_node rclcpp std_msgs)

find_path(PM_INCLUDE_DIR pm.h PATH_SUFFIXES include)
find_library(PM_LIB pm)
if(NOT PM_INCLUDE_DIR OR NOT PM_LIB)
  message(FATAL_ERROR "pm.h or libpm not found. Build/install components/peripherals/pm first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(pm_node PRIVATE ${PM_INCLUDE_DIR})
target_link_libraries(pm_node ${PM_LIB} pthread "${cpp_typesupport_target}")

install(TARGETS pm_node
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
