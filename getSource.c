/********** getSource.c **********/
#include <stdio.h>
#include <string.h>
#include "getSource.h"

#define MAXLINE  120          /* １行の最大文字数 */
#define MAXERROR 30           /* これ以上のエラーがあったら終り */
#define MAXNUM   14           /* 定数の最大桁数 */
#define TAB      5            /* タブのスペース */
#define INSERT_C "#0000FF"    /* 挿入文字の色 */
#define DELETE_C "#FF0000"    /* 削除文字の色 */
#define TYPE_C   "#00FF00"    /* タイプエラー文字の色 */

static FILE *fpi;                /* ソースファイル */
static FILE *fptex;              /* LaTeX出力ファイル */
static char line[MAXLINE];       /* １行分の入力バッファー */
static int lineIndex;            /* 次に読む文字の位置 */
static char ch;                  /* 最後に読んだ文字 */

static Token cToken;             /* 最後に読んだトークン */
static KindT idKind;             /* 現トークン(Id)の種類 */
static int spaces;               /* そのトークンの前のスペースの個数 */
static int CR;                   /* その前のCRの個数 */
static int printed;              /* トークンは印字済みか */

static int errorNo = 0;          /* 出力したエラーの数 */
static char nextChar();          /* 次の文字を読む関数 */
static int isKeySym(KeyId k);    /* tは記号か? */
static int isKeyWd(KeyId k);     /* tは予約語か? */
static void printSpaces();       /* トークンの前のスペースの印字 */
static void printcToken();       /* トークンの印字 */

/* 予約語や記号と名前(KeyId) */
struct keyWd {
	char *word;
	KeyId keyId;
};

/* 予約語や記号と名前(KeyId)の表 */
static struct keyWd KeyWdT[] = {
	{"begin", Begin},
	{"end", End},
	{"if", If},
	{"then", Then},
	{"else", Else},
	{"unless", Unless},
	{"while", While},
	{"do", Do},
	{"repeat", Repeat},
	{"until", Until},
	{"return", Ret},
	{"function", Func},
	{"var", Var},
	{"const", Const},
	{"odd", Odd},
	{"procedure", Proc},
	{"write", Write},
	{"writeln",WriteLn},
	{"$dummy1",end_of_KeyWd},
	/* 記号と名前(KeyId)の表 */
	{"+", Plus},
	{"-", Minus},
	{"*", Mult},
	{"/", Div},
	{"(", Lparen},
	{")", Rparen},
	{"=", Equal},
	{"<", Lss},
	{">", Gtr},
	{"<>", NotEq},
	{"<=", LssEq},
	{">=", GtrEq},
	{",", Comma},
	{".", Period},
	{";", Semicolon},
	{":=", Assign},
	{"$dummy2",end_of_KeySym}
};

/* キーkは予約語か? */
int isKeyWd(KeyId k)
{
	return (k < end_of_KeyWd);
}

/* キーkは記号か? */
int isKeySym(KeyId k)
{
	if (k < end_of_KeyWd)
		return 0;
	return (k < end_of_KeySym);
}

/* 文字の種類を示す表にする */
static KeyId charClassT[256];

/* 文字の種類を示す表を作る関数 */
static void initCharClassT()
{
	int i;
	for (i = 0; i < 256; i++)
		charClassT[i] = others;
	for (i = '0'; i <= '9'; i++)
		charClassT[i] = digit;
	for (i = 'A'; i <= 'Z'; i++)
		charClassT[i] = letter;
	for (i = 'a'; i <= 'z'; i++)
		charClassT[i] = letter;
	charClassT['+'] = Plus; charClassT['-'] = Minus;
	charClassT['*'] = Mult; charClassT['/'] = Div;
	charClassT['('] = Lparen; charClassT[')'] = Rparen;
	charClassT['='] = Equal; charClassT['<'] = Lss;
	charClassT['>'] = Gtr; charClassT[','] = Comma;
	charClassT['.'] = Period; charClassT[';'] = Semicolon;
	charClassT[':'] = colon;
}

