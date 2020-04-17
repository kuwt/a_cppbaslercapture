cmake_minimum_required(VERSION 3.0.0)
project(baslerCapture VERSION 0.1.0)

# opencv
find_package( OpenCV REQUIRED )
find_package(pylon REQUIRED)

#add_definitions(-std=c++11)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")


add_library(baslerCapture SHARED			
"./src/baslerCapture.cpp"
"./src/test_baslerCapture.cpp"
)

target_link_libraries( baslerCapture "${OpenCV_LIBS}" "${PYLON_LIBRARIES})
