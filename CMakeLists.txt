cmake_minimum_required(VERSION 3.10)
project(xianwei_OS C ASM)
set(CMAKE_C_COMPILER gcc)

set(C_SRC
  ${CMAKE_SOURCE_DIR}/kernel/init.c
  ${CMAKE_SOURCE_DIR}/kernel/interrupt.c
  ${CMAKE_SOURCE_DIR}/kernel/main.c
  ${CMAKE_SOURCE_DIR}/lib/kernel/list.c
  ${CMAKE_SOURCE_DIR}/kernel/debug.c
  ${CMAKE_SOURCE_DIR}/lib/kernel/bitmap.c
  ${CMAKE_SOURCE_DIR}/kernel/memory.c
  ${CMAKE_SOURCE_DIR}/device/timer.c
  ${CMAKE_SOURCE_DIR}/device/console.c
  ${CMAKE_SOURCE_DIR}/device/keyboard.c
  ${CMAKE_SOURCE_DIR}/device/ioqueue.c
  ${CMAKE_SOURCE_DIR}/lib/string.c
  ${CMAKE_SOURCE_DIR}/thread/thread.c
  ${CMAKE_SOURCE_DIR}/thread/sync.c
  ${CMAKE_SOURCE_DIR}/userprog/tss.c
  ${CMAKE_SOURCE_DIR}/userprog/process.c
)

foreach(source_file ${C_SRC})
  get_filename_component(obj_name ${source_file} NAME_WE)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${obj_name}.o
    COMMAND gcc
    -I${CMAKE_SOURCE_DIR}
    -I${CMAKE_SOURCE_DIR}/lib/kernel
    -I${CMAKE_SOURCE_DIR}/lib
    -I${CMAKE_SOURCE_DIR}/kernel
    -I${CMAKE_SOURCE_DIR}/device
    -I${CMAKE_SOURCE_DIR}/thread
    -I${CMAKE_SOURCE_DIR}/userprog
    -m32 -c -fno-builtin -fno-stack-protector
    ${source_file} -o ${CMAKE_BINARY_DIR}/${obj_name}.o
    DEPENDS ${source_file}
    COMMENT "Compiling ${obj_name}.c to ${obj_name}.o"
  )
  add_custom_target(${obj_name}_obj ALL DEPENDS ${CMAKE_BINARY_DIR}/${obj_name}.o)
  list(APPEND O_FILE ${CMAKE_BINARY_DIR}/${obj_name}.o)
endforeach()

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/print.o
  COMMAND nasm -f elf -o ${CMAKE_BINARY_DIR}/print.o ${CMAKE_SOURCE_DIR}/lib/kernel/print.s
  DEPENDS ${CMAKE_SOURCE_DIR}/lib/kernel/print.s
)
add_custom_target(print.s ALL DEPENDS ${CMAKE_BINARY_DIR}/print.o)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/kernel.o
  COMMAND nasm -f elf -o ${CMAKE_BINARY_DIR}/kernel.o ${CMAKE_SOURCE_DIR}/kernel/kernel.s
  DEPENDS ${CMAKE_SOURCE_DIR}/kernel/kernel.s
)
add_custom_target(kernel.s ALL DEPENDS ${CMAKE_BINARY_DIR}/kernel.o)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/switch.o
  COMMAND nasm -f elf -o ${CMAKE_BINARY_DIR}/switch.o ${CMAKE_SOURCE_DIR}/thread/switch.s
  DEPENDS ${CMAKE_SOURCE_DIR}/thread/switch.s
)
add_custom_target(switch.s ALL DEPENDS ${CMAKE_BINARY_DIR}/switch.o)

list(APPEND O_FILE ${CMAKE_BINARY_DIR}/print.o)
list(APPEND O_FILE ${CMAKE_BINARY_DIR}/kernel.o)

add_custom_command(
  OUTPUT kernel.bin
  COMMAND ld -m elf_i386 -Ttext 0xc0001500 -e main -o ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/main.o ${CMAKE_BINARY_DIR}/init.o ${CMAKE_BINARY_DIR}/interrupt.o ${CMAKE_BINARY_DIR}/print.o ${CMAKE_BINARY_DIR}/kernel.o ${CMAKE_BINARY_DIR}/timer.o ${CMAKE_BINARY_DIR}/debug.o ${CMAKE_BINARY_DIR}/memory.o ${CMAKE_BINARY_DIR}/bitmap.o ${CMAKE_BINARY_DIR}/string.o ${CMAKE_BINARY_DIR}/thread.o ${CMAKE_BINARY_DIR}/list.o ${CMAKE_BINARY_DIR}/switch.o ${CMAKE_BINARY_DIR}/sync.o ${CMAKE_BINARY_DIR}/console.o ${CMAKE_BINARY_DIR}/keyboard.o ${CMAKE_BINARY_DIR}/ioqueue.o ${CMAKE_BINARY_DIR}/tss.o ${CMAKE_BINARY_DIR}/process.o
  DEPENDS ${O_FILE} 
  COMMENT "kernel"
)
add_custom_target(kernel ALL DEPENDS ${CMAKE_BINARY_DIR}/kernel.bin)

