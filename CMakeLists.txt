cmake_minimum_required(VERSION 3.20)
project(ScreenRecorder)

set(CMAKE_CXX_FLAGS "-D__STDC_CONSTANT_MACROS")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(ScreenRecorder src/main.cpp src/ScreenRecorder.cpp)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVDEVICE_INCLUDE_DIR libavdevice/avdevice.h)
find_library(AVDEVICE_LIBRARY avdevice)

find_path(POSTPROC_INCLUDE_DIR libpostproc/postprocess.h)
find_library(POSTPROC_LIBRARY postproc)

find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
find_library(SWRESAMPLE_LIBRARY swresample)

find_path(SWSCALE_INCLUDE_DIR libswscale/swscale.h)
find_library(SWSCALE_LIBRARY swscale)

target_include_directories(ScreenRecorder PRIVATE ${AVCODEC_INCLUDE_DIR} ${AVFORMAT_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVDEVICE_INCLUDE_DIR} ${POSTPROC_INCLUDE_DIR} ${SWRESAMPLE_INCLUDE_DIR} ${SWSCALE_INCLUDE_DIR})
target_link_libraries(ScreenRecorder PRIVATE ${AVCODEC_LIBRARY} ${AVFORMAT_LIBRARY} ${AVUTIL_LIBRARY} ${AVDEVICE_LIBRARY} ${POSTPROC_LIBRARY} ${SWRESAMPLE_LIBRARY} ${SWSCALE_LIBRARY}${ScreenRecorder_LINK_LIBRARIES} X11)

