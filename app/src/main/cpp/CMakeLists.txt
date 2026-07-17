cmake_minimum_required(VERSION 3.22.1)
project(guitarfx)

find_package(oboe REQUIRED CONFIG)

add_library(guitarfx SHARED
        native-lib.cpp
        AudioEngine.cpp)

target_link_libraries(guitarfx
        oboe::oboe
        android
        log)
