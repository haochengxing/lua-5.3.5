/*
** $Id: lstate.c,v 2.133.1.1 2017/04/19 17:39:34 roberto Exp $
** Global State
** See Copyright Notice in lua.h
*/

#define lstate_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "llex.h"
#include "lmem.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"


#if !defined(LUAI_GCPAUSE)
#define LUAI_GCPAUSE	200  /* 200% */
#endif

#if !defined(LUAI_GCMUL)
#define LUAI_GCMUL	200 /* GC runs 'twice the speed' of memory allocation */
#endif


/*
** a macro to help the creation of a unique random seed when a state is
** created; the seed is used to randomize hashes.
*/
#if !defined(luai_makeseed)
#include <time.h>
#define luai_makeseed()		cast(unsigned int, time(NULL))
#endif



/*
** thread state + extra space
* LX的定义
* 1:在lua_State之前留出了大小为LUAI_EXTRASPACE字节的空间。
* 2:面对外部用户操作的指针是L而不是LX,但L所占据的内存块的前面却是有所保留的。
* 3:这是一个有趣的技巧。用户可以在拿到L指针后向前移动指针,取得一些LUAT_EXTRASPACE中额外的数据。
* 4:把这些数据放在前面而不是lua_State结构的后面避免了向用户暴露结构的大小。
* 5:这里,LUAI_EXTRASPACE是通过编译配置的,默认为0;
* 6:LUAI EXTRASPACE,—ANNARRI (luai_userstateopen(L). ...)
* 7:给L附加一些用户自定义信息在追求性能的环境很有意义。可以在为Lua编写的C模块中,直接偏移L指针来获取一些附加信息。这比去读取L中的注册表要高效的多。
* 8:另一方面,在多线程环境下,访问注册表本身会改变L的状态,是线程不安全的。
*/
typedef struct LX {
  lu_byte extra_[LUA_EXTRASPACE];
  lua_State l;
} LX;


/*
** Main thread combines a thread state and the global state
*/
typedef struct LG {
  LX l;
  global_State g;
} LG;



#define fromstate(L)	(cast(LX *, cast(lu_byte *, (L)) - offsetof(LX, l)))


/*
** Compute an initial seed as random as possible. Rely on Address Space
** Layout Randomization (if present) to increase randomness..
*/
#define addbuff(b,p,e) \
  { size_t t = cast(size_t, e); \
    memcpy(b + p, &t, sizeof(t)); p += sizeof(t); }

static unsigned int makeseed (lua_State *L) {
  char buff[4 * sizeof(size_t)];
  unsigned int h = luai_makeseed();
  int p = 0;
  addbuff(buff, p, L);  /* heap variable */
  addbuff(buff, p, &h);  /* local variable */
  addbuff(buff, p, luaO_nilobject);  /* global variable */
  addbuff(buff, p, &lua_newstate);  /* public function */
  lua_assert(p == sizeof(buff));
  return luaS_hash(buff, p, h);
}


/*
** set GCdebt to a new value keeping the value (totalbytes + GCdebt)
** invariant (and avoiding underflows in 'totalbytes')
*/
void luaE_setdebt (global_State *g, l_mem debt) {
  l_mem tb = gettotalbytes(g);
  lua_assert(tb > 0);
  if (debt < tb - MAX_LMEM)
    debt = tb - MAX_LMEM;  /* will make 'totalbytes == MAX_LMEM' */
  g->totalbytes = tb - debt;
  g->GCdebt = debt;
}

/*扩展CallInfo CI具体执行部分并不直接调用这个API 而是使用另一个宏 next_ci(L) */
CallInfo *luaE_extendCI (lua_State *L) {
  CallInfo *ci = luaM_new(L, CallInfo);
  lua_assert(L->ci->next == NULL);
  L->ci->next = ci;
  ci->previous = L->ci;
  ci->next = NULL;
  L->nci++;
  return ci;
}


/*
** free all CallInfo structures not in use by a thread
*  在GC的时候只需要调用luaE_freeCI就可以释放过长的链表CallInfo
*/
void luaE_freeCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next = ci->next;
  ci->next = NULL;
  while ((ci = next) != NULL) {
    next = ci->next;
    luaM_free(L, ci);
    L->nci--;
  }
}


/*
** free half of the CallInfo structures not in use by a thread
*/
void luaE_shrinkCI (lua_State *L) {
  CallInfo *ci = L->ci;
  CallInfo *next2;  /* next's next */
  /* while there are two nexts */
  while (ci->next != NULL && (next2 = ci->next->next) != NULL) {
    luaM_free(L, ci->next);  /* free next */
    L->nci--;
    ci->next = next2;  /* remove 'next' from the list */
    next2->previous = ci;
    ci = next2;  /* keep next's next */
  }
}


