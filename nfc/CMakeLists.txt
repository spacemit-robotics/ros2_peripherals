cmake_minimum_required(VERSION 3.8)
project(peripherals_nfc_node)

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
  "msg/NfcTagInfo.msg"
  "srv/NfcPoll.srv"
  "srv/NfcReadBlock.srv"
  "srv/NfcWriteBlock.srv"
  DEPENDENCIES std_msgs
)

rosidl_get_typesupport_target(cpp_typesupport_target ${PROJECT_NAME} "rosidl_typesupport_cpp")

add_executable(nfc_node src/nfc_node.cpp)
ament_target_dependencies(nfc_node rclcpp std_msgs)

find_path(NFC_INCLUDE_DIR nfc.h PATH_SUFFIXES include)
find_library(NFC_LIB nfc)
if(NOT NFC_INCLUDE_DIR OR NOT NFC_LIB)
  message(FATAL_ERROR "nfc.h or libnfc not found. Build/install components/peripherals/nfc first (PREFIX must be in CMAKE_PREFIX_PATH).")
endif()

target_include_directories(nfc_node PRIVATE ${NFC_INCLUDE_DIR})
target_link_libraries(nfc_node ${NFC_LIB} "${cpp_typesupport_target}")

install(TARGETS nfc_node
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
