cmake_minimum_required(VERSION 3.16)

project(AudioEnc LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    # ソースコードは各ファイルの形式（自動判別）、実行時の文字集合のみUTF-8にする場合
    add_compile_options(/execution-charset:utf-8)
endif()

# Source files
set(SOURCES
    src/AudioEnc.cpp
    src/resource.rc
)

# Header files (for IDE visibility)
set(HEADERS
    include/logger2.h
    include/module2.h
    include/output2.h
    include/resource.h
)

# Create the library
add_library(AudioEnc SHARED ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(AudioEnc PRIVATE include)

# Target properties
set_target_properties(AudioEnc PROPERTIES
    PREFIX ""
    SUFFIX ".auo2"
    OUTPUT_NAME "AudioEnc"
)

# Windows specific definitions
target_compile_definitions(AudioEnc PRIVATE
    WIN32
    _UNICODE
    UNICODE
    _CRT_SECURE_NO_WARNINGS
)

# Link libraries
target_link_libraries(AudioEnc PRIVATE
    comdlg32
    pathcch
)
