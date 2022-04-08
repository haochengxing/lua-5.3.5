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
  TValue *v;  /* points to stack or to its own value   ָ����Upvalue ������ֵ�� ָ��  �� upvalue ���رպ󣬾Ͳ�����Ҫ������ָ���ˡ���νclose ״̬ upvalue��ָ����upvalue ���õ�����ջ�ϵ����ݲ��ٴ���������ջʱ��ͨ����������ֲ������ĺ�����������ģ�, ��Ҫ�� upvalue �� lua_State ->openupval �����������޳������������õ�����ջ�ϵı���ֵ�浽����һ����ȫ���ڴ�ռ䣨UpVal �ṹ���ڣ��� */
  lu_mem refcount;  /* reference counter   ���ü��� */
  union {
    struct {  /* (when open) */
      UpVal *next;  /* linked list */
      int touched;  /* mark to avoid cycles with dead threads */
    } open;/* �������õı�����������ջ��ʱ�����ָ��ֱ��ָ��ջ�ϵĵ�ַ�����upvalue ����Ϊ���ŵġ� ������ǰ����open �� upvalue ���õ��ǵ�ǰ lua_State ->openupval ����*/
    TValue value;  /* the value (when closed) */
  } u;/* �� һ��Proto ���� open ״̬ʱ��Ҳ��������Proto�����ĺ���û�з���֮ǰ����������ֵ����ֱ��ͨ�� upval ->v ���ָ�����ã����ʱ�� open �ṹ��ѵ�ǰ�������ڵ����бհ�����������һ�����������Ժ���ҡ� ��֮ close ״̬���� ��㺯�����ص�ʱ��Proto ��Ҫ�ѱհ����� ��ֵcopy ������ upval->u.value �У�upval->v ��Ȼָ�� upval->u.value */
  /* ͨ���ж�UpVal ->v��u->value�Ƿ�������ж�UpVal����open����clsoed״̬ */
};

/* upvalue �� open �� close ״̬ ��UpVal �ṹ�� ����Ҫ��һ�����λ���� ����Ϊ��upvalue close ʱ�� UpVal �е� v ָ��һ��ָ��ṹ���ڲ��� value�� */
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
