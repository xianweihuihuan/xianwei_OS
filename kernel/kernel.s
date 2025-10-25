[bits 32]
%define ERROR_CODE nop
%define ZERO push 0

extern idt_table
extern put_str
section .data
intr_str db "interrupt occur",0xa,0
global intr_entry_table
intr_entry_table:
;宏定义，中断处理历程
%macro VECTOR 2
section .text
intr%1entry:
  %2
  push ds
  push es
  push fs
  push gs
  pushad
  ;向中断控制器的主片及从片发送中断结束命令EOI
  mov al, 0x20
  out 0xa0, al
  out 0x20, al
  push %1
  ;调用idt_table中对应的中断处理方法
  call [idt_table + %1 * 4]
  ;中断处理结束
  jmp intr_exit
;由于都是.data，这些section在编译时会被合并到一起，从而形成intr_entry_table,存放着各个中断处理历程的地址
section .data
  dd intr%1entry
%endmacro

section .text
global intr_exit
intr_exit:
  add esp, 4
  popad
  pop gs
  pop fs 
  pop es
  pop ds
  add esp, 4
  iretd

;定义中断处理历程
VECTOR 0x00,ZERO
VECTOR 0x01,ZERO
VECTOR 0x02,ZERO
VECTOR 0x03,ZERO 
VECTOR 0x04,ZERO
VECTOR 0x05,ZERO
VECTOR 0x06,ZERO
VECTOR 0x07,ZERO 
VECTOR 0x08,ERROR_CODE
VECTOR 0x09,ZERO
VECTOR 0x0a,ERROR_CODE
VECTOR 0x0b,ERROR_CODE 
VECTOR 0x0c,ZERO
VECTOR 0x0d,ERROR_CODE
VECTOR 0x0e,ERROR_CODE
VECTOR 0x0f,ZERO 
VECTOR 0x10,ZERO
VECTOR 0x11,ERROR_CODE
VECTOR 0x12,ZERO
VECTOR 0x13,ZERO 
VECTOR 0x14,ZERO
VECTOR 0x15,ZERO
VECTOR 0x16,ZERO
VECTOR 0x17,ZERO 
VECTOR 0x18,ERROR_CODE
VECTOR 0x19,ZERO
VECTOR 0x1a,ERROR_CODE
VECTOR 0x1b,ERROR_CODE 
VECTOR 0x1c,ZERO
VECTOR 0x1d,ERROR_CODE
VECTOR 0x1e,ERROR_CODE
VECTOR 0x1f,ZERO 
VECTOR 0x20,ZERO
VECTOR 0x21,ZERO	;键盘中断对应的入口
VECTOR 0x22,ZERO	;级联用的
VECTOR 0x23,ZERO	;串口2对应的入口
VECTOR 0x24,ZERO	;串口1对应的入口
VECTOR 0x25,ZERO	;并口2对应的入口
VECTOR 0x26,ZERO	;软盘对应的入口
VECTOR 0x27,ZERO	;并口1对应的入口
VECTOR 0x28,ZERO	;实时时钟对应的入口
VECTOR 0x29,ZERO	;重定向
VECTOR 0x2a,ZERO	;保留
VECTOR 0x2b,ZERO	;保留
VECTOR 0x2c,ZERO	;ps/2鼠标
VECTOR 0x2d,ZERO	;fpu浮点单元异常
VECTOR 0x2e,ZERO	;硬盘
VECTOR 0x2f,ZERO	;保留


[bits 32]
extern syscall_table
section .text
global syscall_handler
syscall_handler:
  push 0
  push ds
  push es
  push fs
  push gs
  pushad
  push 0x80
  push edx
  push ecx
  push ebx
  call [syscall_table + eax * 4]
  add esp, 12
  mov [esp + 8* 4], eax
  jmp intr_exit
