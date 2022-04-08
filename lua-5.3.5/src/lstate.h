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
	StkId func;  /* ��ǰ����ջ�ĵ���ָ�봦 function index in the stack  ��Ҫ��¼�����Ϣ������Ϊ���������һ�� Lua �������Ҵ���Ĳ�������������ʱ����Ҫ�����λ�ú͵�ǰ����ջ�� ��λ���������ò�����������������  */
	StkId	top;  /* ����ջ��ջ�� top for this function */
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
	unsigned short callstatus; /* CallInfo ���������ڵ��õĺ���������״̬��״̬��ʶ����� callstatus �С� C ������Lua �����Ľṹ����ȫ��ͬ���� callstatus �еı�����һλ��־���������� C �������� Lua ���� */
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

#define isLua(ci)	((ci)->callstatus & CIST_LUA)/* Lua ���� */

/* assume that CIST_OAH has offset 0 and that 'v' is strictly 0/1 */
#define setoah(st,v)	((st) = ((st) & ~CIST_OAH) | (v))
#define getoah(st)	((st) & CIST_OAH)


/*
** 'global state', shared by all threads of this state
** lua ȫ��״̬��
** ���ã�����ȫ�����ݣ�ȫ���ַ������ڴ�������� GC �����ж�������������Ϣ���ڴ��
*/
typedef struct global_State {

	/* �汾��  */
	const lua_Number *version;  /* pointer to version number */

	/* �ڴ���� */
	lua_Alloc frealloc;  /* Lua��ȫ���ڴ���������û������滻���Լ��� - function to reallocate memory */
	void *ud;         /* ��������userdata - auxiliary data to 'frealloc' */

	/* �̹߳��� */
	struct lua_State *mainthread; /* ���߳� */
	struct lua_State *twups;  /* �հ��˵�ǰ�̱߳����������߳��б� - list of threads with open upvalues */

	/* �ַ������� */
	stringtable strt;  /* �ַ���table Lua���ַ����ֶ��ַ����ͳ��ַ��� - hash table for strings */
	TString *strcache[STRCACHE_N][STRCACHE_M];  /* �ַ������� - cache for strings in API */

	/* �麯���� */
	TString *tmname[TM_N];  /* Ԥ���巽���������� -  array with tag-method names */
	struct Table *mt[LUA_NUMTAGS];  /* ÿ����������һ��metatable������Lua����Ҫ��Hook���ƣ� - metatables for basic types */

	/* ������ */
	lua_CFunction panic;  /* to be called in unprotected errors */
	TString *memerrmsg;  /* memory-error message */

	/* GC���� */
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
	GCObject *allgc;  /* list of all collectable objects ���ַ�����һ��������������߱Ƚϣ����Գ��ַ���ֱ�����ӵ�allgc ������ ��GC ����������*/
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
** Lua ���߳� ջ ���ݽṹ
** ���ã���������ջ�͵�ǰ����ʹ�õ�ջ�����������Ҫ�Ĺ��ܾ��Ǻ��������Լ���c��ͨ��
*/
struct lua_State {
	CommonHeader;/* Lua ���͹���ͷ�� */
	lu_byte status; /* �������������ڼ�¼�м�״̬  ÿһ��THREAD ʵ���Ͼ���һ����ѭ������ִ��ָ�����������������һ��״̬�������ֶα�ʾ�м䲽���״̬*/

	/* ȫ��״̬�� Lua��ȫ�ֶ���ֻ����һ�ݣ����е� L ��������� G */
	global_State *l_G;

	/* ����ջ������ջ��Ϣ����CallInfo Ϊ˫������ṹ�� ����ջ��һ������ */
	unsigned short nci;  /* number of items in 'ci' list - �洢һ�����ٸ�CallInfo */
	CallInfo base_ci;  /* CallInfo for first level (C calling Lua) - ����ջ��ͷ��ָ�� */
	CallInfo *ci;  /* call info for current function - ��ǰ���к�����Ϣ */

	/* ����ջ��ջָ���ַ����  StkId = TValue ������  ����ջ��һ������ */
	StkId top;  /* first free slot in the stack - �߳�ջ��ջ��ָ�� */
	StkId stack_last;  /* last free slot in the stack  - �߳�ջ�����һ��λ�� */
	StkId stack;  /* stack base - ջ��ָ�룬��ǰִ�е�λ��*/


	const Instruction *oldpc;  /* last pc traced �ڵ�ǰthread �Ľ���ִ��ָ��Ĺ����У�ָ�����һ��ִ�е�ָ���ָ�� */
	UpVal *openupval;  /* list of open upvalues in this stack  Lua Closure�ıհ���������Ϊopen��close����״̬�������close״̬���򿽱���LClosure�Լ���UpVal������������open״̬����ֱ��ָ�����������ϵı�����ַ */
	GCObject *gclist; /* GC�б� */
	struct lua_State *twups;  /* list of threads with open upvalues  ����ջչ�������У��ӵ���ջ��ջ��������open ״̬��UpVal Ҳ���� ջ�ṹ�����������ģ��� һ��lua-State ����һ��Э��,һ��Э���ܱհ�����Э�̵ı��������� twups ���Ǵ��� ������Э�̣��հ��˵�ǰ��lua-state �ı�����*/
	struct lua_longjmp *errorJmp;  /* current error recover point �쳣��������Ϊthread ��һ����ѭ������ִ��ָ�����������У����ֵ��쳣�������Ҫ���� */

	/* Hook ��ع��� - ������debugģ�� */
	volatile lua_Hook hook;
	ptrdiff_t errfunc;  /* current error handling function (stack index) */
	int stacksize; /* ����ջ:һ����̬TValue ���飬���е�Lua object ����TValue */
	int basehookcount;
	int hookcount;
	l_signalT hookmask;
	lu_byte allowhook;

	/* ��C����ͨ�� ����*/
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

/* ��gco2ts�ǽ�GCObject����ת��ΪTString���� */
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

