cmake_minimum_required(VERSION 2.8)

project(OpenCLMC)

set(CMAKE_MODULE_PATH
	${CMAKE_MODULE_PATH}
	${CMAKE_SOURCE_DIR}/cmake/modules
)

find_package(OpenGL REQUIRED)
find_package(OpenCL REQUIRED)

# glfw library
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Build the GLFW example programs")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Build the GLFW test programs")
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Build the GLFW documentation")
set(GLFW_INSTALL OFF CACHE BOOL "Generate installation target")
add_subdirectory(${CMAKE_SOURCE_DIR}/dep/glfw)

if(APPLE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -std=c++11 -stdlib=libc++")
elseif(CMAKE_COMPILER_IS_GNUCXX OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
endif()

set(INCLUDE_DIRS
  ${CMAKE_SOURCE_DIR}/dep/glfw/include
  ${CMAKE_SOURCE_DIR}/dep/glm
  ${OPENCL_INCLUDE_DIRS}
)

include_directories(${INCLUDE_DIRS})

set(SRC_DIRS
  ${CMAKE_SOURCE_DIR}/*.cpp
  ${CMAKE_SOURCE_DIR}/*.c
  ${CMAKE_SOURCE_DIR}/*.h
  ${CMAKE_SOURCE_DIR}/*.cl
)

file(GLOB SRC_FILES ${SRC_DIRS})

add_executable(${CMAKE_PROJECT_NAME} ${SRC_FILES})

target_link_libraries(${CMAKE_PROJECT_NAME} glfw ${GLFW_LIBRARIES} ${OPENGL_LIBRARIES} ${OPENCL_LIBRARIES})

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES 
 	RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_SOURCE_DIR}" 
 	RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}"
 )