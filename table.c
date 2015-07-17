/********** table.c **********/
#include "table.h"
#include "getSource.h"

#define MAXTABLE 100    /* 名前表の最大長さ */
#define MAXNAME  31     /* 名前の最大長さ */
#define MAXLEVEL 5      /* ブロックの最大深さ */

extern char *strcpy(char *s1, const char *s2);

/* 名前表のエントリーの型 */
typedef struct tableE {
	KindT kind;               /* 名前の種類 */
	char name[MAXNAME];       /* 名前のつづり */
	union {
		int value;            /* 定数の場合：値 */
		struct {
			RelAddr raddr;    /* 関数の場合：先頭アドレス */
			int pars;         /* 関数の場合：パラメタ数 */
		} f;
		RelAddr raddr;        /* 変数、パラメタの場合：アドレス */
	} u;
} TabelE;

static TabelE nameTable[MAXTABLE];    /* 名前表 */
static int tIndex = 0;                /* 名前表のインデックス */
static int level = -1;                /* 現在のブロックレベル */
static int index[MAXLEVEL];           /* index[i]にはブロックレベルiの最後のインデックス */
static int addr[MAXLEVEL];            /* addr[i]にはブロックレベルiの最後の変数の番地 */
static int localAddr;                 /* 現在のブロックの最後の変数の番地 */
static int tfIndex;

/* 名前の種類の出力用関数 */
static char* kindName(KindT k)
{
	switch (k) {
	case varId: return "var";
	case parId: return "par";
	case funcId: return "func";
	case constId: return "const";
	}
}

/* ブロックの始まり(最初の変数の番地)で呼ばれる */
void blockBegin(int firstAddr)
{
	if (level == -1) {        /* 主ブロックの時、初期設定 */
		localAddr = firstAddr;
		tIndex = 0;
		level++;
		return;
	}
	if (level == MAXLEVEL-1)
		errorF("too many nested blocks");
	index[level] = tIndex;    /* 今までのブロックの情報を格納 */
	addr[level] = localAddr;
	localAddr = firstAddr;    /* 新しいブロックの最初の変数の番地 */
	level++;                  /* 新しいブロックのレベル */
	return;
}

/* ブロックの終りで呼ばれる */
void blockEnd()
{
	level--;
	tIndex = index[level];    /* 一つ外側のブロックの情報を回復 */
	localAddr = addr[level];
}

/* 現ブロックのレベルを返す */
int bLevel()
{
	return level;
}

/* 現プロックが関数内か手続き内か */
int inProcedureBlock()
{
	return nameTable[index[level - 1]].kind == procId;
}

/* 現ブロックの関数のパラメタ数を返す */
int fPars()
{
	return nameTable[index[level - 1]].u.f.pars;
}

/* 名前表に関数や手続きを登録 */
static int enterTsequence(char *id, int v, KindT kind)
{
	enterT(id);
	nameTable[tIndex].kind = kind;
	nameTable[tIndex].u.f.raddr.level = level;
	nameTable[tIndex].u.f.raddr.addr = v;    /* 関数の先頭番地 */
	nameTable[tIndex].u.f.pars = 0;          /* パラメタ数の初期値 */
	tfIndex = tIndex;
	return tIndex;
}

/* 名前表に名前を登録 */
void enterT(char *id)
{
	if (tIndex++ < MAXTABLE)
		strcpy(nameTable[tIndex].name, id);
	else
		errorF("too many names");
}

/* 名前表に関数名と先頭番地を登録 */
int enterTfunc(char *id, int v)
{
	return enterTsequence(id, v, funcId);
}

/* 名前表に手続き名と先頭番地を登録 */
int enterTproc(char *id, int v)
{
	return enterTsequence(id, v, procId);
}

/* 名前表にパラメタ名を登録 */
int enterTpar(char *id)
{
	enterT(id);
	nameTable[tIndex].kind = parId;
	nameTable[tIndex].u.raddr.level = level;
	nameTable[tfIndex].u.f.pars++;    /* 関数のパラメタ数のカウント */
	return tIndex;
}

/* 名前表に変数名を登録 */
int enterTvar(char *id)
{
	enterT(id);
	nameTable[tIndex].kind = varId;
	nameTable[tIndex].u.raddr.level = level;
	nameTable[tIndex].u.raddr.addr = localAddr++;
	return tIndex;
}

/* 名前表に定数名とその値を登録 */
int enterTconst(char *id, int v)
{
	enterT(id);
	nameTable[tIndex].kind = constId;
	nameTable[tIndex].u.value = v;
	return tIndex;
}

/* パラメタ宣言部の最後で呼ばれる */
void endpar()
{
	int i;
	int pars = nameTable[tfIndex].u.f.pars;
	if (pars == 0)  return;
	for (i=1; i<=pars; i++)    /* 各パラメタの番地を求める */
		nameTable[tfIndex+i].u.raddr.addr = i-1-pars;
}

/* 名前表[ti]の値(関数の先頭番地)の変更 */
void changeV(int ti, int newVal)
{
	nameTable[ti].u.f.raddr.addr = newVal;
}

/* 名前idの名前表の位置を返す (未宣言の時エラーとする) */
int searchT(char *id, KindT k)
{
	int i;
	i = tIndex;
	strcpy(nameTable[0].name, id);    /* 番兵をたてる */
	while( strcmp(id, nameTable[i].name) )
		i--;
	if ( i )                          /* 名前があった */
		return i;
	else {                            /* 名前がなかった */
		errorType("undef");
		if (k == varId)
			return enterTvar(id);     /* 変数の時は仮登録 */
		return 0;
	}
}

/* 名前表[i]の種類を返す */
KindT kindT(int i)
{
	return nameTable[i].kind;
}

/* 名前表[ti]のアドレスを返す */
RelAddr relAddr(int ti)
{
	return nameTable[ti].u.raddr;
}

/* 名前表[ti]のvalueを返す */
int val(int ti)
{
	return nameTable[ti].u.value;
}

/* 名前表[ti]の関数のパラメタ数を返す */
int pars(int ti)
{
	return nameTable[ti].u.f.pars;
}

/* そのブロックで実行時に必要とするメモリー容量 */
int frameL()
{
	return localAddr;
}

/* 次に割り当てられるアドレスをn番地先に進める */
void forwardAllocatedAddr(int n)
{
	localAddr += n;
}
