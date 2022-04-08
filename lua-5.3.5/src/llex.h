/*
** $Id: llex.h,v 1.79.1.1 2017/04/19 17:20:42 roberto Exp $
** Lexical Analyzer
** See Copyright Notice in lua.h
*/

#ifndef llex_h
#define llex_h

#include "lobject.h"
#include "lzio.h"


#define FIRST_RESERVED	257


#if !defined(LUA_ENV)
#define LUA_ENV		"_ENV"
#endif


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER RESERVED"
*/

enum RESERVED {
	/* 系统默认关键字 terminal symbols denoted by reserved words */
	TK_AND = FIRST_RESERVED, TK_BREAK,
	TK_DO, TK_ELSE, TK_ELSEIF, TK_END, TK_FALSE, TK_FOR, TK_FUNCTION,
	TK_GOTO, TK_IF, TK_IN, TK_LOCAL, TK_NIL, TK_NOT, TK_OR, TK_REPEAT,
	TK_RETURN, TK_THEN, TK_TRUE, TK_UNTIL, TK_WHILE,
	/* 其它关键字 other terminal symbols */
	TK_IDIV, TK_CONCAT, TK_DOTS, TK_EQ, TK_GE, TK_LE, TK_NE,
	TK_SHL, TK_SHR,
	TK_DBCOLON, TK_EOS,
	TK_FLT, TK_INT, TK_NAME, TK_STRING
};


/* number of reserved words */
#define NUM_RESERVED	(cast(int, TK_WHILE-FIRST_RESERVED+1))

//语义辅助信息
typedef union {
	lua_Number r;
	lua_Integer i;
	TString *ts;
} SemInfo;  /* 语义信息 semantics information */

//语义分割最小单位Token
typedef struct Token {
	int token; //Token类型
	SemInfo seminfo; //语义信息，例如token为字符串/数字等都需要存储具体的值
} Token;



/*
 * 语法分析上下文状态
 * state of the lexer plus state of the parser when shared by all
   functions
	*/
typedef struct LexState {
	int current;  /* 解析字符指针 current character (charint) */
	int linenumber;  /* 行数计数器 input line counter */
	int lastline;  /* 最后一行 line of last token 'consumed' */
	Token t;  /* 当前Token current token */
	Token lookahead;  /* 头部Token look ahead token */
	struct FuncState *fs;  /* 当前解析的方法 current function (parser) */
	struct lua_State *L; //Lua栈
	ZIO *z;  /* io输入流 input stream */
	Mbuffer *buff;  /* buffer for tokens */
	Table *h;  /* to avoid collection/reuse strings */
	struct Dyndata *dyd;  /* dynamic structures used by the parser */
	TString *source;  /* 当前源名称 current source name */
	TString *envn;  /* 环境变量 environment variable name */
} LexState;


LUAI_FUNC void luaX_init (lua_State *L);
LUAI_FUNC void luaX_setinput (lua_State *L, LexState *ls, ZIO *z,
                              TString *source, int firstchar);
LUAI_FUNC TString *luaX_newstring (LexState *ls, const char *str, size_t l);
LUAI_FUNC void luaX_next (LexState *ls);
LUAI_FUNC int luaX_lookahead (LexState *ls);
LUAI_FUNC l_noret luaX_syntaxerror (LexState *ls, const char *s);
LUAI_FUNC const char *luaX_token2str (LexState *ls, int token);


#endif
