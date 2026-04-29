cmake_minimum_required(VERSION 3.8)
project(peripherals_wifi_node)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(std_srvs REQUIRED)

rosidl_generate_interfaces(${PROJECT_NAME}
  "msg/WifiScanResult.msg"
  "msg/WifiScanResults.msg"
  "msg/WifiStatus.msg"
  "srv/WifiConnect.srv"
  "srv/WifiScan.srv"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(wifi_node src/wifi_node.cpp)
ament_target_dependencies(wifi_node rclcpp std_msgs std_srvs)

find_path(WIFI_INCLUDE_DIR wifi.h PATH_SUFFIXES include)
find_library(WIFI_LIB wifi)
if(NOT WIFI_INCLUDE_DIR OR NOT WIFI_LIB)
  message(FATAL_ERROR "wifi.h or libwifi not found. Build/install components/peripherals/wifi first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(wifi_node PRIVATE ${WIFI_INCLUDE_DIR})
target_link_libraries(wifi_node ${WIFI_LIB} "${cpp_typesupport_target}")

install(TARGETS wifi_node
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
