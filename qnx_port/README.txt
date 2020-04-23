Please following steps to build libua for QNX based platforms

1) Initialize submodules
   # git submodule init
   # git submodule update

2) Set location of the XL4BUS library build directory in config.cmk.tmpl
   export PORT=qnx_port
  
3) Set target build version 
   # export XL4_TARGET_OS=qnx660 or
   # export XL4_TARGET_OS=qnx700
  
4) Set target architecture
   # export XL4_TARGET_ARCH=armv7l or
   # export XL4_TARGET_ARCH=aarch64l
  
5) Load QNX build environment
   # source ~/qnx660/qnx660-env.sh or
   # source ~/qnx700/qnxsdp-env.sh
  
6) Build dependencies
   # cd qnx_port && ./build_deps.sh all clean && ./build_deps.sh all
  
7) Build library and application
   # cd qnx_port && ./build.sh clean && ./build.sh
