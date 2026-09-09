cmake_minimum_required(VERSION 3.25)

set(PROJECT_TARGET KnurLogger)

project(${PROJECT_TARGET} VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)

# gcc 14 carries std::format and its chrono formatters under -std=c++20, verified on the box, so
# iSitePiLogger's FetchContent of fmt is dropped. That matters beyond tidiness: FetchContent needs
# network at configure time, and this box gates Wi-Fi off per session.
add_executable(${PROJECT_TARGET} main.cxx)

include_directories(${GLIB_INCLUDE_DIRS})
add_subdirectory(bluez_inc/binc)

target_include_directories(${PROJECT_TARGET} PRIVATE
	${GLIB_INCLUDE_DIRS}
	${GIO_INCLUDE_DIRS}
)

target_link_directories(${PROJECT_TARGET} PRIVATE
	${GLIB_LIBRARY_DIRS}
	${GIO_LIBRARY_DIRS}
)

target_link_libraries(${PROJECT_TARGET} PRIVATE
	${GLIB_LIBRARIES}
	${GIO_LIBRARIES}
	Binc
	m
)

# The .ini is not generated or copied: it lives in build/ beside the binary, is tracked there as
# the template, and is resolved at runtime from /proc/self/exe. Copying it here would overwrite a
# deployed file carrying real values on every build.
