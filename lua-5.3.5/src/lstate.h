/*
** $Id: lstate.h,v 2.133.1.1 2017/04/19 17:39:34 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#ifndef lstate_h
#define lstate_h

#include "lua.h"

#include "lobject.h"
#include "ltm.h"
#include "lzio.h"


/*

** Some notes about garbage-collected objects: All objects in Lua must
** be kept somehow accessible until being freed, so all objects always
** belong to one (and only one) of these lists, using field 'next' of
** the 'CommonHeader' for the link:
**
** 'allgc': all objects not marked for finalization;
** 'finobj': all objects marked for finalization;
** 'tobefnz': all objects ready to be finalized;
** 'fixedgc': all objects that are not to be collected (currently
** only small strings, such as reserved words).
**
** Moreover, there is another set of lists that control gray objects.
** These lists are linked by fields 'gclist'. (All objects that
** can become gray have such a field. The field is not the same
** in all objects, but it always has this name.)  Any gray object
** must belong to one of these lists, and all objects in these lists
** must be gray:
**
** 'gray': regular gray objects, still waiting to be visited.
** 'grayagain': objects that must be revisited at the atomic phase.
**   That includes
**   - black objects got in a write barrier;
**   - all kinds of weak tables during propagation phase;
**   - all threads.
** 'weak': tables with weak values to be cleared;
** 'ephemeron': ephemeron tables with white->white entries;
** 'allweak': tables with weak keys and/or weak values to be cleared.
** The last three lists are used only during the atomic phase.

*/


struct lua_longjmp;  /* defined in ldo.c */


/*
** Atomic type (relative to signals) to better ensure that 'lua_sethook'
** is thread safe
*/
#if !defined(l_signalT)
#include <signal.h>
#define l_signalT	sig_atomic_t
#endif


/* extra stack space to handle TM calls and some other extras */
#define EXTRA_STACK   5


#define BASIC_STACK_SIZE        (2*LUA_MINSTACK)


/* kinds of Garbage Collection */
#define KGC_NORMAL	0
#define KGC_EMERGENCY	1	/* gc was forced by an allocation failure */


typedef struct stringtable {
  TString **hash;
  int nuse;  /* number of elements */
  int size;
} stringtable;


/*
** Information about a call.
** When a thread yields, 'func' is adjusted to pretend that the
** top function has only the yielded values in its stack; in that
** case, the actual 'func' value is saved in field 'extra'.
** When a function calls another with a continuation, 'extra' keeps
** the function index so that, in case of errors, the continuation
** function can be called with the correct top.
*/
typedef struct CallInfo {
	StkId func;  /* 当前调用栈的调用指针处 function index in the stack  需要记录这个信息，是因为如果当期是一个 Lua 函数，且传入的参数个数不定的时候，需要用这个位置和当前数据栈底 的位置相减，获得不定参数的数据数量  */
	StkId	top;  /* 调用栈的栈顶 top for this function */
	struct CallInfo *previous, *next;  /* dynamic call link */
	union {
		struct {  /* only for Lua functions */
			StkId base;  /* base for this function */
			const Instruction *savedpc;
		} l;
		struct {  /* only for C functions */
			lua_KFunction k;  /* continuation in case of yields */
			ptrdiff_t old_errfunc;
			lua_KContext ctx;  /* context info. in case of yields */
		} c;
	} u;
	ptrdiff_t extra;
	short nresults;  /* expected number of results from this function */
	unsigned short callstatus; /* CallInfo 保存着正在调用的函数的运行状态。状态标识存放在 callstatus 中。 C 函数与Lua 函数的结构不完全相同，在 callstatus 中的保存了一位标志用来区分是 C 函数还是 Lua 函数 */
} CallInfo;


/*
** Bits in CallInfo status
*/
#define CIST_OAH	(1<<0)	/* original value of 'allowhook' */
#define CIST_LUA	(1<<1)	/* call is running a Lua function */
#define CIST_HOOKED	(1<<2)	/* call is running a debug hook */
#define CIST_FRESH	(1<<3)	/* call is running on a fresh invocation
                                   of luaV_execute */
