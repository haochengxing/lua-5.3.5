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
* LX�Ķ���
* 1:��lua_State֮ǰ�����˴�СΪLUAI_EXTRASPACE�ֽڵĿռ䡣
* 2:����ⲿ�û�������ָ����L������LX,��L��ռ�ݵ��ڴ���ǰ��ȴ�����������ġ�
* 3:����һ����Ȥ�ļ��ɡ��û��������õ�Lָ�����ǰ�ƶ�ָ��,ȡ��һЩLUAT_EXTRASPACE�ж�������ݡ�
* 4:����Щ���ݷ���ǰ�������lua_State�ṹ�ĺ�����������û���¶�ṹ�Ĵ�С��
* 5:����,LUAI_EXTRASPACE��ͨ���������õ�,Ĭ��Ϊ0;
* 6:LUAI EXTRASPACE,��ANNARRI (luai_userstateopen(L). ...)
* 7:��L����һЩ�û��Զ�����Ϣ��׷�����ܵĻ����������塣������ΪLua��д��Cģ����,ֱ��ƫ��Lָ������ȡһЩ������Ϣ�����ȥ��ȡL�е�ע���Ҫ��Ч�Ķࡣ
* 8:��һ����,�ڶ��̻߳�����,����ע������ı�L��״̬,���̲߳���ȫ�ġ�
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

/*��չCallInfo CI����ִ�в��ֲ���ֱ�ӵ������API ����ʹ����һ���� next_ci(L) */
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
*  ��GC��ʱ��ֻ��Ҫ����luaE_freeCI�Ϳ����ͷŹ���������CallInfo
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
 * ջ��ʼ����ջ��Ϊ����ջ + ����ջ��
 */
static void stack_init(lua_State *L1, lua_State *L) {
	int i; CallInfo *ci;
	/* initialize stack array */
	L1->stack = luaM_newvector(L, BASIC_STACK_SIZE, TValue); //Ĭ��40�� Lua ������ջ����һ��TValue�����飨����һ��ʼ�ʹ���һ��TValue ���飩
	L1->stacksize = BASIC_STACK_SIZE;
	for (i = 0; i < BASIC_STACK_SIZE; i++)
		setnilvalue(L1->stack + i);  /* erase new stack */
	L1->top = L1->stack; // stack ָ�������׵�ַ��ջ�ף� top �� ��ջ����ָ�� stack
	L1->stack_last = L1->stack + L1->stacksize - EXTRA_STACK;  //ջ��Ĭ�ϵ�35���ճ�5����buf��

	/* initialize first ci */
	ci = &L1->base_ci; // �� L ״̬���е� base_ci �л�ȡ��һ��ci�ĵ�ַ
	ci->next = ci->previous = NULL;
	ci->callstatus = 0;
	ci->func = L1->top;  //ָ��ǰջ�� Ȼ�� ci->func ָ�� L1->top
	setnilvalue(L1->top++);  /* 'function' entry for this 'ci' L1->top++ ����� function ѹջ */
	ci->top = L1->top + LUA_MINSTACK;  //ָ��L1->top +20��λ��  ���� ci->top �� L1->top �Ļ�������չ LUA_MINSTACK (20)�� TValue
	L1->ci = ci;
}

