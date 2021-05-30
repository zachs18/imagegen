cmake_minimum_required(VERSION 3.16)
project(imagegen)

#message("Generated with config types: ${CMAKE_CONFIGURATION_TYPES}")

set(CMAKE_C_FLAGS "-D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -std=c18 -Wall -Wno-multichar")
# _GNU_SOURCE for non-modifying basename
# _POSIX_C_SOURCE >= 200809L for strdup
set(CMAKE_C_FLAGS_DEBUG "-g")
set(CMAKE_C_FLAGS_DEBUGO2 "-g -O2")
set(CMAKE_C_FLAGS_RELEASE "-O2 -DNO_DEBUG")

set(SOURCES
    src/main.c
    src/safelib.c
    src/debug.c
    src/pnmlib.c
    src/randint.c
    src/input.c
    src/output.c
    src/color.c
    src/setup.c
    src/seed_common.c
    src/seed_normal.c
    src/seed_symmetric.c
    src/generate_common.c
    src/generate_normal.c
    src/generate_symmetric.c
    src/progress.c
)

include_directories(include)

#list(APPEND cpu_types generiy sse avx)
#list(APPEND cpu_types_args "-m64" "-msse4.2;-DSSE" "-mavx;-DAVX")

#foreach(cpu_type cpu_type_args IN ZIP_LISTS cpu_types cpu_types_args)
#    add_executable(imagegen_${cpu_type} ${SOURCES})
#    target_link_libraries(imagegen_${cpu_type} pthread SDL2)
#    target_compile_options(imagegen_${cpu_type} PRIVATE "${cpu_type_args}" "-DSDL_PROGRESS" "-DFRAMEBUFFER_PROGRESS")
#endforeach()

list(APPEND GENERIC "generic" "-m64"     "-DGENERIC")
list(APPEND FLOAT   "float"   "-m64"     "-DFLOAT")
list(APPEND SSE     "sse"     "-msse4.2" "-DSSE")
list(APPEND AVX     "avx"     "-mavx"    "-DVAX")

list(APPEND CPU_TYPE "GENERIC" "FLOAT" "SSE" "AVX")
foreach(CPU_TYPE IN LISTS CPU_TYPE)
    list(GET ${CPU_TYPE} 0 CPU_TYPE_STR)
    list(SUBLIST ${CPU_TYPE} 1 -1 CPU_TYPE_ARGS)
    add_executable(imagegen_${CPU_TYPE_STR} ${SOURCES})
    target_link_libraries(imagegen_${CPU_TYPE_STR} pthread SDL2)
    target_compile_options(imagegen_${CPU_TYPE_STR} PRIVATE "${CPU_TYPE_ARGS}" "-DSDL_PROGRESS" "-DFRAMEBUFFER_PROGRESS")
    message("${CPU_TYPE}")
    message("${${CPU_TYPE}}")
endforeach()

# sse
#add_executable(imagegen_sse ${SOURCES})
#target_link_libraries(imagegen_sse pthread SDL2)
#target_compile_options(imagegen_sse PRIVATE "-msse4.2" "-DSSE" "-DSDL_PROGRESS" "-DFRAMEBUFFER_PROGRESS")

# avx
#add_executable(imagegen_avx ${SOURCES})
#target_link_libraries(imagegen_avx pthread SDL2)
#target_compile_options(imagegen_avx PRIVATE "-mavx" "-DAVX" "-DSDL_PROGRESS" "-DFRAMEBUFFER_PROGRESS")