/**
 * 栈初始化（栈分为数据栈 + 调用栈）
 */
static void stack_init(lua_State *L1, lua_State *L) {
	int i; CallInfo *ci;
	/* initialize stack array */
	L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue); //默认40个 Lua 的数据栈就是一个TValue的数组（函数一开始就创建一个TValue 数组）
	L1->stacksize = BASIC_STACK_SIZE;
	for (i = 0; i < BASIC_STACK_SIZE; i++)
		setnilvalue(L1->stack + i);  /* erase new stack */
	L1->top = L1->stack; // stack 指向它的首地址（栈底） top 域 （栈顶）指向 stack
	L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;  //栈顶默认到35，空出5个做buf？

	/* initialize first ci */
	ci = &L1->base_ci; // 从 L 状态机中的 base_ci 中获取第一个ci的地址
	ci->next = ci->previous = NULL;
	ci->callstatus = 0;
	ci->func = L1->top;  //指向当前栈顶 然后 ci->func 指向 L1->top
	setnilvalue(L1->top++);  /* 'function' entry for this 'ci' L1->top++ 将这个 function 压栈 */
	ci->top = L1->top + LUA_MINSTACK;  //指向到L1->top +20的位置  计算 ci->top 在 L1->top 的基础上扩展 LUA_MINSTACK (20)个 TValue
	L1->ci = ci;
}

/**
 * 释放栈结构内存
 */
static void freestack(lua_State *L) {
	if (L->stack == NULL)
		return;  /* stack not completely built yet */
	L->ci = &L->base_ci;  /* free the entire 'ci' list */
	luaE_freeCI(L);
	lua_assert(L->nci == 0);
	luaM_freearray(L, L->stack, L->stacksize);  /* free stack array */
}


/*
** Create registry table and its predefined values
** 创建一个注册表，并且定义默认值
*/
static void init_registry(lua_State *L, global_State *g) {
	TValue temp;
	/* create registry */
	Table *registry = luaH_new(L); //创建一个Table表
	sethvalue(L, &g->l_registry, registry);
	luaH_resize(L, registry, LUA_RIDX_LAST, 0);
	/* registry[LUA_RIDX_MAINTHREAD] = L */
	setthvalue(L, &temp, L);  /* temp = L */
	luaH_setint(L, registry, LUA_RIDX_MAINTHREAD, &temp);
	/* registry[LUA_RIDX_GLOBALS] = table of globals */
	sethvalue(L, &temp, luaH_new(L));  /* temp = new table (global table) */
	luaH_setint(L, registry, LUA_RIDX_GLOBALS, &temp);
}



/*
** open parts of the state that may cause memory-allocation errors.
** ('g->version' != NULL flags that the state was completely build)
** 启动Lua程序
** 初始化栈、初始化字符串结构、初始化原方法、初始化保留字实现、初始化注册表
*/
static void f_luaopen(lua_State *L, void *ud) {
	global_State *g = G(L);
	UNUSED(ud);
	stack_init(L, L);  /* init stack */
	init_registry(L, g); //初始化注册表
	luaS_init(L); //字符串结构初始化
	luaT_init(L); //元方法初始化
	luaX_init(L); //保留字实现
	g->gcrunning = 1;  /* allow gc */
	g->version = lua_version(NULL);
	luai_userstateopen(L);
}



/*
** preinitialize a thread with consistent values without allocating
** any memory (to avoid errors)
*/
static void preinit_thread (lua_State *L, global_State *g) {
  G(L) = g;
  L->stack = NULL;
  L->ci = NULL;
  L->nci = 0;
  L->stacksize = 0;
  L->twups = L;  /* thread has no upvalues */
  L->errorJmp = NULL;
  L->nCcalls = 0;
  L->hook = NULL;
  L->hookmask = 0;
  L->basehookcount = 0;
  L->allowhook = 1;
  resethookcount(L);
  L->openupval = NULL;
  L->nny = 1;
  L->status = LUA_OK;
  L->errfunc = 0;
}


/**
 * 释放Lua栈结构
 */
