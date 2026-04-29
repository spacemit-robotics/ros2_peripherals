
cmake_minimum_required(VERSION 3.8)
project(peripherals_imu_node)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Only need ament_cmake for ros2 run compatibility
find_package(ament_cmake REQUIRED)

# Find Fast DDS
find_package(fastcdr REQUIRED)
find_package(fastrtps REQUIRED)
find_package(sensor_msgs REQUIRED)

# Find IMU library - try multiple locations
set(IMU_FOUND FALSE)

# Method 1: Check CMAKE_INSTALL_PREFIX (colcon --merge-install)
if(EXISTS ${CMAKE_INSTALL_PREFIX}/include/imu.h)
  set(IMU_INCLUDE_DIRS ${CMAKE_INSTALL_PREFIX}/include)
  set(IMU_LIBRARY_DIRS ${CMAKE_INSTALL_PREFIX}/lib)
  set(IMU_LIBRARIES imu)
  set(IMU_FOUND TRUE)
  message(STATUS "Found IMU library in CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}")
endif()

# Method 2: Check AMENT_PREFIX_PATH
if(NOT IMU_FOUND AND DEFINED ENV{AMENT_PREFIX_PATH})
  string(REPLACE ":" ";" AMENT_PREFIX_LIST "$ENV{AMENT_PREFIX_PATH}")
  foreach(PREFIX_PATH ${AMENT_PREFIX_LIST})
    if(EXISTS ${PREFIX_PATH}/include/imu.h)
      set(IMU_INCLUDE_DIRS ${PREFIX_PATH}/include)
      set(IMU_LIBRARY_DIRS ${PREFIX_PATH}/lib)
      set(IMU_LIBRARIES imu)
      set(IMU_FOUND TRUE)
      message(STATUS "Found IMU library in AMENT_PREFIX_PATH: ${PREFIX_PATH}")
      break()
    endif()
  endforeach()
endif()

# Method 3: Fallback to local paths for in-tree build
if(NOT IMU_FOUND)
  set(IMU_COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../components/peripherals/imu)
  set(STAGING_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../../../output/staging/lib)
  if(EXISTS ${IMU_COMPONENT_DIR}/include/imu.h)
    set(IMU_INCLUDE_DIRS ${IMU_COMPONENT_DIR}/include)
    set(IMU_LIBRARY_DIRS ${STAGING_LIB_DIR})
    set(IMU_LIBRARIES imu)
    set(IMU_FOUND TRUE)
    message(STATUS "Found IMU library in local path: ${IMU_COMPONENT_DIR}")
  endif()
endif()

if(NOT IMU_FOUND)
  message(FATAL_ERROR "IMU library not found. Please build imu package first.")
endif()

link_directories(${IMU_LIBRARY_DIRS})

# IMU DDS implementation source files
set(IMU_IMPL_SOURCES
  src/impl/Time.cxx
  src/impl/TimePubSubTypes.cxx
  src/impl/Vector3.cxx
  src/impl/Vector3PubSubTypes.cxx
  src/impl/Quaternion.cxx
  src/impl/QuaternionPubSubTypes.cxx
  src/impl/Header.cxx
  src/impl/HeaderPubSubTypes.cxx
  src/impl/Imu.cxx
  src/impl/ImuPubSubTypes.cxx
  src/impl/ImuPublisher.cxx
  src/impl/ImuSubscriber.cxx
)

# Build shared library
add_library(${PROJECT_NAME}_lib SHARED
  ${IMU_IMPL_SOURCES}
)

target_include_directories(${PROJECT_NAME}_lib
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/impl
)

target_link_libraries(${PROJECT_NAME}_lib
  fastcdr
  fastrtps
)
ament_target_dependencies(${PROJECT_NAME}_lib
  sensor_msgs
)
# IMU UART node executable
add_executable(imu_uart_node
  src/imu_uart_node.cpp
)

target_include_directories(imu_uart_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src/impl
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${IMU_INCLUDE_DIRS}
)

target_link_libraries(imu_uart_node
  ${PROJECT_NAME}_lib
  ${IMU_LIBRARIES}
)

# Install library
install(TARGETS ${PROJECT_NAME}_lib
  EXPORT export_${PROJECT_NAME}_lib
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)

# Install public headers only
install(DIRECTORY include/
  DESTINATION include
)

# Install executable
install(TARGETS imu_uart_node
  DESTINATION lib/${PROJECT_NAME}
)

# Export library
ament_export_targets(export_${PROJECT_NAME}_lib HAS_LIBRARY_TARGET)
ament_export_dependencies(fastcdr fastrtps sensor_msgs)

ament_package()