/* ソースファイルのopen */
int openSource(char fileName[])
{
	char fileNameO[30];
	if ((fpi = fopen(fileName,"r")) == NULL) {
		printf("can't open %s\n", fileName);
		return 0;
	}
	strcpy(fileNameO, fileName);
#if defined(LATEX)
	strcat(fileNameO,".tex");
#elif defined(TOKEN_HTML)
	strcat(fileNameO,".html");
#else
	strcat(fileNameO,".html");
#endif
	/* .html(または.tex)ファイルを作る */
	if ((fptex = fopen(fileNameO, "w")) == NULL) {
		printf("can't open %s\n", fileNameO);
		return 0;
	}
	return 1;
}

/* ソースファイルと.html(または.tex)ファイルをclose */
void closeSource()
{
	fclose(fpi);
	fclose(fptex);
}

void initSource()
{
	lineIndex = -1;    /* 初期設定 */
	ch = '\n';
	printed = 1;
	initCharClassT();
#if defined(LATEX)
	fprintf(fptex,"\\documentstyle[12pt]{article}\n");
	fprintf(fptex,"\\begin{document}\n");
	fprintf(fptex,"\\fboxsep=0pt\n");
	fprintf(fptex,"\\def\\insert#1{$\\fbox{#1}$}\n");
	fprintf(fptex,"\\def\\delete#1{$\\fboxrule=.5mm\\fbox{#1}$}\n");
	fprintf(fptex,"\\rm\n");
#elif defined(TOKEN_HTML)
	fprintf(fptex,"<HTML>\n");   /* htmlコマンド */
	fprintf(fptex,"<HEAD>\n<TITLE>compiled source program</TITLE>\n</HEAD>\n");
	fprintf(fptex,"<BODY>\n<PRE>\n");
#else
	fprintf(fptex,"<HTML>\n");   /* htmlコマンド */
	fprintf(fptex,"<HEAD>\n<TITLE>compiled source program</TITLE>\n</HEAD>\n");
	fprintf(fptex,"<BODY>\n<PRE>\n");
#endif
}

void finalSource()
{
	if (cToken.kind == Period)
		printcToken();
	else
		errorInsert(Period);
#if defined(LATEX)
	fprintf(fptex,"\n\\end{document}\n");
#elif defined(TOKEN_HTML)
	fprintf(fptex,"\n</PRE>\n</BODY>\n</HTML>\n");
#else
	fprintf(fptex,"\n</PRE>\n</BODY>\n</HTML>\n");
#endif
}

/* 通常のエラーメッセージの出力の仕方(参考まで) */
/*
 * void error(char *m)
 * {
 *     if (lineIndex > 0)
 *         printf("%*s\n", lineIndex, "***^");
 *     else
 *         printf("^\n");
 *     printf("*** error *** %s\n", m);
 *     errorNo++;
 *     if (errorNo > MAXERROR) {
 *         printf("too many errors\n");
 *         printf("abort compilation\n");
 *         exit (1);
 *     }
 * }
 */

/* エラーの個数のカウント、多すぎたら終わり */
void errorNoCheck()
{
	if (errorNo++ > MAXERROR) {
#if defined(LATEX)
		fprintf(fptex, "too many errors\n\\end{document}\n");
#elif defined(TOKEN_HTML)
		fprintf(fptex, "too many errors\n</PRE>\n</BODY>\n</HTML>\n");
#else
		fprintf(fptex, "too many errors\n</PRE>\n</BODY>\n</HTML>\n");
#endif
		printf("; abort compilation\n");
		exit(1);
	}
}

/* 型エラーを.html(または.tex)ファイルに出力 */
void errorType(char *m)
{
	printSpaces();
#if defined(LATEX)
	fprintf(fptex, "\\(\\stackrel{\\mbox{\\scriptsize %s}}{\\mbox{", m);
	printcToken();
	fprintf(fptex, "}}\\)");
#elif defined(TOKEN_HTML)
	fprintf(fptex, "<FONT COLOR=%s>", TYPE_C);
	fprintf(fptex, "TypeError(%s)-&gt", m);
	printcToken();
	fprintf(fptex, "</FONT>");
#else
	fprintf(fptex, "<FONT COLOR=%s>%s</FONT>", TYPE_C, m);
	printcToken();
#endif
	errorNoCheck();
}