#define CIST_YPCALL	(1<<4)	/* call is a yieldable protected call */
#define CIST_TAIL	(1<<5)	/* call was tail called */
#define CIST_HOOKYIELD	(1<<6)	/* last hook called yielded */
#define CIST_LEQ	(1<<7)  /* using __lt for __le */
#define CIST_FIN	(1<<8)  /* call is running a finalizer */

#define isLua(ci)	((ci)->callstatus & CIST_LUA)/* Lua 函数 */

/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */
#define setoah(st,v)	((st) = ((st) & ~CIST_OAH) | (v))
#define getoah(st)	((st) & CIST_OAH)


/*
** 'global state', shared by all threads of this state
** lua 全局状态机
** 作用：管理全局数据，全局字符串表、内存管理函数、 GC 把所有对象串联起来的信息、内存等
*/
typedef struct global_State {

	/* 版本号  */
	const lua_Number *version;  /* pointer to version number */

	/* 内存管理 */
	lua_Alloc frealloc;  /* Lua的全局内存分配器，用户可以替换成自己的 - function to reallocate memory */
	void *ud;         /* 分配器的userdata - auxiliary data to 'frealloc' */

	/* 线程管理 */
	struct lua_State *mainthread; /* 主线程 */
	struct lua_State *twups;  /* 闭包了当前线程变量的其他线程列表 - list of threads with open upvalues */

	/* 字符串管理 */
	stringtable strt;  /* 字符串table Lua的字符串分短字符串和长字符串 - hash table for strings */
	TString *strcache[STRCACHE_N][STRCACHE_M];  /* 字符串缓存 - cache for strings in API */

	/* 虚函数表 */
	TString *tmname[TM_N];  /* 预定义方法名字数组 -  array with tag-method names */
	struct Table *mt[LUA_NUMTAGS];  /* 每个基本类型一个metatable（整个Lua最重要的Hook机制） - metatables for basic types */

	/* 错误处理 */
	lua_CFunction panic;  /* to be called in unprotected errors */
	TString *memerrmsg;  /* memory-error message */

	/* GC管理 */
	unsigned int gcfinnum;  /* number of finalizers to call in each GC step */
	int gcpause;  /* size of pause between successive GCs */
	int gcstepmul;  /* GC 'granularity' */

	l_mem totalbytes;  /* number of bytes currently allocated - GCdebt */
	l_mem GCdebt;  /* bytes allocated not yet compensated by the collector */
	lu_mem GCmemtrav;  /* memory traversed by the GC */
	lu_mem GCestimate;  /* an estimate of the non-garbage memory in use */

	TValue l_registry;
	unsigned int seed;  /* randomized seed for hashes */
	lu_byte currentwhite;
	lu_byte gcstate;  /* state of garbage collector */
	lu_byte gckind;  /* kind of GC running */
	lu_byte gcrunning;  /* true if GC is running */
	GCObject *allgc;  /* list of all collectable objects 长字符串，一般很少做索引或者比较，所以长字符串直接链接到allgc 链表上 做GC 对象来处理*/
	GCObject **sweepgc;  /* current position of sweep in list */
	GCObject *finobj;  /* list of collectable objects with finalizers */
	GCObject *gray;  /* list of gray objects */
	GCObject *grayagain;  /* list of objects to be traversed atomically */
	GCObject *weak;  /* list of tables with weak values */
	GCObject *ephemeron;  /* list of ephemeron tables (weak keys) */
	GCObject *allweak;  /* list of all-weak tables */
	GCObject *tobefnz;  /* list of userdata to be GC */
	GCObject *fixedgc;  /* list of objects not to be collected */

} global_State;


/*
** 'per thread' state
** Lua 主线程 栈 数据结构
** 作用：管理整个栈和当前函数使用的栈的情况，最主要的功能就是函数调用以及和c的通信
*/
struct lua_State {
	CommonHeader;/* Lua 类型公共头部 */
	lu_byte status; /* 解析容器的用于记录中间状态  每一个THREAD 实际上就是一个死循环解释执行指令的容器，本质上是一个状态机，该字段表示中间步骤的状态*/

	/* 全局状态机 Lua的全局对象，只存在一份，所有的 L 都共享这个 G */
	global_State *l_G;

	/* 调用栈：调用栈信息管理（CallInfo 为双向链表结构） 调用栈是一个链表 */
	unsigned short nci;  /* number of items in 'ci' list - 存储一共多少个CallInfo */
	CallInfo base_ci;  /* CallInfo for first level (C calling Lua) - 调用栈的头部指针 */
	CallInfo *ci;  /* call info for current function - 当前运行函数信息 */

