/*
** $Id: lfunc.h,v 2.15.1.1 2017/04/19 17:39:34 roberto Exp $
** Auxiliary functions to manipulate prototypes and closures
** See Copyright Notice in lua.h
*/

#ifndef lfunc_h
#define lfunc_h


#include "lobject.h"


#define sizeCclosure(n)	(cast(int, sizeof(CClosure)) + \
                         cast(int, sizeof(TValue)*((n)-1)))

#define sizeLclosure(n)	(cast(int, sizeof(LClosure)) + \
                         cast(int, sizeof(TValue *)*((n)-1)))


/* test whether thread is in 'twups' list */
#define isintwups(L)	(L->twups != L)


/*
** maximum number of upvalues in a closure (both C and Lua). (Value
** must fit in a VM register.)
*/
#define MAXUPVAL	255


/*
** Upvalues for Lua closures
*/
struct UpVal {
  TValue *v;  /* points to stack or to its own value   指向了Upvalue 变量的值得 指针  当 upvalue 被关闭后，就不再需要这两个指针了。所谓close 状态 upvalue是指：当upvalue 引用的数据栈上的数据不再存在于数据栈时（通常是由申请局部变量的函数返回引起的）, 需要把 upvalue 从 lua_State ->openupval 开放链表中剔除，并把其引用的数据栈上的变量值存到另外一个安全的内存空间（UpVal 结构体内）。 */
  lu_mem refcount;  /* reference counter   引用计数 */
  union {
    struct {  /* (when open) */
      UpVal *next;  /* linked list */
      int touched;  /* mark to avoid cycles with dead threads */
    } open;/* 当被引用的变量还在数据栈上时，这个指针直接指向栈上的地址。这个upvalue 被称为开放的。 遍历当前所有open 的 upvalue 利用的是当前 lua_State ->openupval 链表。*/
    TValue value;  /* the value (when closed) */
  } u;/* 当 一个Proto 处于 open 状态时候（也就是这样Proto在外层的函数没有返回之前），变量的值可以直接通过 upval ->v 这个指针引用，这个时候 open 结构体把当前作用域内的所有闭包变量都链成一个链表，方便以后查找。 反之 close 状态就是 外层函数返回的时候，Proto 需要把闭包变量 的值copy 出来到 upval->u.value 中，upval->v 自然指向 upval->u.value */
  /* 通过判断UpVal ->v和u->value是否相等来判断UpVal处于open还是clsoed状态 */
};

/* upvalue 的 open 与 close 状态 在UpVal 结构中 不需要用一个标记位区分 。因为当upvalue close 时， UpVal 中的 v 指针一定指向结构体内部的 value。 */
#define upisopen(up)	((up)->v != &(up)->u.value)


LUAI_FUNC Proto *luaF_newproto (lua_State *L);
LUAI_FUNC CClosure *luaF_newCclosure (lua_State *L, int nelems);
LUAI_FUNC LClosure *luaF_newLclosure (lua_State *L, int nelems);
LUAI_FUNC void luaF_initupvals (lua_State *L, LClosure *cl);
LUAI_FUNC UpVal *luaF_findupval (lua_State *L, StkId level);
LUAI_FUNC void luaF_close (lua_State *L, StkId level);
LUAI_FUNC void luaF_freeproto (lua_State *L, Proto *f);
LUAI_FUNC const char *luaF_getlocalname (const Proto *func, int local_number,
                                         int pc);


#endif
