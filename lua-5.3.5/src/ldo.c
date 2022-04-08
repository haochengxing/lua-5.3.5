/*
** $Id: ldo.c,v 2.157.1.1 2017/04/19 17:20:42 roberto Exp $
** Stack and Call structure of Lua
** See Copyright Notice in lua.h
*/

#define ldo_c
#define LUA_CORE

#include "lprefix.h"


#include <setjmp.h>
#include <stdlib.h>
#include <string.h>

#include "lua.h"

#include "lapi.h"
#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lparser.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lundump.h"
#include "lvm.h"
#include "lzio.h"



#define errorstatus(s)	((s) > LUA_YIELD)


/*
** {======================================================
** Error-recovery functions
** =======================================================
*/

/*
** LUAI_THROW/LUAI_TRY define how Lua does exception handling. By
** default, Lua handles errors with exceptions when compiling as
** C++ code, with _longjmp/_setjmp when asked to use them, and with
** longjmp/setjmp otherwise.
*/
#if !defined(LUAI_THROW)				/* { */

#if defined(__cplusplus) && !defined(LUA_USE_LONGJMP)	/* { */

/* C++ exceptions */
#define LUAI_THROW(L,c)		throw(c)
#define LUAI_TRY(L,c,a) \
	try { a } catch(...) { if ((c)->status == 0) (c)->status = -1; }
#define luai_jmpbuf		int  /* dummy variable */

#elif defined(LUA_USE_POSIX)				/* }{ */

/* in POSIX, try _longjmp/_setjmp (more efficient) */
#define LUAI_THROW(L,c)		_longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (_setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#else							/* }{ */

/* ISO C handling with long jumps */
#define LUAI_THROW(L,c)		longjmp((c)->b, 1)
#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
#define luai_jmpbuf		jmp_buf

#endif							/* } */

#endif							/* } */



/* chain list of long jump buffers */
struct lua_longjmp {
  struct lua_longjmp *previous;
  luai_jmpbuf b;
  volatile int status;  /* error code */
};


static void seterrorobj (lua_State *L, int errcode, StkId oldtop) {
  switch (errcode) {
    case LUA_ERRMEM: {  /* memory error? */
      setsvalue2s(L, oldtop, G(L)->memerrmsg); /* reuse preregistered msg. */
      break;
    }
    case LUA_ERRERR: {
      setsvalue2s(L, oldtop, luaS_newliteral(L, "error in error handling"));
      break;
    }
    default: {
      setobjs2s(L, oldtop, L->top - 1);  /* error message on current top */
      break;
    }
  }
  L->top = oldtop + 1;
}


/**
 * 抛出异常
 * 其中L->errorJmp->status 状态值会设置为异常状态值
 * LUAI_THROW longjmp((c)->b, 1)
 * 通过longjmp跳转到 setjmp点
 */
l_noret luaD_throw(lua_State *L, int errcode) {
	if (L->errorJmp) {  /* thread has an error handler? */
		L->errorJmp->status = errcode;  /* set status */
		LUAI_THROW(L, L->errorJmp);  /* jump to it */
	}
	else {  /* 没有error句柄，则直接中断处理 thread has no error handler */
		global_State *g = G(L);
		L->status = cast_byte(errcode);  /* L->status=LUA_YIELD  mark it as dead */
		if (g->mainthread->errorJmp) {  /* main thread has a handler? */
			setobjs2s(L, g->mainthread->top++, L->top - 1);  /* copy error obj. */
			luaD_throw(g->mainthread, errcode);  /* re-throw in main thread */
		}
		else {  /* no handler at all; abort */
			if (g->panic) {  /* panic function? */
				seterrorobj(L, errcode, L->top);  /* assume EXTRA_STACK */
				if (L->ci->top < L->top)
					L->ci->top = L->top;  /* pushing msg. can break this invariant */
				lua_unlock(L);
				g->panic(L);  /* call panic function (last chance to jump out) */
			}
			abort();
		}
	}
}


/**
 * 异常保护方法
 * 通过回调Pfunc f，并用setjmp和longjpm方式，实现代码的中断并回到setjmp处
 *
 *  #define LUAI_THROW(L,c)		longjmp((c)->b, 1)
 *	#define LUAI_TRY(L,c,a)		if (setjmp((c)->b) == 0) { a }
 */
