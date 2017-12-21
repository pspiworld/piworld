cmake_minimum_required(VERSION 2.8)
add_definitions(-D_GNU_SOURCE=1 -std=gnu11)

project(piworld)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

FILE(GLOB SOURCE_FILES
    src/*.c
    deps/lodepng/lodepng.c
    deps/noise/noise.c
    deps/pg/*.c
    deps/tinycthread/tinycthread.c
    )

set(COMMON_LIBS dl m pthread sqlite3 X11 Xcursor)

if(MESA)
    # The MESA driver is only for the pi2/3.
    add_definitions(-DMESA=1)
    add_executable(piworld ${SOURCE_FILES})
    target_link_libraries(piworld ${COMMON_LIBS} EGL GLESv2)
else()
    include_directories(/opt/vc/include)
    link_directories(/opt/vc/lib)
    add_executable(piworld ${SOURCE_FILES})
    target_link_libraries(piworld ${COMMON_LIBS} brcmEGL brcmGLESv2
        bcm_host openmaxil vcos vchiq_arm)
endif()

if(RASPI EQUAL 1)
    add_definitions(-mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard)
elseif(RASPI EQUAL 2)
    add_definitions(-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard)
endif()

if(PROFILE)
    add_definitions(-pg)
    set(CMAKE_EXE_LINKER_FLAGS -pg)
endif()

if(DEBUG)
    add_definitions(-DDEBUG=1 -Og -g)
else()
    add_definitions(-O3)
endif()

if(RELEASE)
    add_definitions(-DRELEASE=1)
endif()

include_directories(deps/lodepng)
include_directories(deps/noise)
include_directories(deps/pg)
include_directories(deps/tinycthread)

