cmake_minimum_required(VERSION 3.8)
project(peripherals_led_node)

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
  "msg/LedCommand.msg"
  "msg/LedState.msg"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(led_node src/led_node.cpp)
ament_target_dependencies(led_node rclcpp std_msgs)

find_path(LED_INCLUDE_DIR led.h PATH_SUFFIXES include)
find_library(LED_LIB led)
if(NOT LED_INCLUDE_DIR OR NOT LED_LIB)
  message(FATAL_ERROR "led.h or libled not found. Build/install components/peripherals/led first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(led_node PRIVATE ${LED_INCLUDE_DIR})
target_link_libraries(led_node ${LED_LIB} "${cpp_typesupport_target}")

install(TARGETS led_node
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