int luaD_rawrunprotected(lua_State *L, Pfunc f, void *ud) {
	unsigned short oldnCcalls = L->nCcalls;
	struct lua_longjmp lj;
	lj.status = LUA_OK;
	lj.previous = L->errorJmp;  /* chain new error handler */
	L->errorJmp = &lj;
	/* if (setjmp((c)->b) == 0) { a } */
	/* 当函数内部调用处理，遇到异常情况下，*/
	LUAI_TRY(L, &lj,
		(*f)(L, ud);
	);
	L->errorJmp = lj.previous;  /* restore old error handler */
	L->nCcalls = oldnCcalls;
	return lj.status;
}


/* }====================================================== */


/*
** {==================================================================
** Stack reallocation
** ===================================================================
*/
/**
 * 拷贝到新的栈结构上
 * 1:有些外部结构对数据的引用需要修正为正确的新地址。
 * 2:这些需要修正的位置包括upvalue以及执行栈对数据线的引用
 * 由于Lua 的数据栈是可以扩展的，当数据栈内存空间扩展时，其内存地址会发生变化。这个时候需要 修正 UpVal 结构中的指针。 correctstack 函数处理修正的操作。
 */
static void correctstack (lua_State *L, TValue *oldstack) {
  CallInfo *ci;
  UpVal *up;
  L->top = (L->top - oldstack) + L->stack;
  for (up = L->openupval; up != NULL; up = up->u.open.next)
    up->v = (up->v - oldstack) + L->stack;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    ci->top = (ci->top - oldstack) + L->stack;
    ci->func = (ci->func - oldstack) + L->stack;
    if (isLua(ci))
      ci->u.l.base = (ci->u.l.base - oldstack) + L->stack;
  }
}


/* some space for error handling */
#define ERRORSTACKSIZE	(LUAI_MAXSTACK + 200)

/**
 * 重新分配一块statck内容，并且进行拷贝
 */
void luaD_reallocstack (lua_State *L, int newsize) {
  TValue *oldstack = L->stack;
  int lim = L->stacksize;
  lua_assert(newsize <= LUAI_MAXSTACK || newsize == ERRORSTACKSIZE);
  lua_assert(L->stack_last - L->stack == L->stacksize - EXTRA_STACK);
  luaM_reallocvector(L, L->stack, L->stacksize, newsize, TValue);
  for (; lim < newsize; lim++)
    setnilvalue(L->stack + lim); /* erase new segment */
  L->stacksize = newsize;
  L->stack_last = L->stack + newsize - EXTRA_STACK;
  correctstack(L, oldstack);
}

/**
 * 对lua_State进行扩容
 * 数据栈的空间扩展
 * 1:数据线扩展的过程,伴随着数据拷贝。这些数据都是可以直接值复制的, 所以不需要在扩展之后修正其中的指针。
 * 2:但,有些外部结构对数据线的引用需要修正为正确的新地址。这些需要修正的位置包括 upvalue以及执行栈对数据线的引用.
 * 3:这个过程由correctstack函数实现
 */
void luaD_growstack (lua_State *L, int n) {
  int size = L->stacksize;
  if (size > LUAI_MAXSTACK)  /* error after extra size? */
    luaD_throw(L, LUA_ERRERR);
  else {
    int needed = cast_int(L->top - L->stack) + n + EXTRA_STACK;
    int newsize = 2 * size;
    if (newsize > LUAI_MAXSTACK) newsize = LUAI_MAXSTACK;
    if (newsize < needed) newsize = needed;
    if (newsize > LUAI_MAXSTACK) {  /* stack overflow? */
      luaD_reallocstack(L, ERRORSTACKSIZE);
      luaG_runerror(L, "stack overflow");
    }
    else
      luaD_reallocstack(L, newsize);
  }
}


static int stackinuse (lua_State *L) {
  CallInfo *ci;
  StkId lim = L->top;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {
    if (lim < ci->top) lim = ci->top;
  }
  lua_assert(lim <= L->stack_last);
  return cast_int(lim - L->stack) + 1;  /* part of stack in use */
}


