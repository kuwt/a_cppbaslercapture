# GOAL:
This repo serves as the software interface operating one or more basler cameras


# Design feature
Design feature: 
1. a standalone class that can perform basler software trigger grabbing
2. With appropriate wrapping the class can perform hardware trigger grabbing
3. support multiple cams

# Organization
There are 3 projects in total. All src code is found in ./src
1. baslerCapture
2. baslerCaptureServer
3. baslerCaptureTestClient

baslerCapture is the core project. Normally you need this only.

If you want to setup the cameras as a server, run server project and use the testClient to test the server.

# Installation
Supports windows and linux.

### linux (support capturing only)
1. Install pylon suite
2. build and run
```
mkdir build
cd build
cmake ..
make
./baslerCapture
```
### windows (support capturing and capture server)
1. start baslerCapture.sln with vs2015]
2. put third party library to ./3rb_lib
2. build and run test_baslerCapture project

