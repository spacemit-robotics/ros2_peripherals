cmake_minimum_required(VERSION 3.8)
project(peripherals_light_sensor_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)

add_executable(light_sensor_node src/light_sensor_node.cpp)
ament_target_dependencies(light_sensor_node rclcpp sensor_msgs)

find_path(LIGHT_SENSOR_INCLUDE_DIR light_sensor.h PATH_SUFFIXES include)
find_library(LIGHT_SENSOR_LIB light_sensor)
if(NOT LIGHT_SENSOR_INCLUDE_DIR OR NOT LIGHT_SENSOR_LIB)
  message(FATAL_ERROR "light_sensor.h or liblight_sensor not found. Build/install components/peripherals/light_sensor first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

get_filename_component(LIGHT_SENSOR_LIB_DIR "${LIGHT_SENSOR_LIB}" DIRECTORY)
find_library(STK_ALPS_LIB stk_alps_lib
  PATHS "${LIGHT_SENSOR_LIB_DIR}"
  NO_DEFAULT_PATH
)
if(NOT STK_ALPS_LIB)
  message(FATAL_ERROR "libstk_alps_lib.a not found next to ${LIGHT_SENSOR_LIB}. Rebuild/install components/peripherals/light_sensor so the vendor STK archive is staged in lib/.")
endif()

target_include_directories(light_sensor_node PRIVATE ${LIGHT_SENSOR_INCLUDE_DIR})

# The vendored STK archive is not PIC; force a non-PIE final executable.
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_link_options(light_sensor_node PRIVATE -no-pie)
endif()

# light_sensor is a static lib; keep all objects to preserve driver registration.
target_link_libraries(light_sensor_node
  -Wl,--whole-archive ${LIGHT_SENSOR_LIB} -Wl,--no-whole-archive
  ${STK_ALPS_LIB}
)

install(TARGETS light_sensor_node
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

ament_package()