void luaD_shrinkstack (lua_State *L) {
  int inuse = stackinuse(L);
  int goodsize = inuse + (inuse / 8) + 2*EXTRA_STACK;
  if (goodsize > LUAI_MAXSTACK)
    goodsize = LUAI_MAXSTACK;  /* respect stack limit */
  if (L->stacksize > LUAI_MAXSTACK)  /* had been handling stack overflow? */
    luaE_freeCI(L);  /* free all CIs (list grew because of an error) */
  else
    luaE_shrinkCI(L);  /* shrink list */
  /* if thread is currently not handling a stack overflow and its
     good size is smaller than current size, shrink its stack */
  if (inuse <= (LUAI_MAXSTACK - EXTRA_STACK) &&
      goodsize < L->stacksize)
    luaD_reallocstack(L, goodsize);
  else  /* don't change stack */
    condmovestack(L,{},{});  /* (change only for debugging) */
}


void luaD_inctop (lua_State *L) {
  luaD_checkstack(L, 1);
  L->top++;
}

/* }================================================================== */


/*
** Call a hook for the given event. Make sure there is a hook to be
** called. (Both 'L->hook' and 'L->hookmask', which triggers this
** function, can be changed asynchronously by signals.)
*/
void luaD_hook (lua_State *L, int event, int line) {
  lua_Hook hook = L->hook;
  if (hook && L->allowhook) {  /* make sure there is a hook */
    CallInfo *ci = L->ci;
    ptrdiff_t top = savestack(L, L->top);
    ptrdiff_t ci_top = savestack(L, ci->top);
    lua_Debug ar;
    ar.event = event;
    ar.currentline = line;
    ar.i_ci = ci;
    luaD_checkstack(L, LUA_MINSTACK);  /* ensure minimum stack size */
    ci->top = L->top + LUA_MINSTACK;
    lua_assert(ci->top <= L->stack_last);
    L->allowhook = 0;  /* cannot call hooks inside a hook */
    ci->callstatus |= CIST_HOOKED;
    lua_unlock(L);
    (*hook)(L, &ar);
    lua_lock(L);
    lua_assert(!L->allowhook);
    L->allowhook = 1;
    ci->top = restorestack(L, ci_top);
    L->top = restorestack(L, top);
    ci->callstatus &= ~CIST_HOOKED;
  }
}


static void callhook (lua_State *L, CallInfo *ci) {
  int hook = LUA_HOOKCALL;
  ci->u.l.savedpc++;  /* hooks assume 'pc' is already incremented */
  if (isLua(ci->previous) &&
      GET_OPCODE(*(ci->previous->u.l.savedpc - 1)) == OP_TAILCALL) {
    ci->callstatus |= CIST_TAIL;
    hook = LUA_HOOKTAILCALL;
  }
  luaD_hook(L, hook, -1);
  ci->u.l.savedpc--;  /* correct 'pc' */
}


static StkId adjust_varargs (lua_State *L, Proto *p, int actual) {
  int i;
  int nfixargs = p->numparams;
  StkId base, fixed;
  /* move fixed parameters to final position */
  fixed = L->top - actual;  /* first fixed argument */
  base = L->top;  /* final position of first argument */
  for (i = 0; i < nfixargs && i < actual; i++) {
    setobjs2s(L, L->top++, fixed + i);
    setnilvalue(fixed + i);  /* erase original copy (for GC) */
  }
  for (; i < nfixargs; i++)
    setnilvalue(L->top++);  /* complete missing arguments */
  return base;
}


/*
** Check whether __call metafield of 'func' is a function. If so, put
** it in stack below original 'func' so that 'luaD_precall' can call
** it. Raise an error if __call metafield is not a function.
*/
static void tryfuncTM (lua_State *L, StkId func) {
  const TValue *tm = luaT_gettmbyobj(L, func, TM_CALL);
  StkId p;
  if (!ttisfunction(tm))
    luaG_typeerror(L, func, "call");
  /* Open a hole inside the stack at 'func' */
  for (p = L->top; p > func; p--)
    setobjs2s(L, p, p-1);
  L->top++;  /* slot ensured by caller */
  setobj2s(L, func, tm);  /* tag method is the new function to be called */
}


