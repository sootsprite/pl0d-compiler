/********** compile.c **********/
#include "getSource.h"
#include "table.h"
#include "codegen.h"

#define MINERROR 3     /* エラーがこれ以下なら実行 */
#define FIRSTADDR 2    /* 各ブロックの最初の変数のアドレス */

static Token token;    /* 次のトークンを入れておく */

static void block(int pIndex);       /* ブロックのコンパイル (pIndexはこのブロックの関数名のインデックス) */
static void declaration();
static void constDecl();             /* 定数宣言のコンパイル */
static void varDecl();               /* 変数宣言のコンパイル */
static void funcDecl();              /* 関数宣言のコンパイル */
static void procDecl();              /* 手続き宣言のコンパイル */
static void statement();             /* 文のコンパイル */
static void expression();            /* 式のコンパイル */
static void term();                  /* 式の項のコンパイル */
static void factor();                /* 式の因子のコンパイル */
static void condition();             /* 条件式のコンパイル */
static int isStBeginKey(Token t);    /* トークンtは文の先頭のキーか? */

int compile()
{
	int i;
	printf("; start compilation\n");
	initSource();                         /* getSourceの初期設定 */
	token = nextToken();                  /* 最初のトークン */
	blockBegin(FIRSTADDR);                /* これ以後の宣言は新しいブロックのもの */
	block(0);                             /* 0 はダミー(主ブロックの関数名はない) */
	finalSource();
	i = errorN();                         /* エラーメッセージの個数 */
	if (i != 0)
		printf("; %d errors\n", i);
	// listCode();                        /* 目的コードのリスト(必要なら) */
	return i < MINERROR;                  /* エラーメッセージの個数が少ないかどうかの判定 */
}

/* pIndex はこのブロックの関数名のインデックス */
void block(int pIndex)
{
	int backP;
	backP = genCodeV(jmp, 0);    /* 内部関数を飛び越す命令、後でバックパッチ */

	/* 宣言部のコンパイルを繰り返す */
	while (1) {
		switch (token.kind) {
		case Const:    /* 定数宣言部のコンパイル */
			token = nextToken();
			constDecl();
			continue;
		case Var:    /* 変数宣言部のコンパイル */
			token = nextToken();
			varDecl();
			continue;
		case Func:    /* 関数宣言部のコンパイル */
			token = nextToken();
			funcDecl();
			continue;
		case Proc:
			token = nextToken();
			procDecl();
			continue;
		default:    /* それ以外なら宣言部は終わり */
			break;
		}
		break;
	}

	backPatch(backP);               /* 内部関数を飛び越す命令にパッチ */
	changeV(pIndex, nextCode());    /* この関数の開始番地を修正 */
	genCodeV(ict, frameL());        /* このブロックの実行時の必要記憶域をとる命令 */
	statement();                    /* このブロックの主文 */
	genCodeR(inProcedureBlock());   /* ret命令 */
	blockEnd();                     /* ブロックが終ったことをtableに連絡 */
}

