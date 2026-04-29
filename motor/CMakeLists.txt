cmake_minimum_required(VERSION 3.8)
project(peripherals_motor_node)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_srvs REQUIRED)
find_package(peripherals REQUIRED)
find_package(Components REQUIRED)

# Explicitly find ethercat library since motor is a static lib and might not propagate it correctly
find_library(ETHERCAT_LIB ethercat REQUIRED)

add_executable(motor_node src/motor_node.cpp)
ament_target_dependencies(motor_node rclcpp std_srvs peripherals)

# motor is a static lib; using components::motor as per SDK spec.
target_link_libraries(motor_node
  -Wl,--whole-archive components::motor -Wl,--no-whole-archive
  ${ETHERCAT_LIB}
  pthread
)

install(TARGETS motor_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY config
  DESTINATION share/${PROJECT_NAME}
)

ament_package()