/*
** Given 'nres' results at 'firstResult', move 'wanted' of them to 'res'.
** Handle most typical cases (zero results for commands, one result for
** expressions, multiple results for tail calls/single parameters)
** separated.
*/
static int moveresults (lua_State *L, const TValue *firstResult, StkId res,
                                      int nres, int wanted) {
  switch (wanted) {  /* handle typical cases separately */
    case 0: break;  /* nothing to move */
    case 1: {  /* one result needed */
      if (nres == 0)   /* no results? */
        firstResult = luaO_nilobject;  /* adjust with nil */
      setobjs2s(L, res, firstResult);  /* move it to proper place */
      break;
    }
    case LUA_MULTRET: {
      int i;
      for (i = 0; i < nres; i++)  /* move all results to correct place */
        setobjs2s(L, res + i, firstResult + i);
      L->top = res + nres;
      return 0;  /* wanted == LUA_MULTRET */
    }
    default: {
      int i;
      if (wanted <= nres) {  /* enough results? */
        for (i = 0; i < wanted; i++)  /* move wanted results to correct place */
          setobjs2s(L, res + i, firstResult + i);
      }
      else {  /* not enough results; use all of them plus nils */
        for (i = 0; i < nres; i++)  /* move all results to correct place */
          setobjs2s(L, res + i, firstResult + i);
        for (; i < wanted; i++)  /* complete wanted number of results */
          setnilvalue(res + i);
      }
      break;
    }
  }
  L->top = res + wanted;  /* top points after the last result */
  return 1;
}


/*
** Finishes a function call: calls hook if necessary, removes CallInfo,
** moves current number of results to proper place; returns 0 iff call
** wanted multiple (variable number of) results.
*/
int luaD_poscall (lua_State *L, CallInfo *ci, StkId firstResult, int nres) {
  StkId res;
  int wanted = ci->nresults;
  if (L->hookmask & (LUA_MASKRET | LUA_MASKLINE)) {
    if (L->hookmask & LUA_MASKRET) {
      ptrdiff_t fr = savestack(L, firstResult);  /* hook may change stack */
      luaD_hook(L, LUA_HOOKRET, -1);
      firstResult = restorestack(L, fr);
    }
    L->oldpc = ci->previous->u.l.savedpc;  /* 'oldpc' for caller function */
  }
  res = ci->func;  /* res == final position of 1st result */
  L->ci = ci->previous;  /* back to caller */
  /* move results to proper place */
  return moveresults(L, firstResult, res, nres, wanted);
}



#define next_ci(L) (L->ci = (L->ci->next ? L->ci->next : luaE_extendCI(L)))


/* macro to check stack size, preserving 'p' */
#define checkstackp(L,n,p)  \
  luaD_checkstackaux(L, n, \
    ptrdiff_t t__ = savestack(L, p);  /* save 'p' */ \
    luaC_checkGC(L),  /* stack grow uses memory */ \
    p = restorestack(L, t__))  /* 'pos' part: restore 'p' */


