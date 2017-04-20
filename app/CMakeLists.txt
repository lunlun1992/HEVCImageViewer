# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)
# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( avcodec
             STATIC
             IMPORTED )
set_target_properties( # Specifies the target library.
                       avcodec

                       # Specifies the parameter you want to define.
                       PROPERTIES IMPORTED_LOCATION

                       # Provides the path to the library you want to import.
                       ${CMAKE_SOURCE_DIR}/src/main/libs/armeabi-v7a/libavcodec.a )

add_library(avutil
            STATIC
            IMPORTED )
set_target_properties( # Specifies the target library.
                      avutil

                      # Specifies the parameter you want to define.
                      PROPERTIES IMPORTED_LOCATION

                      # Provides the path to the library you want to import.
                      ${CMAKE_SOURCE_DIR}/src/main/libs/armeabi-v7a/libavutil.a )
add_library(swresample
          STATIC
          IMPORTED )
set_target_properties( # Specifies the target library.
                    swresample

                    # Specifies the parameter you want to define.
                    PROPERTIES IMPORTED_LOCATION

                    # Provides the path to the library you want to import.
                    ${CMAKE_SOURCE_DIR}/src/main/libs/armeabi-v7a/libswresample.a )
add_library(swscale
          STATIC
          IMPORTED )
set_target_properties( # Specifies the target library.
                    swscale

                    # Specifies the parameter you want to define.
                    PROPERTIES IMPORTED_LOCATION

                    # Provides the path to the library you want to import.
                    ${CMAKE_SOURCE_DIR}/src/main/libs/armeabi-v7a/libswscale.a )
find_library(z-lib z)
find_library(m-lib m)


# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library(log-lib log)
find_library(jnigraphic-lib jnigraphics)
find_library(GLES-lib GLESv2)
find_library(android-lib android)

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

add_library( # Sets the name of the library.
             imageshow

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             ${CMAKE_SOURCE_DIR}/src/main/cpp/native_show.cpp
             ${CMAKE_SOURCE_DIR}/src/main/cpp/ImageCodec.cpp)
include_directories(${CMAKE_SOURCE_DIR}/src/main/cpp/include/)
target_link_libraries(imageshow ${z-lib} ${m-lib} avcodec swresample swscale avutil ${log-lib} ${jnigraphic-lib} ${GLES-lib} ${android-lib})
