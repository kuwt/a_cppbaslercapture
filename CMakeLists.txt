cmake_minimum_required(VERSION 3.0.0)
project(baslerCapture VERSION 0.1.0)

# opencv
find_package( OpenCV REQUIRED )

#add_definitions(-std=c++11)

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")


link_directories("/opt/pylon/lib")

add_executable(baslerCapture			
"./src/baslerCapture.cpp"
"./src/test_baslerCapture.cpp"
)

target_include_directories(baslerCapture PRIVATE "/opt/pylon/include")

target_link_libraries( baslerCapture 
"${OpenCV_LIBS}"
pylonutility 
pylonbase
GCBase_gcc_v3_1_Basler_pylon)