	/* 数据栈：栈指针地址管理  StkId = TValue 的数组  数据栈是一个数组 */
	StkId top;  /* first free slot in the stack - 线程栈的栈顶指针 */
	StkId stack_last;  /* last free slot in the stack  - 线程栈的最后一个位置 */
	StkId stack;  /* stack base - 栈的指针，当前执行的位置*/


	const Instruction *oldpc;  /* last pc traced 在当前thread 的解释执行指令的过程中，指向最后一次执行的指令的指针 */
	UpVal *openupval;  /* list of open upvalues in this stack  Lua Closure的闭包变量，分为open和close两种状态，如果是close状态，则拷贝到LClosure自己的UpVal数组里，但如果是open状态，则直接指向了作用域上的变量地址 */
	GCObject *gclist; /* GC列表 */
	struct lua_State *twups;  /* list of threads with open upvalues  调用栈展开过程中，从调用栈的栈顶的所有open 状态的UpVal 也构成 栈结构（链表串起来的）， 一个lua-State 代表一个协程,一个协程能闭包其它协程的变量，所以 twups 就是代表 其它的协程（闭包了当前的lua-state 的变量）*/
	struct lua_longjmp *errorJmp;  /* current error recover point 异常错误处理：因为thread 是一个死循环解释执行指令，在这个过程中，出现的异常与错误需要处理 */

	/* Hook 相关管理 - 服务于debug模块 */
	volatile lua_Hook hook;
	ptrdiff_t errfunc;  /* current error handling function (stack index) */
	int stacksize; /* 数据栈:一个动态TValue 数组，所有的Lua object 都是TValue */
	int basehookcount;
	int hookcount;
	l_signalT hookmask;
	lu_byte allowhook;

	/* 跟C语言通信 管理*/
	unsigned short nCcalls;  /* number of nested C calls */
	unsigned short nny;  /* number of non-yieldable calls in stack */
};


#define G(L)	(L->l_G)


/*
** Union of all collectable objects (only for conversions)
*/
union GCUnion {
  GCObject gc;  /* common header */
  struct TString ts;
  struct Udata u;
  union Closure cl;
  struct Table h;
  struct Proto p;
  struct lua_State th;  /* thread */
};


#define cast_u(o)	cast(union GCUnion *, (o))

/* macros to convert a GCObject into a specific value */

/* 宏gco2ts是将GCObject对象转换为TString对象 */
#define gco2ts(o)  \
	check_exp(novariant((o)->tt) == LUA_TSTRING, &((cast_u(o))->ts))
#define gco2u(o)  check_exp((o)->tt == LUA_TUSERDATA, &((cast_u(o))->u))
#define gco2lcl(o)  check_exp((o)->tt == LUA_TLCL, &((cast_u(o))->cl.l))
#define gco2ccl(o)  check_exp((o)->tt == LUA_TCCL, &((cast_u(o))->cl.c))
#define gco2cl(o)  \
	check_exp(novariant((o)->tt) == LUA_TFUNCTION, &((cast_u(o))->cl))
#define gco2t(o)  check_exp((o)->tt == LUA_TTABLE, &((cast_u(o))->h))
#define gco2p(o)  check_exp((o)->tt == LUA_TPROTO, &((cast_u(o))->p))
#define gco2th(o)  check_exp((o)->tt == LUA_TTHREAD, &((cast_u(o))->th))


/* macro to convert a Lua object into a GCObject */
#define obj2gco(v) \
	check_exp(novariant((v)->tt) < LUA_TDEADKEY, (&(cast_u(v)->gc)))


/* actual number of total bytes allocated */
#define gettotalbytes(g)	cast(lu_mem, (g)->totalbytes + (g)->GCdebt)

LUAI_FUNC void luaE_setdebt (global_State *g, l_mem debt);
LUAI_FUNC void luaE_freethread (lua_State *L, lua_State *L1);
LUAI_FUNC CallInfo *luaE_extendCI (lua_State *L);
LUAI_FUNC void luaE_freeCI (lua_State *L);
LUAI_FUNC void luaE_shrinkCI (lua_State *L);


#endif

