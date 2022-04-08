/*
** $Id: lfunc.c,v 2.45.1.1 2017/04/19 17:39:34 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#define lfunc_c
#define LUA_CORE

#include "lprefix.h"


#include <stddef.h>

#include "lua.h"

#include "lfunc.h"
#include "lgc.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"


/* C �հ��е� upvalue һ���� closed ״̬�� Lua Ҳ�Ͳ���Ҫ�õ�����UpVal �������������ǡ� C �հ���upvalue ��ֱ�Ӵ���� CClosure �ṹ���еġ� */
CClosure *luaF_newCclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TCCL, sizeCclosure(n));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(n);
  return c;
}

/*
*  ֻ���� Proto ��û�г�ʼ��upvalue����������Ϊ ���� LClosure �����ֿ��ܵ�;��
*  1. LClosure һ���� Lua VM ���й����б���̬�Ĺ����������ʱ��LClosure ��Ҫ���� upvalue ���ڵ�ǰ������ջ�ϡ�����luaF_findupval �������԰�����ջ�ϵ� TValue ֵת��Ϊ upvalue��
*  2. Lua ����ʱ������һ��Lua ���룬Ҳ������ LClosure ��ÿ��Lua ���붼�ᱻ�����һ�� ��������ԭ��Proto����Lua ��Public API �ǲ����� Proto ����ģ�ֻ�� Lua VM �����ѣ������ǽ���� Proto ��װ��һ�� LClosure ���󷵻ء� �����Դ������صĻ������������û����������� upvalue�� �����κ�һ������鶼������һ�� upvalue ��_ENV ��, �����ÿһ��Lua �����ɼ������Ա�����Lua �����ĵ�һ�� upvalue �У���
* 
*/
LClosure *luaF_newLclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TLCL, sizeLclosure(n));
  LClosure *c = gco2lcl(o);
  c->p = NULL;
  c->nupvalues = cast_byte(n);
  while (n--) c->upvals[n] = NULL;
  return c;
}

/*
** fill a closure with new closed upvalues
*  ��Щupvalue ��������������ջ�������� luaF_initupvals ��������Щ upvalue ��䵽��� LClosure����ʱ����Щ UpVal һ���� closed ״̬��
*/
void luaF_initupvals (lua_State *L, LClosure *cl) {
  int i;
  for (i = 0; i < cl->nupvalues; i++) {
    UpVal *uv = luaM_new(L, UpVal);
    uv->refcount = 1;
    uv->v = &uv->u.value;  /* make it closed */
    setnilvalue(uv->v);
    cl->upvals[i] = uv;
  }
}

/* luaF_findupval �����߼������ڵ�ǰ��openupval ������Ѱ���Ƿ��Ѿ�ת����������Ѿ��������á���֮�͹���һ���µ� UpVal ���󣬲��������ӵ� openupval �����С� */
UpVal *luaF_findupval (lua_State *L, StkId level) {
  UpVal **pp = &L->openupval;
  UpVal *p;
  UpVal *uv;
  lua_assert(isintwups(L) || L->openupval == NULL);
  while (*pp != NULL && (p = *pp)->v >= level) {
    lua_assert(upisopen(p));
    if (p->v == level)  /* found a corresponding upvalue? */
      return p;  /* return it */
    pp = &p->u.open.next;
  }
  /* not found: create a new upvalue */
  uv = luaM_new(L, UpVal);
  uv->refcount = 0;
  uv->u.open.next = *pp;  /* link it to list of open upvalues */
  uv->u.open.touched = 1;
  *pp = uv;
  uv->v = level;  /* current value lives in the stack */
  if (!isintwups(L)) {  /* thread not in list of threads with upvalues? */
    L->twups = G(L)->twups;  /* link it to the list */
    G(L)->twups = L;
  }
  return uv;
}


/*  ���뿪һ�����������������ж���ľֲ������ͱ�Ϊ���ɼ��ġ�Lua ���������ջָ�룬���ٵ���Щ����������Щջֵ����ĳЩ�հ� �� open ״̬ �� upvalue ����ʽ���ã�����Ҫ�����ǹرա�
 *  luaF_close �����߼����Ƚ���ǰ UpVal �� L->openipval �������޳�����Ȼ���ж� ��ǰUpVal ->refcount �鿴�Ƿ��б������հ� ����, ���refcount == 0 ���ͷ�UpVal �ṹ�����������������Ҫ �����ݣ�uv->v ��ʱ��������ջ�ϣ�������ջ�� copy �� UpVal �ṹ�е� ��uv->u.value���У�������� UpVal �е�ָ�� v��uv->v ����ָ��UpVal �ṹ��  uv->u.value  ���ڵ�ַ����
 */
void luaF_close (lua_State *L, StkId level) {
  UpVal *uv;
  while (L->openupval != NULL && (uv = L->openupval)->v >= level) {
    lua_assert(upisopen(uv));
    L->openupval = uv->u.open.next;  /* remove from 'open' list */
    if (uv->refcount == 0)  /* no references? */
      luaM_free(L, uv);  /* free upvalue */
    else {
      setobj(L, &uv->u.value, uv->v);  /* move value to upvalue slot */
      uv->v = &uv->u.value;  /* now current value lives here */
      luaC_upvalbarrier(L, uv);
    }
  }
}


Proto *luaF_newproto (lua_State *L) {
  GCObject *o = luaC_newobj(L, LUA_TPROTO, sizeof(Proto));
  Proto *f = gco2p(o);
  f->k = NULL;
  f->sizek = 0;
  f->p = NULL;
  f->sizep = 0;
  f->code = NULL;
  f->cache = NULL;
  f->sizecode = 0;
  f->lineinfo = NULL;
  f->sizelineinfo = 0;
  f->upvalues = NULL;
  f->sizeupvalues = 0;
  f->numparams = 0;
  f->is_vararg = 0;
  f->maxstacksize = 0;
  f->locvars = NULL;
  f->sizelocvars = 0;
  f->linedefined = 0;
  f->lastlinedefined = 0;
  f->source = NULL;
  return f;
}

/* ���ͷ�һ�� Proto�ṹʱ��˳���������ڲ����ݽṹռ�õ��ڴ档��ΪProto �ṹ�� ��¼��ÿ���ڴ��ĳߴ磬ʹ��Lua �� LuaM_xxxx API �ܾ�׼�Ĺ����ڴ档 */
void luaF_freeproto (lua_State *L, Proto *f) {
  luaM_freearray(L, f->code, f->sizecode);
  luaM_freearray(L, f->p, f->sizep);
  luaM_freearray(L, f->k, f->sizek);
  luaM_freearray(L, f->lineinfo, f->sizelineinfo);
  luaM_freearray(L, f->locvars, f->sizelocvars);
  luaM_freearray(L, f->upvalues, f->sizeupvalues);
  luaM_free(L, f);
}


/*
** Look for n-th local variable at line 'line' in function 'func'.
** Returns NULL if not found.
*/
const char *luaF_getlocalname (const Proto *f, int local_number, int pc) {
  int i;
  for (i = 0; i<f->sizelocvars && f->locvars[i].startpc <= pc; i++) {
    if (pc < f->locvars[i].endpc) {  /* is variable active? */
      local_number--;
      if (local_number == 0)
        return getstr(f->locvars[i].varname);
    }
  }
  return NULL;  /* not found */
}

