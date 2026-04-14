cmake_minimum_required(VERSION 3.10)
project(xianwei_OS C ASM)
set(CMAKE_C_COMPILER gcc)

set(C_SRC
  ${CMAKE_SOURCE_DIR}/kernel/init.c
  ${CMAKE_SOURCE_DIR}/kernel/interrupt.c
  ${CMAKE_SOURCE_DIR}/kernel/main.c
  ${CMAKE_SOURCE_DIR}/lib/kernel/list.c
  ${CMAKE_SOURCE_DIR}/lib/kernel/stdio-kernel.c
  ${CMAKE_SOURCE_DIR}/kernel/debug.c
  ${CMAKE_SOURCE_DIR}/lib/kernel/bitmap.c
  ${CMAKE_SOURCE_DIR}/kernel/memory.c
  ${CMAKE_SOURCE_DIR}/device/timer.c
  ${CMAKE_SOURCE_DIR}/device/console.c
  ${CMAKE_SOURCE_DIR}/device/ide.c
  ${CMAKE_SOURCE_DIR}/device/keyboard.c
  ${CMAKE_SOURCE_DIR}/device/ioqueue.c
  ${CMAKE_SOURCE_DIR}/lib/string.c
  ${CMAKE_SOURCE_DIR}/thread/thread.c
  ${CMAKE_SOURCE_DIR}/thread/sync.c
  ${CMAKE_SOURCE_DIR}/userprog/tss.c
  ${CMAKE_SOURCE_DIR}/userprog/exec.c
  ${CMAKE_SOURCE_DIR}/userprog/process.c
  ${CMAKE_SOURCE_DIR}/userprog/fork.c
  ${CMAKE_SOURCE_DIR}/userprog/wait_exit.c
  ${CMAKE_SOURCE_DIR}/userprog/syscall-init.c
  ${CMAKE_SOURCE_DIR}/lib/user/syscall.c
  ${CMAKE_SOURCE_DIR}/lib/user/assert.c
  ${CMAKE_SOURCE_DIR}/lib/stdio.c
  ${CMAKE_SOURCE_DIR}/fs/fs.c
  ${CMAKE_SOURCE_DIR}/fs/dir.c
  ${CMAKE_SOURCE_DIR}/fs/inode.c
  ${CMAKE_SOURCE_DIR}/fs/file.c
  ${CMAKE_SOURCE_DIR}/shell/shell.c
  ${CMAKE_SOURCE_DIR}/shell/buildin_cmd.c
  ${CMAKE_SOURCE_DIR}/shell/pipe.c
)

foreach(source_file ${C_SRC})
  get_filename_component(obj_name ${source_file} NAME_WE)
  add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/${obj_name}.o
    COMMAND gcc
    -I${CMAKE_SOURCE_DIR}
    -I${CMAKE_SOURCE_DIR}/lib/user
    -I${CMAKE_SOURCE_DIR}/lib/kernel
    -I${CMAKE_SOURCE_DIR}/lib
    -I${CMAKE_SOURCE_DIR}/kernel
    -I${CMAKE_SOURCE_DIR}/device
    -I${CMAKE_SOURCE_DIR}/thread
    -I${CMAKE_SOURCE_DIR}/userprog
    -I${CMAKE_SOURCE_DIR}/fs
    -I${CMAKE_SOURCE_DIR}/shell
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

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/MBR.bin
  COMMAND nasm -I${CMAKE_SOURCE_DIR}/boot/include/ -f bin -o ${CMAKE_BINARY_DIR}/MBR.bin ${CMAKE_SOURCE_DIR}/boot/MBR.s
  DEPENDS ${CMAKE_SOURCE_DIR}/boot/MBR.s ${CMAKE_SOURCE_DIR}/boot/include/boot.inc
)
add_custom_target(mbr_bin ALL DEPENDS ${CMAKE_BINARY_DIR}/MBR.bin)

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/loader.bin
  COMMAND nasm -I${CMAKE_SOURCE_DIR}/boot/include/ -f bin -o ${CMAKE_BINARY_DIR}/loader.bin ${CMAKE_SOURCE_DIR}/boot/loader.s
  DEPENDS ${CMAKE_SOURCE_DIR}/boot/loader.s ${CMAKE_SOURCE_DIR}/boot/include/boot.inc
)
add_custom_target(loader_bin ALL DEPENDS ${CMAKE_BINARY_DIR}/loader.bin)