void declaration(KindT kind)
{
	Token temp;
	int fIndex;

	switch (kind) {
	case constId:
		while(1) {
			if (token.kind == Id) {
				setIdKind(constId);                           /* 印字のための情報のセット */
				temp = token;                                 /* 名前を入れておく */
				token = checkGet(nextToken(), Equal);         /* 名前の次は"="のはず */
				if (token.kind == Num)
					enterTconst(temp.u.id, token.u.value);    /* 定数名と値をテーブルに */
				else
					errorType("number");
				token = nextToken();
			}
			else
				errorMissingId();
			if (token.kind != Comma) {                        /* 次がコンマなら定数宣言が続く */
				if (token.kind == Id) {                       /* 次が名前ならコンマを忘れたことにする */
					errorInsert(Comma);
					continue;
				}
				else
					break;
			}
			token = nextToken();
		}
		token = checkGet(token, Semicolon);                   /* 最後は";"のはず */
		break;
	case varId:
		while(1) {
			if (token.kind == Id) {
				setIdKind(varId);                             /* 印字のための情報のセット */
				enterTvar(token.u.id);                        /* 変数名をテーブルに、番地はtableが決める */
				token = nextToken();

				if (token.kind == Lbracket) {                 /* 配列だったら */
					token = nextToken();
					if (token.kind == Num)
						forwardAllocatedAddr(token.u.value - 1);
					else
						errorType("number");
					token = checkGet(nextToken(), Rbracket);
				}
			}
			else
				errorMissingId();
			if (token.kind != Comma) {                        /* 次がコンマなら変数宣言が続く */
				if (token.kind == Id) {                       /* 次が名前ならコンマを忘れたことにする */
					errorInsert(Comma);
					continue;
				}
				else
					break;
			}
			token = nextToken();
		}
		token = checkGet(token, Semicolon);                   /* 最後は";"のはず */
		break;
	default:
		if (token.kind == Id) {
			setIdKind(kind);                                  /* 印字のための情報のセット */
			fIndex = kind == funcId ?
				enterTfunc(token.u.id, nextCode()) : enterTproc(token.u.id, nextCode());
			token = checkGet(nextToken(), Lparen);
			blockBegin(FIRSTADDR);                            /* パラメタ名のレベルは関数のブロックと同じ */
			while(1) {
				if (token.kind == Id) {                       /* パラメタ名がある場合 */
					setIdKind(parId);                         /* 印字のための情報のセット */
					enterTpar(token.u.id);                    /* パラメタ名をテーブルに登録 */
					token = nextToken();
				}
				else
					break;
				if (token.kind != Comma) {                    /* 次がコンマならパラメタ名が続く */
					if (token.kind == Id) {                   /* 次が名前ならコンマを忘れたことに */
						errorInsert(Comma);
						continue;
					}
					else
						break;
				}
				token = nextToken();
			}
			token = checkGet(token, Rparen);                  /* 最後は")"のはず */
			endpar();                                         /* パラメタ部が終わったことをテーブルに連絡 */
			if (token.kind == Semicolon) {
				errorDelete();
				token = nextToken();
			}
			block(fIndex);                                    /* ブロックのコンパイル、その関数名のインデックスを渡す */
			token = checkGet(token, Semicolon);               /* 最後は";"のはず */
		}
		else
			errorMissingId();                                 /* 関数名がない */
	}
}

/* 定数宣言のコンパイル */
void constDecl()
{
	declaration(constId);
}

/* 変数宣言のコンパイル */
void varDecl()
{
	declaration(varId);
}

/* 関数宣言のコンパイル */
void funcDecl()
{
	declaration(funcId);
}

/* 手続き宣言のコンパイル */
void procDecl()
{
	declaration(procId);
}

