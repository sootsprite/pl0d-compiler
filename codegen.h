/********** codegen.h **********/
#ifndef CODEGEN_H_
#define CODEGEN_H_

/* 命令語のコード */
typedef enum codes {
	lit, opr, lod, sto, cal, ret, ict, jmp, jpc,
	loda, stoa, retp
} OpCode;

/* 演算命令のコード */
typedef enum ops {
	neg, add, sub, mul, div, odd, eq, ls, gr,
	neq, lseq, greq, wrt, wrl
} Operator;

int genCodeV(OpCode op, int v);     /* 命令語の生成、アドレス部にv */
int genCodeT(OpCode op, int ti);    /* 命令語の生成、アドレスは名前表から */
int genCodeO(Operator p);           /* 命令語の生成、アドレス部に演算命令 */
int genCodeR(int forProc);          /* ret命令語の生成 */
void backPatch(int i);              /* 命令語のバックパッチ(次の番地を) */

int nextCode();                     /* 次の命令語のアドレスを返す */
void listCode();                    /* 目的コード(命令語)のリスティング */
void execute();                     /* 目的コード(命令語)の実行 */

#endif