list(APPEND O_FILE ${CMAKE_BINARY_DIR}/print.o)
list(APPEND O_FILE ${CMAKE_BINARY_DIR}/kernel.o)
list(APPEND O_FILE ${CMAKE_BINARY_DIR}/switch.o)

add_custom_command(
  OUTPUT kernel.bin
  COMMAND ld -m elf_i386 -Ttext 0xc0001500 -e main -o ${CMAKE_BINARY_DIR}/kernel.bin ${CMAKE_BINARY_DIR}/main.o ${CMAKE_BINARY_DIR}/init.o ${CMAKE_BINARY_DIR}/interrupt.o ${CMAKE_BINARY_DIR}/print.o ${CMAKE_BINARY_DIR}/kernel.o ${CMAKE_BINARY_DIR}/timer.o ${CMAKE_BINARY_DIR}/debug.o ${CMAKE_BINARY_DIR}/memory.o ${CMAKE_BINARY_DIR}/bitmap.o ${CMAKE_BINARY_DIR}/string.o ${CMAKE_BINARY_DIR}/thread.o ${CMAKE_BINARY_DIR}/list.o ${CMAKE_BINARY_DIR}/switch.o ${CMAKE_BINARY_DIR}/sync.o ${CMAKE_BINARY_DIR}/console.o ${CMAKE_BINARY_DIR}/keyboard.o ${CMAKE_BINARY_DIR}/ioqueue.o ${CMAKE_BINARY_DIR}/tss.o ${CMAKE_BINARY_DIR}/process.o ${CMAKE_BINARY_DIR}/syscall-init.o ${CMAKE_BINARY_DIR}/syscall.o
  ${CMAKE_BINARY_DIR}/stdio.o ${CMAKE_BINARY_DIR}/stdio-kernel.o ${CMAKE_BINARY_DIR}/ide.o ${CMAKE_BINARY_DIR}/fs.o ${CMAKE_BINARY_DIR}/dir.o ${CMAKE_BINARY_DIR}/inode.o ${CMAKE_BINARY_DIR}/file.o ${CMAKE_BINARY_DIR}/fork.o ${CMAKE_BINARY_DIR}/shell.o ${CMAKE_BINARY_DIR}/buildin_cmd.o ${CMAKE_BINARY_DIR}/exec.o ${CMAKE_BINARY_DIR}/assert.o ${CMAKE_BINARY_DIR}/wait_exit.o ${CMAKE_BINARY_DIR}/pipe.o
  DEPENDS ${O_FILE}
  COMMENT "kernel"
)
add_custom_target(kernel ALL DEPENDS ${CMAKE_BINARY_DIR}/kernel.bin)

add_custom_target(write_kernel
  COMMAND dd if=${CMAKE_BINARY_DIR}/kernel.bin of=${CMAKE_SOURCE_DIR}/hd60M.img bs=512 count=200 seek=9 conv=notrunc
  DEPENDS kernel
  COMMENT "Writing kernel.bin to hd60M.img"
)

add_custom_target(write_mbr
  COMMAND dd if=${CMAKE_BINARY_DIR}/MBR.bin of=${CMAKE_SOURCE_DIR}/hd60M.img bs=512 count=1 conv=notrunc
  DEPENDS mbr_bin
  COMMENT "Writing MBR.bin to hd60M.img"
)

add_custom_target(write_loader
  COMMAND dd if=${CMAKE_BINARY_DIR}/loader.bin of=${CMAKE_SOURCE_DIR}/hd60M.img bs=512 seek=2 conv=notrunc
  DEPENDS loader_bin
  COMMENT "Writing loader.bin to hd60M.img"
)

add_custom_target(write_image ALL
  DEPENDS write_mbr write_loader write_kernel
)

