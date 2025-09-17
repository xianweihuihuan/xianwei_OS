%include "boot.inc"
section MBR vstart=0x7c00
  mov ax,cs
  mov ds,ax
  mov es,ax
  mov ss,ax
  mov fs,ax
  ;栈顶指针
  mov sp,0x7c00
  ;用于访问文本模式显存
  mov ax,0xb800
  mov gs,ax

  ;int x10 发起BIOS中断 
  ;AH = 0x06 功能号：0x06 功能：上卷窗口
  ;AL = 0  上卷的行数，如果为0则全部上卷
  ;BH 上卷的属性
  ;(CL,CH) = 窗口左上角的位置
  ;(DL,DH) = 窗口右下角的位置

  mov ax,0600h ;AX寄存器的低八位为AL，高八位为AH
  mov bx,0700h 
  mov cx,0     ;左上角(0,0)
  mov dx,184fh ;右下角(80,25) 
  ;下标从0开始，所以一行是80个字符，一列是25个字符

  int 10h ;发起BIOS中断，执行上卷窗口

  ;打印黑底量青色闪烁“xianwei MBR”
  mov byte [gs:0x00],'x'
  mov byte [gs:0x01],0x8B

  mov byte [gs:0x02],'i'
  mov byte [gs:0x03],0x8B

  mov byte [gs:0x04],'a'
  mov byte [gs:0x05],0x8B

  mov byte [gs:0x06],'n'
  mov byte [gs:0x07],0x8B

  mov byte [gs:0x08],'w'
  mov byte [gs:0x09],0x8B

  mov byte [gs:0x0a],'e'
  mov byte [gs:0x0b],0x8B

  mov byte [gs:0x0c],'i'
  mov byte [gs:0x0d],0x8B

  mov byte [gs:0x0e],' '
  mov byte [gs:0x0f],0x8B

  mov byte [gs:0x10],'M'
  mov byte [gs:0x11],0x8B

  mov byte [gs:0x12],'B'
  mov byte [gs:0x13],0x8B

  mov byte [gs:0x14],'R'
  mov byte [gs:0x15],0x8B
  
  ;对rd_disk_m_16函数传参
  mov eax,LOADER_START_SECTOR 
  mov bx,LOADER_BASE_ADDR
  mov cx,4
  call rd_disk_m_16

  jmp LOADER_BASE_ADDR + 0x300

rd_disk_m_16:
  
  ;对参数进行备份，因为eax和cx的值在下面会被更改
  mov esi,eax
  mov di,cx

  ;设置读取的扇区数
  mov dx,0x1f2 
  mov al,cl
  out dx,al
  
  ;恢复eax的值
  mov eax,esi

  ;设置LBA的值

  ;写入LBA low
  mov dx,0x1f3
  out dx,al

  ;写入LBA mid
  mov dx,0x1f4
  mov cl,8
  shr eax,cl
  out dx,al

  ;写入LBA high
  shr eax,cl
  mov dx,0x1f5
  out dx,al

  ;写入LBA剩余的位置并设置为LBA模式
  ;将device的7-4位设置为1110
  shr eax,cl
  and al,0x0f
  or al,0xe0
  mov dx,0x1f6
  out dx,al

  ;设置读硬盘命令
  mov dx,0x1f7
  mov al,0x20
  out dx,al

  ;检测硬盘状态
  ;使用同一端口，但读和写的作用不同
.not_ready:
  nop ;空操作，目的为了增加延迟，减少打扰硬盘的工作
  in al,dx
  and al,0x88 ;第三位为1表示硬盘控制器以准备好数据，第七位表示硬盘忙

  cmp al,0x08
  jnz .not_ready ;条件转移，若上一条结果不等于0,则跳转

  ;从硬盘读数据
  mov ax,di ;读扇区的数量
  mov dx,256 ;一次读一个字，两字节，也就是一个扇区需要读512/2次
  mul dx
  mov cx,ax

  mov dx, 0x1f0 ;从data端口读数据
.go_on_read:
  in ax,dx
  mov [bx],ax
  add bx,2
  loop .go_on_read
  ret

  times 510-($-$$) db 0
  
  db 0x55,0xaa
