cmake_minimum_required(VERSION 2.8)
add_definitions(-D_GNU_SOURCE=1 -std=gnu11)

project(piworld)

set(CMAKE_VERBOSE_MAKEFILE TRUE)

FILE(GLOB SOURCE_FILES
    src/client.c src/config.c src/cube.c src/db.c src/door.c src/item.c
    src/fence.c src/main.c src/map.c src/matrix.c src/pwlua_api.c
    src/pwlua_worldgen.c src/pwlua.c src/ring.c src/sign.c src/ui.c src/util.c
    src/world.c
    deps/lodepng/lodepng.c
    deps/noise/noise.c
    deps/pg/*.c
    deps/tinycthread/tinycthread.c
    )
if(RASPI)
    list(APPEND SOURCE_FILES
        deps/RPi.GPIO/source/c_gpio.c deps/RPi.GPIO/source/cpuinfo.c
        deps/RPi.GPIO/source/event_gpio.c deps/RPi.GPIO/source/soft_pwm.c
        deps/rpi-gpio-lua/lua/darksidesync_aux.c
        deps/rpi-gpio-lua/lua/RPi_GPIO_Lua_module.c
        )
endif()

set(COMMON_LIBS dl m pthread X11 Xcursor Xi
     ${CMAKE_SOURCE_DIR}/deps/moonjit/src/libluajit.a)

if(SQLITE_BUILTIN)
    list(APPEND SOURCE_FILES deps/sqlite/sqlite3.c)
    include_directories(deps/sqlite)
else()
    list(APPEND COMMON_LIBS sqlite3)
endif()

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

include(ExternalProject)
ExternalProject_Add(
    moonjit
    GIT_REPOSITORY https://github.com/moonjit/moonjit.git
    # GIT_TAG 2.2.0 is f409987a39f7552dcf426ca60cca97c30df7dc62
    GIT_TAG f409987a39f7552dcf426ca60cca97c30df7dc62
    SOURCE_DIR ./deps/moonjit
    BUILD_IN_SOURCE TRUE
    CONFIGURE_COMMAND ""
    INSTALL_COMMAND ""
    BUILD_COMMAND make
)
add_dependencies(piworld moonjit)

add_definitions(-Wall -Wextra -Wstrict-prototypes)

if(RASPI EQUAL 1)
    add_definitions(-mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -DRASPI=1)
elseif(RASPI EQUAL 2)
    add_definitions(-march=armv7-a -mfpu=neon-vfpv4 -mfloat-abi=hard -DRASPI=2)
endif()

if(RASPI)
    add_definitions(-DRASPI=${RASPI})
endif()

if(PROFILE)
    add_definitions(-pg)
    set(CMAKE_EXE_LINKER_FLAGS -pg)
endif()

if(DEBUG)
    add_definitions(-DDEBUG=1 -Og -g)
else()
    add_definitions(-O3 -flto)
endif()

if(RELEASE)
    add_definitions(-DRELEASE_BUILD=1)
else()
    add_definitions(-DRELEASE_BUILD=0)
endif()

include_directories(deps/lodepng)
include_directories(deps/moonjit/src)
include_directories(deps/noise)
include_directories(deps/pg)
include_directories(deps/RPi.GPIO/source)
include_directories(deps/rpi-gpio-lua/lua)
include_directories(deps/tinycthread)

# Setup for make install
include(GNUInstallDirs)
set(PW_INSTALL_BINDIR ${CMAKE_INSTALL_FULL_BINDIR})
set(PW_INSTALL_DATADIR ${CMAKE_INSTALL_FULL_DATAROOTDIR}/piworld)
set(PW_INSTALL_DOCDIR ${CMAKE_INSTALL_FULL_DOCDIR}/piworld)
install(TARGETS piworld DESTINATION ${PW_INSTALL_BINDIR})
install(DIRECTORY shaders DESTINATION ${PW_INSTALL_DATADIR})
install(DIRECTORY textures DESTINATION ${PW_INSTALL_DATADIR})
install(DIRECTORY worldgen DESTINATION ${PW_INSTALL_DATADIR})
install(FILES LICENSE.md README.md DESTINATION ${PW_INSTALL_DOCDIR})
add_definitions(-DPW_INSTALL_BINDIR="${PW_INSTALL_BINDIR}")
add_definitions(-DPW_INSTALL_DATADIR="${PW_INSTALL_DATADIR}")
add_definitions(-DPW_INSTALL_DOCDIR="${PW_INSTALL_DOCDIR}")

