cmake_minimum_required(VERSION 3.8)
project(qrc_udriver)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

include_directories(
  include
)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -O0 -fPIC)
endif()

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

ament_auto_add_library(${PROJECT_NAME} SHARED
  src/qti_qrc_udriver.c
  src/qti_qrc_uart.c
)
SET_TARGET_PROPERTIES(qrc_udriver PROPERTIES VERSION 1.0)
target_link_libraries(qrc_udriver gpiod)

if(BUILD_TESTING)
  ament_auto_add_executable(${PROJECT_NAME}_test
  test/qti_qrc_test.c
  )
endif()

ament_auto_package()
