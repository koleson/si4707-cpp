# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_SDK_FETCH_FROM_GIT ON)

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

option(SI4707_WIZNET "Use Wiznet ethernet + MQTT client" OFF)
option(SI4707_PICO_W "Use Pico W Wi-Fi + MQTT client" OFF)

if (DEFINED ENV{PICO_BOARD})
    set(PICO_BOARD $ENV{PICO_BOARD})
    message(STATUS "Using PICO_BOARD from environment ('${PICO_BOARD}')")
else()
    if (NOT PICO_BOARD)
        set(PICO_BOARD "wiznet_w5100s_evb_pico")
        set(SI4707_WIZNET ON)
        message(STATUS "Defaulting Pico target board to ${PICO_BOARD} (unspecified)")
        pico_message("Si4707: Defaulting PICO target board to ${PICO_BOARD} since not specified.")
    elseif(PICO_BOARD STREQUAL wiznet_w5100s_evb_pico)
        message(STATUS "Si4707:  Wiznet EVB is the target.")
        set(SI4707_WIZNET ON)
    elseif(PICO_BOARD STREQUAL pico_w)   
        message(STATUS "Si4707: Pico W is the target.")
        set(SI4707_PICO_W ON)
    else()
        message(STATUS "Si4707: PICO target board is ${PICO_BOARD}.")
    endif()
endif()

set(PICO_BOARD ${PICO_BOARD} CACHE STRING "PICO target board (e.g. pico)" FORCE)

if (PICO_SDK_VERSION_STRING VERSION_LESS "1.4.0")
  message(FATAL_ERROR "Raspberry Pi Pico SDK version 1.4.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

project(si4707-cpp C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

file(GLOB MAIN_SOURCES src/main/*.c)
file(GLOB MAIN_HEADERS src/main/*.h)
file(GLOB SI4707_SOURCES src/si4707/*.c)
file(GLOB SI4707_HEADERS src/si4707/*.h)
file(GLOB SI4707_TEST_SOURCES src/test/si4707_driver_test.c)
file(GLOB SI4707_HAL_SOURCES src/hal/*.c)

add_executable(si4707-cpp 
                ${MAIN_SOURCES}
                ${SI4707_SOURCES}
                ${SI4707_HAL_SOURCES}
                )

add_executable(si4707_test_driver
                ${SI4707_SOURCES}
                ${SI4707_TEST_SOURCES}
                )

pico_set_program_name(si4707-cpp "si4707-cpp")
pico_set_program_version(si4707-cpp "0.1")

pico_enable_stdio_uart(si4707-cpp 1)
pico_enable_stdio_usb(si4707-cpp 0)


if (SI4707_WIZNET)
    message(STATUS "Wiznet networking selected")
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
    
    target_link_libraries(si4707-cpp
        ETHERNET_FILES
        IOLIBRARY_FILES
        MQTT_FILES
        DHCP_FILES
        DNS_FILES
        TIMER_FILES
    )

    target_include_directories(si4707-cpp PRIVATE
        src/net/wiznet
    )

    target_sources(si4707-cpp PRIVATE
        src/net/wiznet/mqtt-publisher.c
    )

    target_compile_definitions(si4707-cpp PRIVATE SI4707_WIZNET=1)

    # END:  WIZNET
elseif(SI4707_PICO_W)
    if (PICO_CYW43_SUPPORTED) # set by BOARD=pico-w
    message(STATUS "yay cyw43 supported")
    else()
    message(WARNING "cyw43 not supported by SDK but we're trying to build Pico W variant - check board type")
    endif()

    target_link_libraries(si4707-cpp
        pico_cyw43_arch_lwip_threadsafe_background
        pico_lwip_mbedtls
        pico_mbedtls
        pico_lwip_mqtt
    )
    target_compile_definitions(si4707-cpp PRIVATE SI4707_PICO_W=1)

    target_include_directories(si4707-cpp PRIVATE
      lwip_opts
    )
else()
    message(STATUS "falling back to pico board with no networking")
    # for now, Pico W is only alternative to Wiznet
    set(PICO_BOARD pico CACHE STRING "Board Type")
endif()



# Add the standard include files to the build
target_include_directories(si4707-cpp PRIVATE
  src/si4707
  src/main
  src/hal
  ${CMAKE_CURRENT_LIST_DIR}
  ${CMAKE_CURRENT_LIST_DIR}/.. # for our common lwipopts or any other standard includes, if required
)

target_link_libraries(si4707-cpp 
        pico_stdlib
        hardware_spi
        hardware_i2c
        hardware_timer
        hardware_clocks
        hardware_dma
	    pico_unique_id
        )


target_include_directories(si4707_test_driver PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/..
    src/si4707
    src/hal
)




target_link_libraries(si4707_test_driver
        pico_stdlib
        hardware_spi)

pico_add_extra_outputs(si4707-cpp)
pico_add_extra_outputs(si4707_test_driver)

# Set compile options
add_compile_options(
        -Wall
        )