/*
** Prepares a function call: checks the stack, creates a new CallInfo
** entry, fills in the relevant information, calls hook if needed.
** If function is a C function, does the call, too. (Otherwise, leave
** the execution ('luaV_execute') to the caller, to allow stackless
** calls.) Returns true iff function has been executed (C function).
** 预处理一个方法call
** 1. 检查栈信息
** 2. 创建一个新的CallInfo 容器
** 3. 填充相关的信息
** 4. 如果需要，回调钩子函数
*/
int luaD_precall(lua_State *L, StkId func, int nresults) {
	lua_CFunction f;
	CallInfo *ci;
	switch (ttype(func)) {
	case LUA_TCCL:  /* C语言闭包 C closure */
		f = clCvalue(func)->f;
		goto Cfunc;
	case LUA_TLCF:  /* C语言函数 light C function */
		f = fvalue(func);
	Cfunc: {
		int n;  /* number of returns */
		checkstackp(L, LUA_MINSTACK, func);  /* ensure minimum stack size */
		ci = next_ci(L);  /* 创建一个新的CallInfo栈对象 now 'enter' new function */
		ci->nresults = nresults; //返回的结果个数
		ci->func = func; //指向需要调用的函数栈
		ci->top = L->top + LUA_MINSTACK; //C语言方法最小的调用栈允许20
		lua_assert(ci->top <= L->stack_last);
		ci->callstatus = 0;
		if (L->hookmask & LUA_MASKCALL)
			luaD_hook(L, LUA_HOOKCALL, -1);
		lua_unlock(L);
		n = (*f)(L);  /* 直接调用C语言闭包函数 do the actual call */
		lua_lock(L);
		api_checknelems(L, n);
		luaD_poscall(L, ci, L->top - n, n); //调整堆栈
		return 1; //返回1 C语言本身函数
		}
	case LUA_TLCL: {  /* Lua方法 Lua function: prepare its call */
		StkId base;
		Proto *p = clLvalue(func)->p;
		int n = cast_int(L->top - func) - 1;  /* number of real arguments */
		int fsize = p->maxstacksize;  /* frame size */
		checkstackp(L, fsize, func);
		if (p->is_vararg)
			base = adjust_varargs(L, p, n);
		else {  /* non vararg function */
			for (; n < p->numparams; n++)
				setnilvalue(L->top++);  /* complete missing arguments */
			base = func + 1;
		}
		ci = next_ci(L);  /* now 'enter' new function */
		ci->nresults = nresults;
		ci->func = func;
		ci->u.l.base = base;
		L->top = ci->top = base + fsize;
		lua_assert(ci->top <= L->stack_last);
		ci->u.l.savedpc = p->code;  /* starting point */
		ci->callstatus = CIST_LUA;
		if (L->hookmask & LUA_MASKCALL)
			callhook(L, ci);
		return 0; //返回0，Lua函数
	}
	default: {  /* not a function */
		checkstackp(L, 1, func);  /* ensure space for metamethod */
		tryfuncTM(L, func);  /* try to get '__call' metamethod */
		return luaD_precall(L, func, nresults);  /* now it must be a function */
	}
	}
}



/*
** Check appropriate error for stack overflow ("regular" overflow or
** overflow while handling stack overflow). If 'nCalls' is larger than
** LUAI_MAXCCALLS (which means it is handling a "regular" overflow) but
** smaller than 9/8 of LUAI_MAXCCALLS, does not report an error (to
** allow overflow handling to work)
*/
static void stackerror (lua_State *L) {
  if (L->nCcalls == LUAI_MAXCCALLS)
    luaG_runerror(L, "C stack overflow");
  else if (L->nCcalls >= (LUAI_MAXCCALLS + (LUAI_MAXCCALLS>>3)))
    luaD_throw(L, LUA_ERRERR);  /* error while handing stack error */
}


/*
** Call a function (C or Lua). The function to be called is at *func.
** The arguments are on the stack, right after the function.
** When returns, all the results are on the stack, starting at the original
** function position.
** 真正执行一个C语言方法 or 一个Lua方法
*/
void luaD_call (lua_State *L, StkId func, int nResults) {
  if (++L->nCcalls >= LUAI_MAXCCALLS)
    stackerror(L);
  if (!luaD_precall(L, func, nResults))  /* is a Lua function? */
    luaV_execute(L);  /* call it Lua方法，则执行字节码方式*/
  L->nCcalls--;
}


/*
** Similar to 'luaD_call', but does not allow yields during the call
** 不允许挂起
*/
void luaD_callnoyield (lua_State *L, StkId func, int nResults) {
  L->nny++;
  luaD_call(L, func, nResults);
  L->nny--;
}


/*
** Completes the execution of an interrupted C function, calling its
** continuation function.
*/
static void finishCcall (lua_State *L, int status) {
  CallInfo *ci = L->ci;
  int n;
  /* must have a continuation and must be able to call it */
  lua_assert(ci->u.c.k != NULL && L->nny == 0);
  /* error status can only happen in a protected call */
  lua_assert((ci->callstatus & CIST_YPCALL) || status == LUA_YIELD);
  if (ci->callstatus & CIST_YPCALL) {  /* was inside a pcall? */
    ci->callstatus &= ~CIST_YPCALL;  /* continuation is also inside it */
    L->errfunc = ci->u.c.old_errfunc;  /* with the same error function */
  }
  /* finish 'lua_callk'/'lua_pcall'; CIST_YPCALL and 'errfunc' already
     handled */
  adjustresults(L, ci->nresults);
  lua_unlock(L);
  n = (*ci->u.c.k)(L, status, ci->u.c.ctx);  /* call continuation function */
  lua_lock(L);
  api_checknelems(L, n);
  luaD_poscall(L, ci, L->top - n, n);  /* finish 'luaD_precall' */
}


