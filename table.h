/********** table.h **********/
#ifndef TABLE_H_
#define TABLE_H_

/* Identifierの種類 */
typedef enum kindT {
	varId, funcId, procId, parId, constId
} KindT;

/* 変数、パラメタ、関数のアドレスの型 */
typedef struct relAddr {
	int level;
	int addr;
} RelAddr;

void blockBegin(int firstAddr);      /* ブロックの始まり(最初の変数の番地)で呼ばれる */
void blockEnd();                     /* ブロックの終りで呼ばれる */
int bLevel();                        /* 現ブロックのレベルを返す */
int inProcedureBlock();              /* 現プロックが関数内か手続き内か */
int fPars();                         /* 現ブロックの関数のパラメタ数を返す */
void enterT(char *id);               /* 名前表に名前を登録 */
int enterTfunc(char *id, int v);     /* 名前表に関数名と先頭番地を登録 */
int enterTvar(char *id);             /* 名前表に変数名を登録 */
int enterTpar(char *id);             /* 名前表にパラメタ名を登録 */
int enterTconst(char *id, int v);    /* 名前表に定数名とその値を登録 */
void endpar();                       /* パラメタ宣言部の最後で呼ばれる */
void changeV(int ti, int newVal);    /* 名前表[ti]の値(関数の先頭番地)の変更 */

int searchT(char *id, KindT k);      /* 名前idの名前表の位置を返す (未宣言の時エラーとする) */
KindT kindT(int i);                  /* 名前表[i]の種類を返す */

RelAddr relAddr(int ti);             /* 名前表[ti]のアドレスを返す */
int val(int ti);                     /* 名前表[ti]のvalueを返す */
int pars(int ti);                    /* 名前表[ti]の関数のパラメタ数を返す */
int frameL();                        /* そのブロックで実行時に必要とするメモリー容量 */
void forwardAllocatedAddr(int n);    /* 次に割り当てられるアドレスをn番地先に進める */

#endif
