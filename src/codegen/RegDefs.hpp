#ifndef HANDLE_REG_DEF
#define HANDLE_REG_DEF(name, index)
#endif

/* ---- constant零寄存器 ---- */
HANDLE_REG_DEF(ZERO, 0);  // $zero

/* ---- 汇编结果寄存器 ---- */
HANDLE_REG_DEF(V0, 2);  // $v0
HANDLE_REG_DEF(V1, 3);  // $v1

/* ---- 参数寄存器 ---- */
HANDLE_REG_DEF(A0, 4);  // $a0
HANDLE_REG_DEF(A1, 5);  // $a1
HANDLE_REG_DEF(A2, 6);  // $a2
HANDLE_REG_DEF(A3, 7);  // $a3

/* ---- 临时（调用者保存）寄存器 ---- */
HANDLE_REG_DEF(T0, 8);   // $t0
HANDLE_REG_DEF(T1, 9);   // $t1
HANDLE_REG_DEF(T2, 10);  // $t2
HANDLE_REG_DEF(T3, 11);  // $t3
HANDLE_REG_DEF(T4, 12);  // $t4
HANDLE_REG_DEF(T5, 13);  // $t5
HANDLE_REG_DEF(T6, 14);  // $t6
HANDLE_REG_DEF(T7, 15);  // $t7

/* ---- 保存（被调用者保存）寄存器 ---- */
HANDLE_REG_DEF(S0, 16);  // $s0
HANDLE_REG_DEF(S1, 17);  // $s1
HANDLE_REG_DEF(S2, 18);  // $s2
HANDLE_REG_DEF(S3, 19);  // $s3
HANDLE_REG_DEF(S4, 20);  // $s4
HANDLE_REG_DEF(S5, 21);  // $s5
HANDLE_REG_DEF(S6, 22);  // $s6
HANDLE_REG_DEF(S7, 23);  // $s7

/* ---- 更多临时寄存器 ---- */
HANDLE_REG_DEF(T8, 24);  // $t8
HANDLE_REG_DEF(T9, 25);  // $t9

/* ---- 内核/临时寄存器 ---- */
HANDLE_REG_DEF(K0, 26);  // $k0
HANDLE_REG_DEF(K1, 27);  // $k1

/* ---- 全局指针 / 栈指针 / 帧指针 / 返回地址 ---- */
HANDLE_REG_DEF(GP, 28);  // $gp
HANDLE_REG_DEF(SP, 29);  // $sp
HANDLE_REG_DEF(FP, 30);  // $fp
HANDLE_REG_DEF(RA, 31);  // $ra

#undef HANDLE_REG_DEF