/* keyString(k)を.html(または.tex)ファイルに挿入 */
void errorInsert(KeyId k)
{
#if defined(LATEX)
	/* 予約語 */
	if (k < end_of_KeyWd)
		fprintf(fptex, "\\ \\insert{{\\bf %s}}", KeyWdT[k].word);
	/* 演算子か区切り記号 */
	else
		fprintf(fptex, "\\ \\insert{$%s$}", KeyWdT[k].word);
#elif defined(TOKEN_HTML)
	fprintf(fptex, "<FONT COLOR=%s>", INSERT_C);
	fprintf(fptex, "insert ");
	/* 予約語 */
	if (k < end_of_KeyWd)
		fprintf(fptex, "(Keyword, '%s')", KeyWdT[k].word);
	/* 演算子か区切り記号 */
	else
		fprintf(fptex, "(Symbol, '%s')", KeyWdT[k].word);
	fprintf(fptex, "</FONT>");
#else
	fprintf(fptex, "<FONT COLOR=%s><b>%s</b></FONT>", INSERT_C, KeyWdT[k].word);
#endif
	errorNoCheck();
}

/* 名前がないとのメッセージを.html(または.tex)ファイルに挿入 */
void errorMissingId()
{
#if defined(LATEX)
	fprintf(fptex, "\\insert{Id}");
#elif defined(TOKEN_HTML)
	fprintf(fptex, "<FONT COLOR=%s>", INSERT_C);
	fprintf(fptex, "insert ");
	fprintf(fptex, "(Id, identifier)");
	fprintf(fptex, "</FONT>");
#else
	fprintf(fptex, "<FONT COLOR=%s>Id</FONT>", INSERT_C);
#endif
	errorNoCheck();
}

/* 演算子がないとのメッセージを.html(または.tex)ファイルに挿入 */
void errorMissingOp()
{
#if defined(LATEX)
	fprintf(fptex, "\\insert{$\\otimes$}");
#elif defined(TOKEN_HTML)
	fprintf(fptex, "<FONT COLOR=%s>", INSERT_C);
	fprintf(fptex, "insert ");
	fprintf(fptex, "(Symbol, operator)");
	fprintf(fptex, "</FONT>");
#else
	fprintf(fptex, "<FONT COLOR=%s>@</FONT>", INSERT_C);
#endif
	errorNoCheck();
}

/* 今読んだトークンを読み捨てる */
void errorDelete()
{
	int i = (int)cToken.kind;
	printSpaces();
	printed = 1;
#if defined(LATEX)
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "\\delete{{\\bf %s}}", KeyWdT[i].word);
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "\\delete{$%s$}", KeyWdT[i].word);
	}
	/* Identfier */
	else if (i == (int)Id) {
		fprintf(fptex, "\\delete{%s}", cToken.u.id);
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "\\delete{%d}", cToken.u.value);
	}
#elif defined(TOKEN_HTML)
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "<FONT COLOR=%s>", DELETE_C);
		fprintf(fptex, "delete ");
		fprintf(fptex, "(Keyword, '%s')", KeyWdT[i].word);
		fprintf(fptex, "</FONT>");
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "<FONT COLOR=%s>", DELETE_C);
		fprintf(fptex, "delete ");
		fprintf(fptex, "(Symbol, '%s')", KeyWdT[i].word);
		fprintf(fptex, "</FONT>");
	}
	/* Identfier */
	else if (i == (int)Id) {
		fprintf(fptex, "<FONT COLOR=%s>", DELETE_C);
		fprintf(fptex, "delete ");
		fprintf(fptex, "(Id, '%s')", cToken.u.id);
		fprintf(fptex, "</FONT>");
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "<FONT COLOR=%s>", DELETE_C);
		fprintf(fptex, "delete ");
		fprintf(fptex, "(number, '%d')", cToken.u.value);
		fprintf(fptex, "</FONT>");
	}
#else
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "<FONT COLOR=%s><b>%s</b></FONT>", DELETE_C, KeyWdT[i].word);
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "<FONT COLOR=%s>%s</FONT>", DELETE_C, KeyWdT[i].word);
	}
	/* Identfier */
	else if (i == (int)Id) {
		fprintf(fptex, "<FONT COLOR=%s>%s</FONT>", DELETE_C, cToken.u.id);
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "<FONT COLOR=%s>%d</FONT>", DELETE_C, cToken.u.value);
	}
