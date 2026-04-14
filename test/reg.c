#include <stdio.h>

// int main(){
//   int a = 0x12345678;
//   int b = 0;
//   asm("movl %1, %0" : "=m"(b) : "r"(a));
//   printf("%x\n", b);
// }


// int main(){
//   int a = 18, b = 3, out = 0;
//   asm("divb %[divisor];movb %%al,%[result]" : [result] "=m"(out) : "a"(a), [divisor] "m"(b));
//   printf("%d\n" ,out);
// }

// int main(){
//   int a = 1, b = 2;
//   asm("addl %%ebx, %%eax": "+a"(a) : "b"(b));
//   printf("%d", a);
// }


// int main(){
//   int ret_cnt = 0, test = 0;
//   char* fmt = "hello xianwei\n";
//   asm("pushl %1; call printf; addl $4, %%esp; movl $7, %2"
//       : "=a"(ret_cnt)
//       : "m"(fmt), "r"(test));
//   printf("%d", ret_cnt);
// }



// int main(){
//   int ret_cnt = 0, test = 0;
//   char* fmt = "hello xianwei\n";
//   asm("pushl %1; call printf; addl $4, %%esp; movl $7, %2"
//       : "=&a"(ret_cnt)
//       : "m"(fmt), "r"(test));
//   printf("%d", ret_cnt);
// }


// int main(){
//   int a = 2;
//   int b = 3;
//   asm("addl %%ebx, %%eax" : "+a"(a) : "b"(b));
//   printf("%d %d", a,b);
// }

// int main(){
//   int a = 1, sum = 0;
//   asm("addl %1, %0" : "=a"(sum) : "%I"(2),"0"(a));
//   printf("%d", sum);
// }

// int main(){
//   int a = 0, b = 0;
//   asm("movl $50, %%eax;movl $50, %%ebx" : "=a"(b) : "b"(a));
  
//     printf("%d %d", a, b);
// }


int main(){
  int a = 0x12345678, b = 0;
  asm("movb %b1, %0" : "=m"(b) : "a"(a));
  printf("%x %x", a, b);
}