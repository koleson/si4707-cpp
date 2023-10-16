# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    MESSAGE(STATUS "pico sdk not set in environment - using macOS default")
    set(PICO_SDK_PATH "/Users/koleson/Documents/Development/pico-sdk")
else()
    MESSAGE(STATUS "Using system-defined pico SDK: $ENV{PICO_SDK_PATH}")
endif()

# set(PICO_BOARD pico CACHE STRING "Board type")
set(PICO_BOARD wiznet_w5100s_evb_pico CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(si4707-cpp C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# START:  WIZNET
# Set ethernet chip
set(WIZNET_CHIP W5500)

if(${WIZNET_CHIP} STREQUAL W5100S)
    add_definitions(-D_WIZCHIP_=W5100S)
elseif(${WIZNET_CHIP} STREQUAL W5500)
    add_definitions(-D_WIZCHIP_=W5500)
else()
    message(FATAL_ERROR "WIZNET_CHIP is wrong = ${WIZNET_CHIP}")
endif()

message(STATUS "WIZNET_CHIP = ${WIZNET_CHIP}")

# Set the project root directory if it's not already defined, as may happen if
# the tests folder is included directly by a parent project, without including
# the top level CMakeLists.txt.
if(NOT DEFINED WIZNET_DIR)
    set(WIZNET_DIR ${CMAKE_SOURCE_DIR}/libraries/ioLibrary_Driver)
    message(STATUS "WIZNET_DIR = ${WIZNET_DIR}")
endif()

# if(NOT DEFINED MBEDTLS_DIR)
#    set(MBEDTLS_DIR ${CMAKE_SOURCE_DIR}/libraries/mbedtls)
#    message(STATUS "MBEDTLS_DIR = ${MBEDTLS_DIR}")
# endif()

if(NOT DEFINED PORT_DIR)
    set(PORT_DIR ${CMAKE_SOURCE_DIR}/port)
    message(STATUS "PORT_DIR = ${PORT_DIR}")
endif()

# Turn off mbedtls test mode 
set(ENABLE_PROGRAMS OFF CACHE BOOL "Build mbedtls programs")
set(ENABLE_TESTING OFF CACHE BOOL "Build mbedtls testing")
add_definitions(-DMBEDTLS_CONFIG_FILE="${PORT_DIR}/mbedtls/inc/ssl_config.h")
add_definitions(-DSET_TRUSTED_CERT_IN_SAMPLES)

# Add libraries in subdirectories
add_subdirectory(${CMAKE_SOURCE_DIR}/libraries)
# add_subdirectory(${MBEDTLS_DIR})
add_subdirectory(${PORT_DIR})
    
# END:  WIZNET

# Add executable. Default name is the project name, version 0.1

add_executable(si4707-cpp 
                si4707-cpp.c
                bus_scan.c
                si4707.c
                mqtt-publisher.c
                )

pico_set_program_name(si4707-cpp "si4707-cpp")
pico_set_program_version(si4707-cpp "0.1")

pico_enable_stdio_uart(si4707-cpp 1)
pico_enable_stdio_usb(si4707-cpp 1)

# Add the standard library to the build
target_link_libraries(si4707-cpp
        pico_stdlib
        hardware_spi
        hardware_dma)

# Add the standard include files to the build
target_include_directories(si4707-cpp PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

# Add any user requested libraries
target_link_libraries(si4707-cpp 
        hardware_spi
        hardware_i2c
        hardware_timer
        hardware_clocks
        ETHERNET_FILES
        IOLIBRARY_FILES
        MQTT_FILES
        DHCP_FILES
        DNS_FILES
        TIMER_FILES
        )

pico_add_extra_outputs(si4707-cpp)

# Set compile options
add_compile_options(
        -Wall
        )
