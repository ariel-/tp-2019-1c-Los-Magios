cmake_minimum_required(VERSION 3.5)
project(Lissandra C)

# necesario ya que CMAKE_C_STANDARD pone el flag de gnu11 en lugar de c11
add_compile_options(-std=c11)

# dame warnings baby
add_compile_options(-Wall -Wextra -Wstrict-prototypes -pedantic)

# para algunas funciones POSIX
add_definitions(-D_DEFAULT_SOURCE)

include_directories(.)
include_directories(Shared)
include_directories(Shared/Config)
include_directories(Shared/Console)
include_directories(Shared/Logging)
include_directories(Shared/Networking)
include_directories(Shared/Protocol)
include_directories(Shared/Utility)

file(GLOB_RECURSE SHARED_SRCS Shared/*.c Shared/*.h)
add_library(shared STATIC ${SHARED_SRCS})

file(GLOB KERNEL_SRCS Kernel/*.c Kernel/*.h)
add_executable(Kernel ${KERNEL_SRCS})

file(GLOB LFS_SRCS LFS/*.c LFS/*.h)
add_executable(LFS ${LFS_SRCS})

file(GLOB MEM_SRCS Memoria/*.c Memoria/*.h)
add_executable(Memoria ${MEM_SRCS})

target_link_libraries(shared pthread readline)
target_link_libraries(Kernel shared)
target_link_libraries(LFS shared)
target_link_libraries(Memoria shared)

add_custom_command(TARGET Kernel
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Kernel/kernel.conf ${CMAKE_BINARY_DIR}/)

add_custom_command(TARGET LFS
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/LFS/lissandra.conf ${CMAKE_BINARY_DIR}/)

add_custom_command(TARGET Memoria
        POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/Memoria/memoria.conf ${CMAKE_BINARY_DIR}/)