/* 文のコンパイル */
void statement()
{
	int tIndex;
	KindT k;
	int backP, backP2;    /* バックパッチ用 */
	int backP3, backP4;

	while(1) {
		OpCode op;

		switch (token.kind) {
		case Id:                                          /* 代入文のコンパイル */
			op = sto;

			tIndex = searchT(token.u.id, varId);          /* 左辺の変数のインデックス */
			setIdKind(k = kindT(tIndex));                 /* 印字のための情報のセット */
			if (k != varId && k != parId)                 /* 変数名かパラメタ名のはず */
				errorType("var/par");

			token = nextToken();
			if (token.kind == Lbracket) {                 /* 配列だったら */
				op = stoa;
				token = nextToken();
				expression();
				token = checkGet(token, Rbracket);
			}

			token = checkGet(token, Assign);              /* ":="のはず */
			expression();                                 /* 式のコンパイル */
			genCodeT(op, tIndex);
			return;
		case If:                                          /* if文のコンパイル */
			token = nextToken();
			condition();                                  /* 条件式のコンパイル */
			token = checkGet(token, Then);                /* "then"のはず */
			backP = genCodeV(jpc, 0);                     /* jpc命令 */
			statement();                                  /* 文のコンパイル */
			if (token.kind == Else) {
				token = nextToken();
				backP2 = genCodeV(jmp, 0);
				backPatch(backP);
				statement();
				backPatch(backP2);
			}
			else
				backPatch(backP);
			return;
		case Unless:
			token = nextToken();
			condition();                                  /* 条件式のコンパイル */
			backP = genCodeV(jpc, 0);                     /* jpc命令 */
			backP2 = genCodeV(jmp, 0);                    /* jmp命令 */
			token = checkGet(token, Then);                /* "then"のはず */
			backPatch(backP);                             /* 上のjpc命令にバックパッチ */
			statement();                                  /* 文のコンパイル */
			backPatch(backP2);                            /* 上のjmp命令にバックパッチ */
			return;
		case Ret:                                         /* return文のコンパイル */
			token = nextToken();
			if ( inProcedureBlock() )
				genCodeR(1);                              /* ret命令 */
			else {
				expression();                             /* 式のコンパイル */
				genCodeR(0);                              /* ret命令 */
			}
			return;
		case Begin:                                       /* begin ... end文のコンパイル */
			token = nextToken();
			while(1) {
				statement();                              /* 文のコンパイル */
				while(1) {
					if (token.kind == Semicolon) {        /* 次が";"なら文が続く */
						token = nextToken();
						break;
					}
					if (token.kind == End) {              /* 次がendなら終り */
						token = nextToken();
						return;
					}
					if ( isStBeginKey(token) ) {          /* 次が文の先頭記号なら */
						errorInsert(Semicolon);           /* ";"を忘れたことにする */
						break;
					}
					errorDelete();                        /* それ以外ならエラーとして読み捨てる */
					token = nextToken();
				}
			}
			return;
		case While:                                       /* while文のコンパイル */
			token = nextToken();
			backP2 = nextCode();                          /* while文の最後のjmp命令の飛び先 */
			condition();                                  /* 条件式のコンパイル */
			token = checkGet(token, Do);                  /* "do"のはず */
			backP = genCodeV(jpc, 0);                     /* 条件式が偽のとき飛び出すjpc命令 */
			statement();                                  /* 文のコンパイル */
			genCodeV(jmp, backP2);                        /* while文の先頭へのジャンプ命令 */
			backPatch(backP);                             /* 偽のとき飛び出すjpc命令へのバックパッチ */
			return;
		case Do:
			token = nextToken();
			backP = nextCode();
			statement();
			token = checkGet(token, While);
			condition();
			backP2 = genCodeV(jpc, 0);
			genCodeV(jmp, backP);
			backPatch(backP2);
			return;
		case Repeat:
			token = nextToken();
			backP = nextCode();
			statement();
			token = checkGet(token, Until);
			condition();
			genCodeV(jpc, backP);
			return;
		case For:
			token = nextToken();
			statement();
			token = checkGet(token, Semicolon);
			backP4 = nextCode();
			condition();
			backP2 = genCodeV(jpc, 0);
			token = checkGet(token, Semicolon);
			backP = genCodeV(jmp, 0);
			backP3 = nextCode();
			statement();  // increment
			genCodeV(jmp, backP4);
			backPatch(backP);
			token = checkGet(token, Do);
			statement();
			genCodeV(jmp, backP3);
			backPatch(backP2);
			return;
		case Call:
			token = nextToken();
			tIndex = searchT(token.u.id, procId);
			setIdKind(k = kindT(tIndex));                 /* 印字のための情報のセット */
			if (k == procId) {
				token = nextToken();
				if (token.kind == Lparen) {
					int i = 0;                            /* iは実引数の個数 */
					token = nextToken();
					if (token.kind != Rparen) {
						for (; ; ) {
							expression(); i++;            /* 実引数のコンパイル */
							if (token.kind == Comma) {    /* 次がコンマなら実引数が続く */
								token = nextToken();
								continue;
							}
							token = checkGet(token, Rparen);
							break;
						}
					}
					else
						token = nextToken();
					if (pars(tIndex) != i)
						errorMessage("\\#par");           /* pars(tIndex)は仮引数の個数 */
				}
				else {
					errorInsert(Lparen);
					errorInsert(Rparen);
				}
				genCodeT(cal, tIndex);                    /* call命令 */
			}
			else
				errorType("proc");
			return;
		case Write:                                       /* write文のコンパイル */
			token = nextToken();
			expression();                                 /* 式のコンパイル */
			genCodeO(wrt);                                /* その値を出力するwrt命令 */
			return;
		case WriteLn:                                     /* writeln文のコンパイル */
			token = nextToken();
			genCodeO(wrl);                                /* 改行を出力するwrl命令 */
			return;
		case End:
		case Semicolon:                                   /* 空文を読んだことにして終り */
			return;
		default:                                          /* 文の先頭のキーまで読み捨てる */
			errorDelete();                                /* 今読んだトークンを読み捨てる */
			token = nextToken();
			continue;
		}
	}
}

