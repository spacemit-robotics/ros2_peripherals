cmake_minimum_required(VERSION 3.8)
project(peripherals_lidar_node)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_srvs REQUIRED)

# Find lidar library from components/peripherals/lidar
find_path(LIDAR_INCLUDE_DIR lidar.h PATH_SUFFIXES include)
find_library(LIDAR_LIB lidar)
if(NOT LIDAR_INCLUDE_DIR OR NOT LIDAR_LIB)
  message(FATAL_ERROR "lidar.h or liblidar not found. Build/install components/peripherals/lidar first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

add_executable(lidar_2d_node src/lidar_2d_node.cpp)
ament_target_dependencies(lidar_2d_node rclcpp sensor_msgs std_srvs)
target_include_directories(lidar_2d_node PRIVATE ${LIDAR_INCLUDE_DIR})
target_link_libraries(lidar_2d_node ${LIDAR_LIB})

install(TARGETS lidar_2d_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

install(DIRECTORY params
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
