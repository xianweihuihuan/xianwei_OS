section mbr vstart=0x700
  mov ax,cs
  mov ds,ax
  mov es,ax
  mov ss,ax
  mov fs,ax
  mov sp,0x7c00
  mov ax,0xb800
  mov gs,ax

  mov ax,0600h
  mov bx,0700h
  mov cx,0
  mov dx,184fh
  

  int 10h

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

  jmp $

  times 510-($-$$) db 0
  db 0x55,0xaa