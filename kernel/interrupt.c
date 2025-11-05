#include "interrupt.h"
#include "global.h"
#include "io.h"
#include "print.h"
#include "stdint.h"

// 主片
#define PIC_M_CTRL 0x20
#define PIC_M_DATA 0x21
// 从片
#define PIC_S_CTRL 0xa0
#define PIC_S_DATA 0xa1

#define IDT_DESC_CNT 0x81

extern uint32_t syscall_handler();

// eflags寄存器的IF位为1
#define EFLAGS_IF 0x00000200
// 获取eflags寄存器的内容
#define GET_EFLAGS(EFLAGS_VAR) asm volatile("pushfl;popl %0" : "=g"(EFLAGS_VAR))

// 中断门描述符
struct gate_desc {
  uint16_t func_offset_low_word;
  uint16_t selector;
  uint8_t dcount;
  uint8_t attribute;
  uint16_t func_offset_high_word;
};

// 对中断门描述符内容进行填充
static void make_idt_desc(struct gate_desc* p_gdesc,
                          uint8_t attr,
                          intr_handler function);

// 中断描述符表
static struct gate_desc idt[IDT_DESC_CNT];

// 中断名称
char* intr_name[IDT_DESC_CNT];

// 中断处理方法
intr_handler idt_table[IDT_DESC_CNT];

// 中断处理历程，在kernel.s定义
extern intr_handler intr_entry_table[IDT_DESC_CNT];

// 初始化可编程中断控制器
static void pic_init() {
  // 初始化主片
  outb(PIC_M_CTRL, 0x11);
  outb(PIC_M_DATA, 0x20);
  outb(PIC_M_DATA, 0x04);
  outb(PIC_M_DATA, 0x01);
  // 初始化从片
  outb(PIC_S_CTRL, 0x11);
  outb(PIC_S_DATA, 0x28);
  outb(PIC_S_DATA, 0x02);
  outb(PIC_S_DATA, 0x01);

  // 测试
  outb(PIC_M_DATA, 0xf8);
  outb(PIC_S_DATA, 0xbf);
  put_str("    pic_init done\n");
}

static void make_idt_desc(struct gate_desc* p_gdesc,
                          uint8_t attr,
                          intr_handler function) {
  p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
  p_gdesc->selector = SELECTOR_K_CODE;
  p_gdesc->dcount = 0;
  p_gdesc->attribute = attr;
  p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

// 中断描述符表初始化
void idt_desc_init() {
  int lastindex = IDT_DESC_CNT - 1;
  for (int i = 0; i < IDT_DESC_CNT; ++i) {
    make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }
  make_idt_desc(&idt[lastindex], IDT_DESC_ATTR_DPL3, syscall_handler);
  put_str("    idt_desc_init done\n");
}

// 临时通用的中断处理方法
static void general_intr_handler(uint8_t vec_nr) {
  if (vec_nr == 0x27 || vec_nr == 0x2f) {
    return;
  }
  set_cursor(0);
  int cursor_pos = 0;
  while (cursor_pos < 320) {
    put_char(' ');
    cursor_pos++;
  }

  set_cursor(0);
  put_str("!!!!!!!    excetion message begin   !!!!!!!\n");
  set_cursor(88);
  put_str(intr_name[vec_nr]);
  if (vec_nr == 14) {
    int page_fault_vaddr = 0;
    asm("movl %%cr2,%0" : "=r"(page_fault_vaddr));

    put_str("\npage fault addr is ");
    put_int(page_fault_vaddr);
  }
  put_str("\n!!!!!!!    excetion message end     !!!!!!!\n");
  while (1)
    ;
}

// 填充中断处理方法和中断名称
static void exception_init() {
  for (int i = 0; i < IDT_DESC_CNT; ++i) {
    idt_table[i] = general_intr_handler;
    intr_name[i] = "unknown";
  }
  intr_name[0] = "#DE Divide Error";
  intr_name[1] = "#DB Debug Exception";
  intr_name[2] = "NMI Interrupt";
  intr_name[3] = "#BP Breakpoint Exception";
  intr_name[4] = "#OF Overflow Exception";
  intr_name[5] = "#BR BOUND Range Exceeded Exception";
  intr_name[6] = "#UD Invalid Opcode Exception";
  intr_name[7] = "#NM Device Not Available Exception";
  intr_name[8] = "#DF Double Fault Exception";
  intr_name[9] = "Coprocessor Segment Overrun";
  intr_name[10] = "#TS Invalid TSS Exception";
  intr_name[11] = "#NP Segment Not Present";
  intr_name[12] = "#SS Stack Fault Exception";
  intr_name[13] = "#GP General Protection Exception";
  intr_name[14] = "#PF Page-Fault Exception";
  // intr_name[15] 第15项是intel保留项，未使用
  intr_name[16] = "#MF x87 FPU Floating-Point Error";
  intr_name[17] = "#AC Alignment Check Exception";
  intr_name[18] = "#MC Machine-Check Exception";
  intr_name[19] = "#XF SIMD Floating-Point Exception";
}

// 初始化中断
void idt_init() {
  put_str("  idt_init start\n");
  idt_desc_init();
  exception_init();
  pic_init();

  uint64_t idt_operand = ((sizeof(idt) - 1) | (uint64_t)(uint32_t)idt << 16);
  asm volatile("lidt %0" ::"m"(idt_operand));
  put_str("  idt_init done\n");
}

// 获取中断开启状态
enum intr_status intr_get_status() {
  uint32_t eflags = 0;
  GET_EFLAGS(eflags);
  return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

// 开启中断
enum intr_status intr_enable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
    return old_status;
  } else {
    old_status = INTR_OFF;
    asm volatile("sti");
    return old_status;
  }
}

// 关闭中断
enum intr_status intr_disable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
    asm volatile("cli" ::: "memory");
    return old_status;
  } else {
    old_status = INTR_OFF;
    return old_status;
  }
}

// 设置中断状态
enum intr_status intr_set_status(enum intr_status status) {
  return status & INTR_ON ? intr_enable() : intr_disable();
}

void register_handler(uint8_t vector_no, intr_handler function) {
  idt_table[vector_no] = function;
}