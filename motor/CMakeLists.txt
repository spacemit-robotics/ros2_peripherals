cmake_minimum_required(VERSION 3.10)
project(peripherals)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(motor REQUIRED)

# Explicitly find ethercat library since motor is a static lib and might not propagate it correctly.
# Rely on CMAKE_PREFIX_PATH (set by build system) for portable library resolution.
find_library(ETHERCAT_LIB
  NAMES ethercat libethercat.so.1
  REQUIRED
)

# Generate messages and services
rosidl_generate_interfaces(${PROJECT_NAME}
  "config/MotorCommand.msg"
  "config/MotorCommandArray.msg"
  "config/MotorState.msg"
  "config/MotorStateArray.msg"
  "config/MotorGetParam.srv"
  "config/MotorSetParam.srv"
  DEPENDENCIES std_msgs
)

add_executable(motor_node src/motor_node.cpp)
target_link_libraries(motor_node
  -Wl,--whole-archive motor::motor -Wl,--no-whole-archive
  ${ETHERCAT_LIB}
  pthread
)

# Use ament_target_dependencies to handle ROS 2 dependencies
# NOTE: Do NOT include 'motor' here. It is already linked via
# target_link_libraries with --whole-archive, which is required for the
# __attribute__((constructor)) driver self-registration to work.
# Adding it again here would re-link without --whole-archive, causing
# the linker to discard driver .o files (no direct symbol references).
ament_target_dependencies(motor_node
  rclcpp
  std_msgs
  std_srvs
)

# Make motor_node depend on generated messages
rosidl_get_typesupport_target(cpp_typesupport_target "${PROJECT_NAME}" "rosidl_typesupport_cpp")
target_link_libraries(motor_node "${cpp_typesupport_target}")

install(TARGETS motor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()