static void close_state(lua_State *L) {
	global_State *g = G(L);
	luaF_close(L, L->stack);  /* close all upvalues for this thread 释放Lua栈的upvalues */
	luaC_freeallobjects(L);  /* collect all objects 释放全部对象 */
	if (g->version)  /* closing a fully built state? */
		luai_userstateclose(L);
	luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
	freestack(L);
	lua_assert(gettotalbytes(g) == sizeof(LG));
	(*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}



/**
 * 创建一个新的线程栈
 * LUA在main函数中，调用luaL_newstate()方法，创建了主线程（既：lua_State *L）
 * 主要用于实现Lua的协程实现（Lua没有多线程实现）
 * 1:把数据线和调用栈合起来就构成了Lua中的线程。
 * 2:在同一个Lua虚拟机中的不同线程因为共享了global_State而很难做到真正意义上的并发。 
 * 3:它也绝非操作系统意义上的线程,但在行为上很相似。用户可以resume一个线程,线程可以被yield打断。
 * 4:Lua的执行过程就是围绕线程进行的。
 * 5:我们从lua_newthread阅读起,可以更好的理解它的数据结构。
 * 6:这里我们能发现,内存中的线程结构并非lua_State,而是一个叫LX的东西。
 */
LUA_API lua_State *lua_newthread(lua_State *L) {
	global_State *g = G(L);
	lua_State *L1;
	lua_lock(L);
	luaC_checkGC(L);
	/* create new thread */
	L1 = &cast(LX *, luaM_newobject(L, LUA_TTHREAD, sizeof(LX)))->l;
	L1->marked = luaC_white(g);
	L1->tt = LUA_TTHREAD;
	/* link it on list 'allgc' */
	L1->next = g->allgc;
	g->allgc = obj2gco(L1);
	/* anchor it on L stack */
	setthvalue(L, L->top, L1); //栈顶上 设置一个新的L1对象
	api_incr_top(L);
	preinit_thread(L1, g);
	L1->hookmask = L->hookmask;
	L1->basehookcount = L->basehookcount;
	L1->hook = L->hook;
	resethookcount(L1);
	/* initialize L1 extra space */
	memcpy(lua_getextraspace(L1), lua_getextraspace(g->mainthread),
		LUA_EXTRASPACE);
	luai_userstatethread(L, L1);
	stack_init(L1, L);  /* init stack */
	lua_unlock(L);
	return L1;
}



void luaE_freethread (lua_State *L, lua_State *L1) {
  LX *l = fromstate(L1);
  luaF_close(L1, L1->stack);  /* close all upvalues for this thread */
  lua_assert(L1->openupval == NULL);
  luai_userstatefree(L, L1);
  freestack(L1);
  luaM_free(L, l);
}



/**
 * 分配lua_State和global_State
 * 说明：global_State全局表会挂载在lua_State结构上，此方法分配的是主线程栈。如果实现协程，则通过lua_newthread分配新的lua_State栈
 * 通过LG结构方式，每个线程会独立维护自己的线程栈和函数栈
 * 对外通过lua_State结构暴露给用户，而global_State挂载在lua_State结构上
 * 主要管理管理全局数据，全局字符串表、内存管理函数、 GC 把所有对象串联起来的信息、内存等
 * global_State：全局状态机
 * lua_State：主线程栈结构
 */
LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud) {
	int i;
	lua_State *L;
	global_State *g;
	/* 分配一块lua_State结构的内容块 */
	LG *l = cast(LG *, (*f)(ud, NULL, LUA_TTHREAD, sizeof(LG)));
	if (l == NULL) return NULL;
	L = &l->l.l;
	g = &l->g;
	L->next = NULL;
	L->tt = LUA_TTHREAD;
	g->currentwhite = bitmask(WHITE0BIT);
	L->marked = luaC_white(g);
	/* 初始化一个线程的栈结构数据 */
	preinit_thread(L, g);
	g->frealloc = f;
	g->ud = ud;
	g->mainthread = L;
	g->seed = makeseed(L);
	g->gcrunning = 0;  /* no GC while building state */
	g->GCestimate = 0;
	g->strt.size = g->strt.nuse = 0;
	g->strt.hash = NULL;
	setnilvalue(&g->l_registry);
	g->panic = NULL;
	g->version = NULL;
	g->gcstate = GCSpause;
	g->gckind = KGC_NORMAL;
	g->allgc = g->finobj = g->tobefnz = g->fixedgc = NULL;
	g->sweepgc = NULL;
	g->gray = g->grayagain = NULL;
	g->weak = g->ephemeron = g->allweak = NULL;
	g->twups = NULL;
	g->totalbytes = sizeof(LG);
	g->GCdebt = 0;
	g->gcfinnum = 0;
	g->gcpause = LUAI_GCPAUSE;
	g->gcstepmul = LUAI_GCMUL;
	for (i = 0; i < LUA_NUMTAGS; i++) g->mt[i] = NULL;
	if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) { //f_luaopen函数中调用了 stack_init 函数
	  /* memory allocation error: free partial state */
		close_state(L);
		L = NULL;
	}
	return L;
}


/*
 * 关闭Lua栈
 */
LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  close_state(L);
}


