cmake_minimum_required(VERSION 3.16)
project(aidetect CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_BUILD_TYPE Release)

if(NOT DEFINED ENV{LIBSOPHON_HOME})
    message(FATAL_ERROR "LIBSOPHON_HOME not set; run: source env/env.sh")
endif()

find_package(Boost 1.71 REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

set(LIBSOPHON_HOME $ENV{LIBSOPHON_HOME})
set(OPENCV_HOME $ENV{OPENCV_HOME})
set(FFMPEG_HOME $ENV{FFMPEG_HOME})

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/third_party
    ${LIBSOPHON_HOME}/include
    ${FFMPEG_HOME}/include
    ${OPENCV_HOME}/include/opencv4
    ${Boost_INCLUDE_DIRS}
)

link_directories(
    ${LIBSOPHON_HOME}/lib
    ${FFMPEG_HOME}/lib
    ${OPENCV_HOME}/lib
)

add_executable(aidetect src/main.cpp)

target_compile_definitions(aidetect PRIVATE USE_BMCV=1 USE_OPENCV=1 USE_FFMPEG=1)

target_link_libraries(aidetect PRIVATE
    bmcv bmrt bmlib yuv bmjpuapi bmjpulite
    opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio opencv_highgui opencv_freetype opencv_video
    avcodec avformat avutil swscale
    Boost::system
    OpenSSL::SSL OpenSSL::Crypto
    Threads::Threads
    dl
)

target_link_options(aidetect PRIVATE -rdynamic)

set_target_properties(aidetect PROPERTIES
    BUILD_RPATH "${LIBSOPHON_HOME}/lib;${FFMPEG_HOME}/lib;${OPENCV_HOME}/lib"
    INSTALL_RPATH "${LIBSOPHON_HOME}/lib;${FFMPEG_HOME}/lib;${OPENCV_HOME}/lib"
)

add_executable(model_probe tools/model_probe.cpp)
target_compile_definitions(model_probe PRIVATE USE_BMCV=1 USE_OPENCV=1 USE_FFMPEG=1)
target_link_libraries(model_probe PRIVATE
    bmcv bmrt bmlib yuv bmjpuapi bmjpulite
    opencv_core opencv_imgproc opencv_imgcodecs opencv_videoio opencv_highgui
    Boost::system OpenSSL::SSL OpenSSL::Crypto Threads::Threads dl)
set_target_properties(model_probe PROPERTIES
    BUILD_RPATH "${LIBSOPHON_HOME}/lib;${OPENCV_HOME}/lib"
    INSTALL_RPATH "${LIBSOPHON_HOME}/lib;${OPENCV_HOME}/lib")

add_executable(gen_label_plates tools/gen_label_plates.cpp)
target_link_libraries(gen_label_plates PRIVATE
    opencv_core opencv_imgproc opencv_imgcodecs opencv_freetype Threads::Threads)
set_target_properties(gen_label_plates PROPERTIES
    BUILD_RPATH "${OPENCV_HOME}/lib"
    INSTALL_RPATH "${OPENCV_HOME}/lib")