/**
 * �ͷ�ջ�ṹ�ڴ�
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
** ����һ��ע������Ҷ���Ĭ��ֵ
*/
static void init_registry(lua_State *L, global_State *g) {
	TValue temp;
	/* create registry */
	Table *registry = luaH_new(L); //����һ��Table��
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
** ����Lua����
** ��ʼ��ջ����ʼ���ַ����ṹ����ʼ��ԭ��������ʼ��������ʵ�֡���ʼ��ע���
*/
static void f_luaopen(lua_State *L, void *ud) {
	global_State *g = G(L);
	UNUSED(ud);
	stack_init(L, L);  /* init stack */
	init_registry(L, g); //��ʼ��ע���
	luaS_init(L); //�ַ����ṹ��ʼ��
	luaT_init(L); //Ԫ������ʼ��
	luaX_init(L); //������ʵ��
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
 * �ͷ�Luaջ�ṹ
 */
static void close_state(lua_State *L) {
	global_State *g = G(L);
	luaF_close(L, L->stack);  /* close all upvalues for this thread �ͷ�Luaջ��upvalues */
	luaC_freeallobjects(L);  /* collect all objects �ͷ�ȫ������ */
	if (g->version)  /* closing a fully built state? */
		luai_userstateclose(L);
	luaM_freearray(L, G(L)->strt.hash, G(L)->strt.size);
	freestack(L);
	lua_assert(gettotalbytes(g) == sizeof(LG));
	(*g->frealloc)(g->ud, fromstate(L), sizeof(LG), 0);  /* free main block */
}



/**
 * ����һ���µ��߳�ջ
 * LUA��main�����У�����luaL_newstate()���������������̣߳��ȣ�lua_State *L��
 * ��Ҫ����ʵ��Lua��Э��ʵ�֣�Luaû�ж��߳�ʵ�֣�
 * 1:�������ߺ͵���ջ�������͹�����Lua�е��̡߳�
 * 2:��ͬһ��Lua������еĲ�ͬ�߳���Ϊ������global_State�������������������ϵĲ����� 
 * 3:��Ҳ���ǲ���ϵͳ�����ϵ��߳�,������Ϊ�Ϻ����ơ��û�����resumeһ���߳�,�߳̿��Ա�yield��ϡ�
 * 4:Lua��ִ�й��̾���Χ���߳̽��еġ�
 * 5:���Ǵ�lua_newthread�Ķ���,���Ը��õ�����������ݽṹ��
 * 6:���������ܷ���,�ڴ��е��߳̽ṹ����lua_State,����һ����LX�Ķ�����
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
	setthvalue(L, L->top, L1); //ջ���� ����һ���µ�L1����
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
 * ����lua_State��global_State
 * ˵����global_Stateȫ�ֱ�������lua_State�ṹ�ϣ��˷�������������߳�ջ�����ʵ��Э�̣���ͨ��lua_newthread�����µ�lua_Stateջ
 * ͨ��LG�ṹ��ʽ��ÿ���̻߳����ά���Լ����߳�ջ�ͺ���ջ
 * ����ͨ��lua_State�ṹ��¶���û�����global_State������lua_State�ṹ��
 * ��Ҫ�������ȫ�����ݣ�ȫ���ַ������ڴ�������� GC �����ж�������������Ϣ���ڴ��
 * global_State��ȫ��״̬��
 * lua_State�����߳�ջ�ṹ
 */
LUA_API lua_State *lua_newstate(lua_Alloc f, void *ud) {
	int i;
	lua_State *L;
	global_State *g;
	/* ����һ��lua_State�ṹ�����ݿ� */
	LG *l = cast(LG *, (*f)(ud, NULL, LUA_TTHREAD, sizeof(LG)));
	if (l == NULL) return NULL;
	L = &l->l.l;
	g = &l->g;
	L->next = NULL;
	L->tt = LUA_TTHREAD;
	g->currentwhite = bitmask(WHITE0BIT);
	L->marked = luaC_white(g);
	/* ��ʼ��һ���̵߳�ջ�ṹ���� */
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
	if (luaD_rawrunprotected(L, f_luaopen, NULL) != LUA_OK) { //f_luaopen�����е����� stack_init ����
	  /* memory allocation error: free partial state */
		close_state(L);
		L = NULL;
	}
	return L;
}


/*
 * �ر�Luaջ
 */
LUA_API void lua_close (lua_State *L) {
  L = G(L)->mainthread;  /* only the main thread can be closed */
  lua_lock(L);
  close_state(L);
}


