cmake_minimum_required(VERSION 3.0.0)
project(baslerCapture VERSION 0.1.0)

# opencv
find_package( OpenCV REQUIRED )

# pylon
find_package(Pylon QUIET)
if (NOT ${Pylon_FOUND})
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FindPylon.cmake")
endif()

message(STATUS "Pylon_INCLUDE_DIRS = " ${Pylon_INCLUDE_DIRS})
message(STATUS "Pylon_LIBRARIES = " ${Pylon_LIBRARIES})
message(STATUS "OpenCV_LIBS = " ${OpenCV_LIBS})

#add_definitions(-std=c++11)
SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

include_directories(
"${Pylon_INCLUDE_DIRS}"
)

add_executable(baslerCapture			
"./src/baslerCapture.cpp"
"./src/test_baslerCapture.cpp"
)

target_link_libraries( baslerCapture 
"${OpenCV_LIBS}"
"${Pylon_LIBRARIES}"
)

