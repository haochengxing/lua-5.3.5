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


/* C 闭包中的 upvalue 一定是 closed 状态。 Lua 也就不需要用单独的UpVal 对象来引用它们。 C 闭包，upvalue 是直接存放在 CClosure 结构体中的。 */
CClosure *luaF_newCclosure (lua_State *L, int n) {
  GCObject *o = luaC_newobj(L, LUA_TCCL, sizeCclosure(n));
  CClosure *c = gco2ccl(o);
  c->nupvalues = cast_byte(n);
  return c;
}

/*
*  只绑定了 Proto 而没有初始化upvalue对象。这是因为 构造 LClosure 有两种可能的途径
*  1. LClosure 一般在 Lua VM 运行过程中被动态的构造出来。这时候，LClosure 需要引用 upvalue 都在当前的数据栈上。利用luaF_findupval 函数可以把数据栈上的 TValue 值转换为 upvalue。
*  2. Lua 运行时，加载一段Lua 代码，也会生成 LClosure 。每段Lua 代码都会被编译成一个 函数函数原型Proto，但Lua 的Public API 是不返回 Proto 对象的（只在 Lua VM 中消费），而是将这个 Proto 包装成一个 LClosure 对象返回。 如果从源代码加载的话，不可能有用户构建出来的 upvalue。 但是任何一个代码块都至少有一个 upvalue （_ENV 表, 必须对每一个Lua 函数可见，所以被放在Lua 代码块的第一个 upvalue 中）。
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
*  这些upvalue 并不是来至数据栈，所有用 luaF_initupvals 函数将这些 upvalue 填充到这个 LClosure，这时候这些 UpVal 一定是 closed 状态。
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

/* luaF_findupval 函数逻辑：先在当前的openupval 链表中寻找是否已经转换过，如果已经存在则复用。反之就构造一个新的 UpVal 对象，并将它链接到 openupval 链表中。 */
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


/*  当离开一个代码块后，这个代码块中定义的局部变量就变为不可见的。Lua 会调整数据栈指针，销毁掉这些变量。若这些栈值还被某些闭包 以 open 状态 的 upvalue 的形式引用，就需要把它们关闭。
 *  luaF_close 函数逻辑：先将当前 UpVal 从 L->openipval 链表中剔除掉，然后判断 当前UpVal ->refcount 查看是否还有被其他闭包 引用, 如果refcount == 0 则释放UpVal 结构；如果还有引用则需要 把数据（uv->v 这时候在数据栈上）从数据栈上 copy 到 UpVal 结构中的 （uv->u.value）中，最后修正 UpVal 中的指针 v（uv->v 现在指向UpVal 结构中  uv->u.value  所在地址）。
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

/* 在释放一个 Proto结构时，顺带回收了内部数据结构占用的内存。因为Proto 结构中 记录了每个内存块的尺寸，使用Lua 的 LuaM_xxxx API 能精准的管理内存。 */
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

