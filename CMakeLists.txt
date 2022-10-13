cmake_minimum_required(VERSION 3.17)

project(mutator_api)
set(CMAKE_CXX_STANDARD 20)

option(USE_STANDALONE_ASIO "set ON to use standalone Asio instead of Boost.Asio" OFF)
if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    option(BUILD_TESTING "set ON to build library tests" ON)
else()
    option(BUILD_TESTING "set ON to build library tests" OFF)
endif()

add_library(mutator_api INTERFACE)

target_include_directories( mutator_api INTERFACE ${CMAKE_CURRENT_SOURCE_DIR} )

find_package(Threads REQUIRED)
target_link_libraries(mutator_api INTERFACE ${CMAKE_THREAD_LIBS_INIT})

set(CURL_LIBRARY "-lcurl")
find_package(CURL REQUIRED)

target_link_libraries(mutator_api INTERFACE ${CURL_LIBRARIES})

if(USE_STANDALONE_ASIO)
    target_compile_definitions(mutator_api INTERFACE ASIO_STANDALONE)
    find_path(ASIO_PATH asio.hpp)
    if(NOT ASIO_PATH)
        message(FATAL_ERROR "Standalone Asio not found")
    else()
        target_include_directories(mutator_api INTERFACE ${ASIO_PATH})
    endif()
else()
    find_package(Boost 1.54.0 COMPONENTS system thread coroutine context REQUIRED)
    target_link_libraries(mutator_api INTERFACE ${Boost_LIBRARIES})
    target_include_directories(mutator_api INTERFACE ${Boost_INCLUDE_DIR})
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 4.9)
        target_compile_definitions(mutator_api INTERFACE USE_BOOST_REGEX)
        find_package(Boost 1.54.0 COMPONENTS regex REQUIRED)
        target_link_libraries(mutator_api INTERFACE ${Boost_LIBRARIES})
        target_include_directories(mutator_api INTERFACE ${Boost_INCLUDE_DIR})
    endif()
endif()

if ( WIN32 )
    target_link_libraries(mutator_api INTERFACE ws2_32 wsock32)
endif ( )

if ( APPLE )
    set( OPENSSL_ROOT_DIR "/usr/local/opt/openssl" )
endif ( )

find_package(OpenSSL REQUIRED)
target_link_libraries(mutator_api INTERFACE ${OPENSSL_LIBRARIES})
target_include_directories(mutator_api INTERFACE ${OPENSSL_INCLUDE_DIR})

if(CMAKE_SOURCE_DIR STREQUAL "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT MSVC)
        add_compile_options(-std=c++11 -Wall -Wextra -Wsign-conversion)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            add_compile_options(-Wthread-safety)
        endif()
    else()
        add_compile_options(/W1)
    endif()

    add_executable(wss_server main.cpp mutator/inst.hpp mutator/)
    target_link_libraries(wss_server mutator_api)

    install(FILES mutator/helpers/ws/asio_compatibility.hpp mutator/helpers/ws/server_ws.hpp mutator/helpers/ws/client_ws.hpp mutator/helpers/ws/client_wss.hpp mutator/helpers/ws/server_wss.hpp mutator/helpers/ws/crypto.hpp mutator/helpers/ws/utility.hpp mutator/helpers/ws/status_code.hpp mutator/helpers/ws/mutex.hpp DESTINATION include/mutator)
endif()