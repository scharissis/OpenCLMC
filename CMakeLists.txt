project(OpenCLMC)

cmake_minimum_required(VERSION 2.8.0)

find_package(OpenGL REQUIRED)

add_subdirectory(${CMAKE_SOURCE_DIR}/dep/glfw)

set(INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/dep/glfw/include
  ${CMAKE_SOURCE_DIR}/dep/opencl/inc
  ${CMAKE_SOURCE_DIR}/dep/glm
)

set(SRC_DIRS
  ${CMAKE_SOURCE_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/*.c
  ${CMAKE_SOURCE_DIR}/*.h
  ${CMAKE_SOURCE_DIR}/*.cl
)

include_directories(${INCLUDE_DIRS})
link_directories(${CMAKE_SOURCE_DIR}/dep/opencl/lib/Win32)
file(GLOB SRC_FILES ${SRC_DIRS})
add_executable(${CMAKE_PROJECT_NAME} ${SRC_FILES})
target_link_libraries(${CMAKE_PROJECT_NAME} glfw opencl ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES})
set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

file(GLOB KERNELS
  ${CMAKE_SOURCE_DIR}/*.cl
)
file(COPY ${KERNELS} DESTINATION ${CMAKE_SOURCE_DIR}/bin)