/********** codegen.c **********/
#include <stdio.h>
#include "codegen.h"
#include "table.h"
#include "getSource.h"

#define MAXCODE 200    /* 目的コードの最大長さ */
#define MAXMEM 2000    /* 実行時スタックの最大長さ */
#define MAXREG 20      /* 演算レジスタスタックの最大長さ */
#define MAXLEVEL 5     /* ブロックの最大深さ */

/* 命令語の型 */
typedef struct inst {
	OpCode  opCode;
	union {
		RelAddr addr;
		int value;
		Operator optr;
	} u;
} Inst;

static char ref[MAXCODE];        /* ref[i]が0ならcode[i]は参照されている. */
static Inst code[MAXCODE];       /* 目的コードが入る */
static int cIndex = -1;          /* 最後に生成した命令語のインデックス */
static void checkMax();          /* 目的コードのインデックスの増加とチェック */
static void printCode(int i);    /* 命令語の印字 */
static void updateRef(int i);

/* 次の命令語のアドレスを返す */
int nextCode()
{
	return cIndex + 1;
}

/* 命令語の生成、アドレス部にv */
int genCodeV(OpCode op, int v)
{
	checkMax();
	code[cIndex].opCode = op;
	code[cIndex].u.value = v;
	return cIndex;
}

/* 命令語の生成、アドレスは名前表から */
int genCodeT(OpCode op, int ti)
{
	checkMax();
	code[cIndex].opCode = op;
	code[cIndex].u.addr = relAddr(ti);
	return cIndex;
}

/* 命令語の生成、アドレス部に演算命令 */
int genCodeO(Operator p)
{
	checkMax();
	code[cIndex].opCode = opr;
	code[cIndex].u.optr = p;
	return cIndex;
}

/* ret命令語の生成 */
int genCodeR(int forProc)
{
	/* 直前がretなら生成せず */
	if (code[cIndex].opCode == ret)
		return cIndex;
	checkMax();
	code[cIndex].opCode = forProc ? retp : ret;
	code[cIndex].u.addr.level = bLevel();
	code[cIndex].u.addr.addr = fPars();    /* パラメタ数(実行スタックの解放用)*/
	return cIndex;
}

/* 目的コードのインデックスの増加とチェック */
void checkMax()
{
	if (++cIndex < MAXCODE)
		return;
	errorF("too many code");
}

/* 命令語のバックパッチ(次の番地を) */
void backPatch(int i)
{
	code[i].u.value = cIndex + 1;
}

/* 命令語のリスティング */
void listCode()
{
	int i;
	printf("\n; code\n");

	for(i = 0; i <= cIndex; i++)
		ref[i] = 0;
	for(i = 0; i <= cIndex; i++)
		updateRef(i);
	for(i = 0; i <= cIndex; i++) {
		if (ref[i])
			printf("L%3.3d: ", i);
		else
			printf("      ");
		printCode(i);
	}
}

/* 配列refの更新 */
void updateRef(int i)
{
	int flag;
	switch(code[i].opCode) {
	case lit: flag = 1; break;
	case opr: flag = 3; break;
	case lod: flag = 2; break;
	case sto: flag = 2; break;
	case cal: flag = 5; break;
	case ret: flag = 2; break;
	case ict: flag = 1; break;
	case jmp: flag = 4; break;
	case jpc: flag = 4; break;
	case loda: flag = 2; break;
	case stoa: flag = 2; break;
	case retp: flag = 2; break;
	}
	switch(flag) {
	case 1:
		return;
	case 2:
		ref[code[i].u.addr.addr] = 1;
		return;
	case 3:
		switch(code[i].u.optr) {
		case neg: return;
		case add: return;
		case sub: return;
		case mul: return;
		case div: return;
		case odd: return;
		case eq: return;
		case ls: return;
		case gr: return;
		case neq: return;
		case lseq: return;
		case greq: return;
		case wrt: return;
		case wrl: return;
		}
	case 4:
		ref[code[i].u.value] = 1;
		return;
	case 5:
		ref[code[i].u.addr.addr] = 1;
		return;
	}
}

