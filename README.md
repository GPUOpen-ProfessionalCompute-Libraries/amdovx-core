# AMD OpenVX (AMDOVX)
AMD OpenVX (beta preview) is a highly optimized open source implementation of the [Khronos OpenVX](https://www.khronos.org/registry/vx/) computer vision specification. It allows for rapid prototyping as well as fast execution on a wide range of computer hardware, including small embedded x86 CPUs and large workstation discrete GPUs.

#### Features
* The code is highly optimized for both x86 CPU and OpenCL for GPU
* Supported hardware spans the range from low power embedded APUs (like the new G series) to laptop, desktop and workstation graphics
* Supports Windows, Linux, and OS X
* Includes a “graph optimizer” that looks at the entire processing pipeline and removes/replaces/merges functions to improve performance and minimize bandwidth at runtime 
* Scripting support allows for rapid prototyping, without re-compiling at production performance levels
* Interoperates with the popular (open source library) OpenCV

The current release verion is 0.9 (beta preview).

Build this project to generate AMD OpenVX library and RUNVX executable. 
* Refer to openvx/include/vx_ext_amd.h for extensions in AMD OpenVX library.
* Refer to runvx/README.md for RUNVX details. 

## Build Instructions

#### Pre-requisites
* CMake 2.8 or newer [download](http://cmake.org/download/).
* OpenCV 3.0 [download](http://opencv.org/downloads.html).
* OpenCV_DIR environment variable should point to OpenCV/build folder
* CPU: SSE4.1 or above CPU, 64-bit.
* GPU: Radeon R7 Series or above (Kaveri+ APU), Radeon 3xx Series or above (optional)
  * DRIVER: AMD Catalyst 15.7 or higher (version 15.20) with OpenCL 2.0 runtimes
  * AMD APP SDK 3.0 [download](http://developer.amd.com/tools-and-sdks/opencl-zone/amd-accelerated-parallel-processing-app-sdk/).
* libssl-dev on linux (optional)

#### Build using Visual Studio Professional 2013 on 64-bit Windows 10/8.1/7
* Use amdovx-core/amdovx.sln to build for x64 platform

#### Build using CMake
* Use CMake to configure and generate Makefile