#endif
}

/* エラーメッセージを.html(または.tex)ファイルに出力 */
void errorMessage(char *m)
{
#if defined(LATEX)
	fprintf(fptex, "$^{%s}$", m);
#elif defined(TOKEN_HTML)
	fprintf(fptex, "<FONT COLOR=%s>%s</FONT>", TYPE_C, m);
#else
	fprintf(fptex, "<FONT COLOR=%s>%s</FONT>", TYPE_C, m);
#endif
	errorNoCheck();
}

/* エラーメッセージを出力し、コンパイル終了 */
void errorF(char *m)
{
	errorMessage(m);
#if defined(LATEX)
	fprintf(fptex, "fatal errors\n\\end{document}\n");
#elif defined(TOKEN_HTML)
	fprintf(fptex, "fatal errors\n</PRE>\n</BODY>\n</HTML>\n");
#else
	fprintf(fptex, "fatal errors\n</PRE>\n</BODY>\n</HTML>\n");
#endif
	if (errorNo)
		printf("; total %d errors\n", errorNo);
	printf("; abort compilation\n");
	exit(1);
}

/* エラーの個数を返す */
int errorN()
{
	return errorNo;
}

/* 次の１文字を返す関数 */
char nextChar()
{
	char ch;
	if (lineIndex == -1) {
		if (fgets(line, MAXLINE, fpi) != NULL) {
			// puts(line);                     /* 通常のエラーメッセージの出力の場合(参考まで) */
			lineIndex = 0;
		} else {
			errorF("end of file\n");           /* end of fileならコンパイル終了 */
		}
	}
	if ((ch = line[lineIndex++]) == '\n') {    /* chに次の１文字 */
		lineIndex = -1;                        /* それが改行文字なら次の行の入力準備 */
		return '\n';                           /* 文字としては改行文字を返す */
	}
	return ch;
}

/* 次のトークンを読んで返す関数 */
Token nextToken()
{
	int i = 0;
	int num;
	KeyId cc;
	Token temp;
	char ident[MAXNAME];
	printcToken();          /* 前のトークンを印字 */
	spaces = 0; CR = 0;
	while (1) {             /* 次のトークンまでの空白や改行をカウント */
		if (ch == ' ')
			spaces++;
		else if	(ch == '\t')
			spaces += TAB;
		else if (ch == '\n') {
			spaces = 0;  CR++;
		}
		else break;
		ch = nextChar();
	}
	switch (cc = charClassT[ch]) {
	case letter:  /* identifier */
		do {
			if (i < MAXNAME)
				ident[i] = ch;
			i++; ch = nextChar();
		} while (charClassT[ch] == letter || charClassT[ch] == digit);
		if (i >= MAXNAME) {
			errorMessage("too long");
			i = MAXNAME - 1;
		}
		ident[i] = '\0';
		for (i = 0; i < end_of_KeyWd; i++)
			/* 予約語の場合 */
			if (!strcmp(ident, KeyWdT[i].word)) {
				temp.kind = KeyWdT[i].keyId;
				cToken = temp; printed = 0;
				return temp;
			}
		/* ユーザの宣言した名前の場合 */
		temp.kind = Id;
		strcpy(temp.u.id, ident);
		break;
	case digit:  /* number */
		num = 0;
		do {
			num = 10 * num + (ch - '0');
			i++; ch = nextChar();
		} while (charClassT[ch] == digit);
		if (i > MAXNUM)
			errorMessage("too large");
		temp.kind = Num;
		temp.u.value = num;
		break;
	case colon:
		/* ":=" */
		if ((ch = nextChar()) == '=') {
			ch = nextChar();
			temp.kind = Assign;
		}
		else {
			temp.kind = nul;
		}
		break;
	case Lss:
		/* "<=" */
		if ((ch = nextChar()) == '=') {
			ch = nextChar();
			temp.kind = LssEq;
		}
		/* "<>" */
		else if (ch == '>') {
			ch = nextChar();
			temp.kind = NotEq;
		}
		else {
			temp.kind = Lss;
		}
		break;
	case Gtr:
		/* ">=" */
		if ((ch = nextChar()) == '=') {
			ch = nextChar();
			temp.kind = GtrEq;
		}
		else {
			temp.kind = Gtr;
		}
		break;
	default:
		temp.kind = cc;
		ch = nextChar();
		break;
	}
	cToken = temp; printed = 0;
	return temp;
}