/* 命令語の印字 */
void printCode(int i)
{
	int flag;
	switch(code[i].opCode) {
	case lit: printf("lit"); flag = 1; break;
	case opr: printf("opr"); flag = 3; break;
	case lod: printf("lod"); flag = 2; break;
	case sto: printf("sto"); flag = 2; break;
	case cal: printf("cal"); flag = 5; break;
	case ret: printf("ret"); flag = 2; break;
	case ict: printf("ict"); flag = 1; break;
	case jmp: printf("jmp"); flag = 4; break;
	case jpc: printf("jpc"); flag = 4; break;
	case loda: printf("loda"); flag = 2; break;
	case stoa: printf("stoa"); flag = 2; break;
	case retp: printf("retp"); flag = 2; break;
	}
	switch(flag) {
	case 1:
		printf(",%d\n", code[i].u.value);
		return;
	case 2:
		printf(",%d", code[i].u.addr.level);
		printf(",%d\n", code[i].u.addr.addr);
		return;
	case 3:
		switch(code[i].u.optr) {
		case neg: printf(",neg\n"); return;
		case add: printf(",add\n"); return;
		case sub: printf(",sub\n"); return;
		case mul: printf(",mul\n"); return;
		case div: printf(",div\n"); return;
		case odd: printf(",odd\n"); return;
		case eq: printf(",eq\n"); return;
		case ls: printf(",ls\n"); return;
		case gr: printf(",gr\n"); return;
		case neq: printf(",neq\n"); return;
		case lseq: printf(",lseq\n"); return;
		case greq: printf(",greq\n"); return;
		case wrt: printf(",wrt\n"); return;
		case wrl: printf(",wrl\n"); return;
		}
	case 4:
		printf(",L%3.3d\n", code[i].u.value);
		return;
	case 5:
		printf(",%d", code[i].u.addr.level);
		printf(",L%3.3d\n", code[i].u.addr.addr);
		return;
	}
}

/* 目的コード(命令語)の実行 */
void execute()
{
	int stack[MAXMEM];        /* 実行時スタック */
	int display[MAXLEVEL];    /* 現在見える各ブロックの先頭番地のディスプレイ */
	int pc, top, lev, temp;
	Inst i;                   /* 実行する命令語 */

	printf("; start execution\n");
	top = 0;  pc = 0;               /* top:次にスタックに入れる場所、pc:命令語のカウンタ */
	stack[0] = 0;  stack[1] = 0;    /* stack[top]はcalleeで壊すディスプレイの退避場所 stack[top+1]はcallerへの戻り番地 */
	display[0] = 0;                 /* 主ブロックの先頭番地は 0 */

	do {
		i = code[pc++];    /* これから実行する命令語 */
		switch(i.opCode) {
		case lit:
			stack[top++] = i.u.value;
			break;
		case lod:
			stack[top++] = stack[display[i.u.addr.level] + i.u.addr.addr];
			break;
		case sto:
			stack[display[i.u.addr.level] + i.u.addr.addr] = stack[--top];
			break;
		case cal:
			lev = i.u.addr.level + 1;                     /* i.u.addr.levelはcalleeの名前のレベル calleeのブロックのレベルlevはそれに+1したもの */
			stack[top] = display[lev];                    /* display[lev]の退避 */
			stack[top + 1] = pc; display[lev] = top;        /* 現在のtopがcalleeのブロックの先頭番地 */
			pc = i.u.addr.addr;
			break;
		case ret:
			temp = stack[--top];                          /* スタックのトップにあるものが返す値 */
			top = display[i.u.addr.level];                /* topを呼ばれたときの値に戻す */
			display[i.u.addr.level] = stack[top];         /* 壊したディスプレイの回復 */
			pc = stack[top + 1];
			top -= i.u.addr.addr;                         /* 実引数の分だけトップを戻す */
			stack[top++] = temp;                          /* 返す値をスタックのトップへ */
			break;
		case ict:
			top += i.u.value;
			if (top >= MAXMEM - MAXREG)
				errorF("stack overflow");
			break;
		case jmp:
			pc = i.u.value; break;
		case jpc:
			if (stack[--top] == 0)
				pc = i.u.value;
			break;
		case opr:
			switch(i.u.optr) {
			case neg: stack[top - 1] = -stack[top - 1]; continue;
			case add: --top;  stack[top - 1] += stack[top]; continue;
			case sub: --top; stack[top - 1] -= stack[top]; continue;
			case mul: --top;  stack[top - 1] *= stack[top];  continue;
			case div: --top;  stack[top - 1] /= stack[top]; continue;
			case odd: stack[top - 1] = stack[top - 1] & 1; continue;
			case eq: --top;  stack[top - 1] = (stack[top - 1] == stack[top]); continue;
			case ls: --top;  stack[top - 1] = (stack[top - 1] < stack[top]); continue;
			case gr: --top;  stack[top - 1] = (stack[top - 1] > stack[top]); continue;
			case neq: --top;  stack[top - 1] = (stack[top - 1] != stack[top]); continue;
			case lseq: --top;  stack[top - 1] = (stack[top - 1] <= stack[top]); continue;
			case greq: --top;  stack[top - 1] = (stack[top - 1] >= stack[top]); continue;
			case wrt: printf("%d ", stack[--top]); continue;
			case wrl: printf("\n"); continue;
			}
		case loda:
			stack[top - 1] = stack[display[i.u.addr.level] + i.u.addr.addr + stack[top - 1]];
			break;
		case stoa:
			--top;
			stack[display[i.u.addr.level] + i.u.addr.addr + stack[top - 1]] = stack[top--];
			break;
		case retp:
			top = display[i.u.addr.level];                /* topを呼ばれたときの値に戻す */
			display[i.u.addr.level] = stack[top];         /* 壊したディスプレイの回復 */
			pc = stack[top + 1];
			top -= i.u.addr.addr;                         /* 実引数の分だけトップを戻す */
			break;
		}
	} while (pc != 0);
}
