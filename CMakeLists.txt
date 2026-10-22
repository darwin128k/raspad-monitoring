cmake_minimum_required(VERSION 3.16)
project(monitoring C)

# Authum client module (ABI 0.3), pure C11 / Linux i386 (-m32).
# Answers server-browser / monitoring queries the Steam GS layer would
# otherwise gate behind an A2S challenge. No HLSDK / metamod-p.

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# VERSION is the one place the module version lives; info.c gets it from here.
file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" MONITORING_VERSION LIMIT_COUNT 1)
string(STRIP "${MONITORING_VERSION}" MONITORING_VERSION)
if (NOT MONITORING_VERSION MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
	message(FATAL_ERROR "VERSION must be MAJOR.MINOR.PATCH, got '${MONITORING_VERSION}'")
endif ()
set(MONITORING_VERSION_MAJOR ${CMAKE_MATCH_1})
set(MONITORING_VERSION_MINOR ${CMAKE_MATCH_2})
set(MONITORING_VERSION_PATCH ${CMAKE_MATCH_3})
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")

# Point at another Authum checkout to build against unreleased Authum changes:
#   cmake -S . -B build -DAUTHUM_DIR=/path/to/raspad-authum
set(AUTHUM_DIR "${CMAKE_SOURCE_DIR}/lib/authum" CACHE PATH "Authum source tree")
if (NOT EXISTS "${AUTHUM_DIR}/client/CMakeLists.txt")
	message(FATAL_ERROR
		"lib/authum missing — run: git submodule update --init --recursive lib/authum")
endif ()

add_subdirectory("${AUTHUM_DIR}/client" authum_client)

add_library(monitoring MODULE
	src/authum/info.c
	src/authum/module.c
	src/authum/bridge.c
	src/monitoring/config.c
	src/monitoring/allowlist.c
	src/monitoring/command.c
	src/monitoring/ratelimit.c
	src/monitoring/reply.c
	src/monitoring/server.c
	src/monitoring/hook.c
)

set_target_properties(monitoring PROPERTIES
	PREFIX ""
	OUTPUT_NAME "monitoring"
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/monitoring"
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/modules/monitoring"
)

target_include_directories(monitoring PRIVATE
	"${CMAKE_SOURCE_DIR}/include"
)

target_compile_definitions(monitoring PRIVATE linux LINUX
	MONITORING_VERSION_MAJOR=${MONITORING_VERSION_MAJOR}
	MONITORING_VERSION_MINOR=${MONITORING_VERSION_MINOR}
	MONITORING_VERSION_PATCH=${MONITORING_VERSION_PATCH}
)

if (NOT MSVC)
	target_compile_options(monitoring PRIVATE
		-m32
		-fvisibility=hidden
		-Wall
		-ffunction-sections
		-fdata-sections
	)
	target_link_options(monitoring PRIVATE
		-m32
		-static-libgcc
		"LINKER:--gc-sections"
	)
endif ()

target_link_libraries(monitoring PRIVATE authum_client pthread dl m)

if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
	if (CMAKE_STRIP)
		add_custom_command(TARGET monitoring POST_BUILD
			COMMAND ${CMAKE_STRIP} --strip-unneeded "$<TARGET_FILE:monitoring>"
			COMMENT "strip monitoring.so")
	endif ()
endif ()