/*
 * t.kind == kのチェック
 * t.kind == kなら、次のトークンを読んで返す
 * t.kind != kならエラーメッセージを出し、tとkが共に記号、または予約語なら
 * tを捨て、次のトークンを読んで返す(tをkで置き換えたことになる)
 * それ以外の場合、kを挿入したことにして、tを返す
 */
Token checkGet(Token t, KeyId k)
{
	if (t.kind==k)
		return nextToken();
	if ((isKeyWd(k) && isKeyWd(t.kind)) || (isKeySym(k) && isKeySym(t.kind))) {
		errorDelete();
		errorInsert(k);
		return nextToken();
	}
	errorInsert(k);
	return t;
}

/* 空白や改行の印字 */
static void printSpaces()
{
#if defined(LATEX)
	while (CR-- > 0) {
		fprintf(fptex, "\\ \\par\n");
	}
	while (spaces-- > 0) {
		fprintf(fptex, " ");
		fprintf(fptex, "\\ ");
	}
#elif defined(TOKEN_HTML)
	while (CR-- > 0) {
		fprintf(fptex, "\n");
	}
	while (spaces-- > 0) {
		fprintf(fptex, " ");
	}
#else
	while (CR-- > 0) {
		fprintf(fptex, "\n");
	}
	while (spaces-- > 0) {
		fprintf(fptex, " ");
	}
#endif
	CR = 0; spaces = 0;
}

/* 現在のトークンの印字 */
void printcToken()
{
	int i = (int)cToken.kind;
	if (printed) {
		printed = 0; return;
	}
	printed = 1;
	/* トークンの前の空白や改行印字 */
	printSpaces();
#if defined(LATEX)
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "{\\bf %s}", KeyWdT[i].word);
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "$%s$", KeyWdT[i].word);
	}
	/* Identfier */
	else if (i == (int)Id) {
		switch (idKind) {
		case varId:
			fprintf(fptex, "%s", cToken.u.id);
			return;
		case parId:
			fprintf(fptex, "{\\sl %s}", cToken.u.id);
			return;
		case funcId:
			fprintf(fptex, "{\\it %s}", cToken.u.id);
			return;
		case constId:
			fprintf(fptex, "{\\sf %s}", cToken.u.id);
			return;
		}
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "%d", cToken.u.value);
	}
#elif defined(TOKEN_HTML)
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "(Keyword, '%s') ", KeyWdT[i].word);
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "(Symbol, '%s') ", KeyWdT[i].word);
	}
	/* Identfier */
	else if (i==(int)Id) {
		switch (idKind) {
		case varId:
			fprintf(fptex, "(varId, '%s') ", cToken.u.id);
			return;
		case parId:
			fprintf(fptex, "(parId, '%s') ", cToken.u.id);
			return;
		case funcId:
			fprintf(fptex, "(funcId, '%s') ", cToken.u.id);
			return;
		case constId:
			fprintf(fptex, "(constId, '%s') ", cToken.u.id);
			return;
		}
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "(number, '%d') ", cToken.u.value);
	}
#else
	/* 予約語 */
	if (i < end_of_KeyWd) {
		fprintf(fptex, "<b>%s</b>", KeyWdT[i].word);
	}
	/* 演算子か区切り記号 */
	else if (i < end_of_KeySym) {
		fprintf(fptex, "%s", KeyWdT[i].word);
	}
	/* Identfier */
	else if (i == (int)Id) {
		switch (idKind) {
		case varId:
			fprintf(fptex, "%s", cToken.u.id);
			return;
		case parId:
			fprintf(fptex, "<i>%s</i>", cToken.u.id);
			return;
		case funcId:
			fprintf(fptex, "<i>%s</i>", cToken.u.id);
			return;
		case constId:
			fprintf(fptex, "<tt>%s</tt>", cToken.u.id);
			return;
		}
	}
	/* Num */
	else if (i == (int)Num) {
		fprintf(fptex, "%d", cToken.u.value);
	}
#endif
}

/* 現トークン(Id)の種類をセット */
void setIdKind(KindT k)
{
	idKind = k;
}