/*
** Executes "full continuation" (everything in the stack) of a
** previously interrupted coroutine until the stack is empty (or another
** interruption long-jumps out of the loop). If the coroutine is
** recovering from an error, 'ud' points to the error status, which must
** be passed to the first continuation function (otherwise the default
** status is LUA_YIELD).
*/
static void unroll (lua_State *L, void *ud) {
  if (ud != NULL)  /* error status? */
    finishCcall(L, *(int *)ud);  /* finish 'lua_pcallk' callee */
  while (L->ci != &L->base_ci) {  /* something in the stack */
    if (!isLua(L->ci))  /* C function? */
      finishCcall(L, LUA_YIELD);  /* complete its execution */
    else {  /* Lua function */
      luaV_finishOp(L);  /* finish interrupted instruction */
      luaV_execute(L);  /* execute down to higher C 'boundary' */
    }
  }
}


/*
** Try to find a suspended protected call (a "recover point") for the
** given thread.
*/
static CallInfo *findpcall (lua_State *L) {
  CallInfo *ci;
  for (ci = L->ci; ci != NULL; ci = ci->previous) {  /* search for a pcall */
    if (ci->callstatus & CIST_YPCALL)
      return ci;
  }
  return NULL;  /* no pending pcall */
}


/*
** Recovers from an error in a coroutine. Finds a recover point (if
** there is one) and completes the execution of the interrupted
** 'luaD_pcall'. If there is no recover point, returns zero.
*/
static int recover (lua_State *L, int status) {
  StkId oldtop;
  CallInfo *ci = findpcall(L);
  if (ci == NULL) return 0;  /* no recovery point */
  /* "finish" luaD_pcall */
  oldtop = restorestack(L, ci->extra);
  luaF_close(L, oldtop);
  seterrorobj(L, status, oldtop);
  L->ci = ci;
  L->allowhook = getoah(ci->callstatus);  /* restore original 'allowhook' */
  L->nny = 0;  /* should be zero to be yieldable */
  luaD_shrinkstack(L);
  L->errfunc = ci->u.c.old_errfunc;
  return 1;  /* continue running the coroutine */
}


/*
** Signal an error in the call to 'lua_resume', not in the execution
** of the coroutine itself. (Such errors should not be handled by any
** coroutine error handler and should not kill the coroutine.)
*/
static int resume_error (lua_State *L, const char *msg, int narg) {
  L->top -= narg;  /* remove args from the stack */
  setsvalue2s(L, L->top, luaS_new(L, msg));  /* push error message */
  api_incr_top(L);
  lua_unlock(L);
  return LUA_ERRRUN;
}


/*
** Do the work for 'lua_resume' in protected mode. Most of the work
** depends on the status of the coroutine: initial state, suspended
** inside a hook, or regularly suspended (optionally with a continuation
** function), plus erroneous cases: non-suspended coroutine or dead
** coroutine.
*/
static void resume(lua_State *L, void *ud) {
	int n = *(cast(int*, ud));  /* number of arguments */
	StkId firstArg = L->top - n;  /* first argument */
	CallInfo *ci = L->ci;
	/* 如果L->status 不为中断状态（Lua中用法：coroutine.resume(co2)） */
	if (L->status == LUA_OK) {  /* starting a coroutine? */
		if (!luaD_precall(L, firstArg - 1, LUA_MULTRET))  /* Lua function? */
			luaV_execute(L);  /* call it */
	}
	else {  /* resuming from previous yield */

	  /* 如果恢复中断挂起情况 */
		lua_assert(L->status == LUA_YIELD);
		L->status = LUA_OK;  /* 调整协程栈的状态 mark that it is running (again) */
		ci->func = restorestack(L, ci->extra); //取一下?
		if (isLua(ci))  /* yielded inside a hook? */
			luaV_execute(L);  /* 继续执行Lua代码 just continue running Lua code */
		else {  /* 通用的中断部分处理 'common' yield */
			if (ci->u.c.k != NULL) {  /* does it have a continuation function? */
				lua_unlock(L);
				n = (*ci->u.c.k)(L, LUA_YIELD, ci->u.c.ctx); /* call continuation */
				lua_lock(L);
				api_checknelems(L, n);
				firstArg = L->top - n;  /* yield results come from continuation */
			}
			//中断方法yield 为一个c语言lib方法，调整整体堆栈情况
			/* 中断处理逻辑：*/
			luaD_poscall(L, ci, firstArg, n);  /* 调整堆栈finish 'luaD_precall' */
		}
		unroll(L, NULL);  /* 执行先前中断的协程 run continuation */
	}
}