/* トークンtは文の先頭のキーか? */
int isStBeginKey(Token t)
{
	switch (t.kind) {
	case Begin:
	case Call:
	case Do:
	case For:
	case Id:
	case If:
	case Repeat:
	case Ret:
	case Unless:
	case While:
	case Write:
	case WriteLn:
		return 1;
	default:
		return 0;
	}
}

/* 式のコンパイル */
void expression()
{
	KeyId k;
	k = token.kind;
	if (k == Plus || k == Minus) {
		token = nextToken();
		term();
		if (k == Minus)
			genCodeO(neg);
	}
	else
		term();
	k = token.kind;
	while (k == Plus || k == Minus) {
		token = nextToken();
		term();
		if (k == Minus)
			genCodeO(sub);
		else
			genCodeO(add);
		k = token.kind;
	}
}

/* 式の項のコンパイル */
void term()
{
	KeyId k;
	factor();
	k = token.kind;
	while (k == Mult || k == Div) {
		token = nextToken();
		factor();
		if (k == Mult)
			genCodeO(mul);
		else
			genCodeO(div);
		k = token.kind;
	}
}

/* 式の因子のコンパイル */
void factor()
{
	int tIndex, i;
	KeyId k;
	if (token.kind == Id) {
		tIndex = searchT(token.u.id, varId);
		setIdKind(k = kindT(tIndex));                 /* 印字のための情報のセット */
		switch ( k ) {
		case varId:
		case parId:                                   /* 変数名かパラメタ名 */
			genCodeT(lod, tIndex);
			token = nextToken();
			if (token.kind == Lbracket) {             /* 配列だったら */
				token = nextToken();
				expression();
				genCodeT(loda, tIndex);
				token = checkGet(token, Rbracket);
			}
			break;
		case constId:                                 /* 定数名 */
			genCodeV(lit, val(tIndex));
			token = nextToken();
			break;
		case funcId:                                  /* 関数呼び出し */
			token = nextToken();
			if (token.kind == Lparen) {
				i = 0;                                /* iは実引数の個数 */
				token = nextToken();
				if (token.kind != Rparen) {
					for (; ; ) {
						expression(); i++;            /* 実引数のコンパイル */
						if (token.kind == Comma) {    /* 次がコンマなら実引数が続く */
							token = nextToken();
							continue;
						}
						token = checkGet(token, Rparen);
						break;
					}
				}
				else
					token = nextToken();
				if (pars(tIndex) != i)
					errorMessage("\\#par");           /* pars(tIndex)は仮引数の個数 */
			}
			else {
				errorInsert(Lparen);
				errorInsert(Rparen);
			}
			genCodeT(cal, tIndex);                    /* call命令 */
			break;
		}
	}
	/* 定数 */
	else if (token.kind == Num) {
		genCodeV(lit, token.u.value);
		token = nextToken();
	}
	/* 「(」「因子」「)」 */
	else if (token.kind == Lparen) {
		token = nextToken();
		expression();
		token = checkGet(token, Rparen);
	}

	/* 因子の後がまた因子ならエラー */
	switch (token.kind) {
	case Id:
	case Num:
	case Lparen:
		errorMissingOp();
		factor();
	default:
		return;
	}
}

/* 条件式のコンパイル */
void condition()
{
	KeyId k;
	if (token.kind == Odd) {
		token = nextToken();
		expression();
		genCodeO(odd);
	}
	else {
		expression();
		k = token.kind;
		switch ( k ) {
		case Equal:
		case Lss:
		case Gtr:
		case NotEq:
		case LssEq:
		case GtrEq:
			break;
		default:
			errorType("rel-op");
			break;
		}
		token = nextToken();
		expression();
		switch ( k ) {
		case Equal:
			genCodeO(eq);
			break;
		case Lss:
			genCodeO(ls);
			break;
		case Gtr:
			genCodeO(gr);
			break;
		case NotEq:
			genCodeO(neq);
			break;
		case LssEq:
			genCodeO(lseq);
			break;
		case GtrEq:
			genCodeO(greq);
			break;
		}
	}
}
