cmake_minimum_required(VERSION 3.1.0)

project(soundextract)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -std=c++17")

if(CMAKE_VERSION VERSION_LESS "3.7.0")
    set(CMAKE_INCLUDE_CURRENT_DIR ON)
endif()
include_directories(.) #  or include_directory(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(ogg/include) #  or include_directory(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(vorbis/include) #  or include_directory(${CMAKE_CURRENT_SOURCE_DIR})

find_package(Qt5 COMPONENTS Core Gui Widgets REQUIRED)


set(SOURCES 
    codebook.cpp
    crc.cpp 
    main.cpp 
    revorb.cpp 
    soundextract.cpp 
    soundextract.ui 
    tinyxml2.cpp 
    wwriff.cpp 
)
add_subdirectory(ogg)
add_subdirectory(vorbis)
add_executable(soundextract ${SOURCES})

target_link_libraries(soundextract Qt5::Core Qt5::Gui Qt5::Widgets ogg vorbis)
