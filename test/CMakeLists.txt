cmake_minimum_required(VERSION 3.0)

project(main)

#配置编译选项
set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} -g)

#配置头文件搜索路径
#include _directories()

#配置库文件搜索路径     
#link_directories()

#设置需要编译的源文件列表
set(SRC_LIST testmuduo02.cpp)

add_executable(testmuduo02 ${SRC_LIST})
target_link_libraries(testmuduo02 muduo_net muduo_base pthread)

#设置生成的可执行文件的输出路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
