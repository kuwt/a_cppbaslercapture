# GOAL:
This repo serves as the software interface operating one or more basler cameras


# Design feature
Design feature: 
1. a standalone class that can perform basler software trigger grabbing
2. With appropriate wrapping the class can perform hardware trigger grabbing
3. support multiple cams

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
1. start baslerCapture.sln with vs2015
2. run test_baslerCapture project

# Development
refers to the test code as the starting point.