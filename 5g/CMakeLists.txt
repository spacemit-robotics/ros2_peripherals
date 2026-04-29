cmake_minimum_required(VERSION 3.8)
project(peripherals_5g_node)

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
  "msg/Modem5gIpInfo.msg"
  "msg/Modem5gPdpContext.msg"
  "msg/Modem5gStatus.msg"
  "srv/Modem5gDataCall.srv"
  "srv/Modem5gGetPdpContext.srv"
  "srv/Modem5gSendAt.srv"
  "srv/Modem5gSetFlightMode.srv"
  "srv/Modem5gSetPdpContext.srv"
  "srv/Modem5gSetPreferRat.srv"
  "srv/Modem5gTrigger.srv"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(modem_5g_node src/modem_5g_node.cpp)
ament_target_dependencies(modem_5g_node rclcpp std_msgs)

find_path(MODEM_5G_INCLUDE_DIR 5g.h PATH_SUFFIXES include)
find_library(MODEM_5G_LIB modem_5g)
if(NOT MODEM_5G_INCLUDE_DIR OR NOT MODEM_5G_LIB)
  message(FATAL_ERROR "5g.h or libmodem_5g not found. Build/install components/peripherals/5g first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(modem_5g_node PRIVATE ${MODEM_5G_INCLUDE_DIR})
target_link_libraries(modem_5g_node ${MODEM_5G_LIB} "${cpp_typesupport_target}")

install(TARGETS modem_5g_node
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