/**
 * 启动一个协程程序，启动方式和lua_pcall相似，但是有3个区别
 * 1. lua_resume没有参数用于指出期望的结果数量，它总是返回被调用函数的所有结果；
 * 2. 它没有用于指定错误处理函数的参数，发生错误时不会展开栈，这就可以在发生错误后检查栈中的情况；
 * 3. 如果正在运行的函数交出（yield）了控制权，lua_resume就会返回一个特殊的代码LUA_YIELD，并将线程置于一个可以被再次恢复执行的状态。
 *
 * L->nny = 0 设置允许挂起状态，协程栈上的操作，都会走luaD_call模式，而不会走luaD_callnoyield模式
 *
 * L：协程栈
 * from：原始线程栈
 * nargs：参数个数
 */
LUA_API int lua_resume (lua_State *L, lua_State *from, int nargs) {
  int status;
  unsigned short oldnny = L->nny;  /* save "number of non-yieldable" calls */
  lua_lock(L);
  if (L->status == LUA_OK) {  /* may be starting a coroutine */
    if (L->ci != &L->base_ci)  /* not in base level? */
      return resume_error(L, "cannot resume non-suspended coroutine", nargs);
  }
  else if (L->status != LUA_YIELD)
    return resume_error(L, "cannot resume dead coroutine", nargs);
  L->nCcalls = (from) ? from->nCcalls + 1 : 1;
  if (L->nCcalls >= LUAI_MAXCCALLS)
    return resume_error(L, "C stack overflow", nargs);
  luai_userstateresume(L, nargs);
  L->nny = 0;  /* allow yields */
  api_checknelems(L, (L->status == LUA_OK) ? nargs + 1 : nargs);
  status = luaD_rawrunprotected(L, resume, &nargs); // 回调函数resume，入参L为协程栈
  if (status == -1)  /* error calling 'lua_resume'? */
    status = LUA_ERRRUN;
  else {  /* continue running after recoverable errors */
    while (errorstatus(status) && recover(L, status)) {
      /* unroll continuation */
      status = luaD_rawrunprotected(L, unroll, &status);
    }
    if (errorstatus(status)) {  /* unrecoverable error? */
      L->status = cast_byte(status);  /* mark thread as 'dead' */
      seterrorobj(L, status, L->top);  /* push error message */
      L->ci->top = L->top;
    }
    else lua_assert(status == L->status);  /* normal end or yield */
  }
  L->nny = oldnny;  /* restore 'nny' */
  L->nCcalls--;
  lua_assert(L->nCcalls == ((from) ? from->nCcalls : 0));
  lua_unlock(L);
  return status;
}


LUA_API int lua_isyieldable (lua_State *L) {
  return (L->nny == 0);
}


/**
 * 协程 - 方法中断操作
 * luaD_throw(L, LUA_YIELD); //抛出一个LUA_YIELD
 */
LUA_API int lua_yieldk(lua_State *L, int nresults, lua_KContext ctx,
	lua_KFunction k) {
	CallInfo *ci = L->ci; //获取操作栈
	luai_userstateyield(L, nresults);
	lua_lock(L);
	api_checknelems(L, nresults);
	if (L->nny > 0) { //如果L->nny>0的话，是不允许中断挂起
		if (L != G(L)->mainthread)
			luaG_runerror(L, "attempt to yield across a C-call boundary");
		else
			luaG_runerror(L, "attempt to yield from outside a coroutine");
	}
	L->status = LUA_YIELD; //中间状态
	ci->extra = savestack(L, ci->func);  /* 扩展字段上保存当前方法 save current 'func' */
	if (isLua(ci)) {  /* inside a hook? */
		api_check(L, k == NULL, "hooks cannot continue after yielding");
	}
	else {
		if ((ci->u.c.k = k) != NULL)  /* is there a continuation? */
			ci->u.c.ctx = ctx;  /* save context */
		ci->func = L->top - nresults - 1;  /* protect stack below results */
		luaD_throw(L, LUA_YIELD); //抛出一个LUA_YIELD
	}
	lua_assert(ci->callstatus & CIST_HOOKED);  /* must be inside a hook */
	lua_unlock(L);
	return 0;  /* return to 'luaD_hook' */
}


/**
 * 函数调用主方法（异常保护方式）
 * func：f_call方法
 * u：CallS 调用的方法等信息
 * old_top：函数调用前的栈顶 L->top
 * ef：错误状态
 */
int luaD_pcall(lua_State *L, Pfunc func, void *u,
	ptrdiff_t old_top, ptrdiff_t ef) {
	int status;
	CallInfo *old_ci = L->ci; //老的函数回调CallInfo
	lu_byte old_allowhooks = L->allowhook; //是否允许钩子
	unsigned short old_nny = L->nny;
	ptrdiff_t old_errfunc = L->errfunc;
	L->errfunc = ef;
	status = luaD_rawrunprotected(L, func, u); //异常保护调用主函数

	/* 处理失败，栈状态回滚？ */
	if (status != LUA_OK) {  /* an error occurred? */
		StkId oldtop = restorestack(L, old_top);
		luaF_close(L, oldtop);  /* close possible pending closures */
		seterrorobj(L, status, oldtop);
		L->ci = old_ci;
		L->allowhook = old_allowhooks;
		L->nny = old_nny;
		luaD_shrinkstack(L);
	}
	L->errfunc = old_errfunc;
	return status;
}





/*
** Execute a protected parser.
*/
struct SParser {  /* data to 'f_parser' */
  ZIO *z;
  Mbuffer buff;  /* dynamic structure used by the scanner */
  Dyndata dyd;  /* dynamic structures used by the parser */
  const char *mode;
  const char *name;
};


static void checkmode (lua_State *L, const char *mode, const char *x) {
  if (mode && strchr(mode, x[0]) == NULL) {
    luaO_pushfstring(L,
       "attempt to load a %s chunk (mode is '%s')", x, mode);
    luaD_throw(L, LUA_ERRSYNTAX);
  }
}


static void f_parser (lua_State *L, void *ud) {
  LClosure *cl;
  struct SParser *p = cast(struct SParser *, ud);
  int c = zgetc(p->z);  /* read first character */
  if (c == LUA_SIGNATURE[0]) {
    checkmode(L, p->mode, "binary");
    cl = luaU_undump(L, p->z, p->name);
  }
  else {
	  //文本类型，使用luaY_parser调用
    checkmode(L, p->mode, "text");
    cl = luaY_parser(L, p->z, &p->buff, &p->dyd, p->name, c);
  }
  lua_assert(cl->nupvalues == cl->p->sizeupvalues);
  luaF_initupvals(L, cl);
}

/**
 * 文件解析函数（保护方式调用）
 * 调用：luaD_pcall方法
 */
int luaD_protectedparser (lua_State *L, ZIO *z, const char *name,
                                        const char *mode) {
  struct SParser p;
  int status;
  L->nny++;  /* cannot yield during parsing */
  p.z = z; p.name = name; p.mode = mode;
  p.dyd.actvar.arr = NULL; p.dyd.actvar.size = 0;
  p.dyd.gt.arr = NULL; p.dyd.gt.size = 0;
  p.dyd.label.arr = NULL; p.dyd.label.size = 0;
  luaZ_initbuffer(L, &p.buff);
  status = luaD_pcall(L, f_parser, &p, savestack(L, L->top), L->errfunc);
  luaZ_freebuffer(L, &p.buff);
  luaM_freearray(L, p.dyd.actvar.arr, p.dyd.actvar.size);
  luaM_freearray(L, p.dyd.gt.arr, p.dyd.gt.size);
  luaM_freearray(L, p.dyd.label.arr, p.dyd.label.size);
  L->nny--;
  return status;
}


