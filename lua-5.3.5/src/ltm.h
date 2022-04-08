/*
** $Id: ltm.h,v 2.22.1.1 2017/04/19 17:20:42 roberto Exp $
** Tag methods
** See Copyright Notice in lua.h
*/

#ifndef ltm_h
#define ltm_h


#include "lobject.h"


/*
* WARNING: if you change the order of this enumeration,
* grep "ORDER TM" and "ORDER OP"
* 所有的元方法映射的bit
*/
typedef enum {
  TM_INDEX,
  TM_NEWINDEX,
  TM_GC,
  TM_MODE,
  TM_LEN,
  TM_EQ,  /* last tag method with fast access */
  TM_ADD,
  TM_SUB,
  TM_MUL,
  TM_MOD,
  TM_POW,
  TM_DIV,
  TM_IDIV,
  TM_BAND,
  TM_BOR,
  TM_BXOR,
  TM_SHL,
  TM_SHR,
  TM_UNM,
  TM_BNOT,
  TM_LT,
  TM_LE,
  TM_CONCAT,
  TM_CALL,
  TM_N		/* number of elements in the enum */
} TMS;


/*
** 1:这个宏用来在table被修改时，清空这组标记位，强迫重新做元方法查询。
** 2:只要充当元表的table没有被修改，缺失元方法这样的查询结果就可以缓存在这组标记位中了。
** 3:我们可以看到gfasttm这个宏能够快速的剔除不存在的元方法。
** 4:另一个优化点是，不必在每次做元方法查询的时候都压入元方法的名字，在state初始化时，lua对这些元方法生成了字符串对象。
*/
#define gfasttm(g,et,e) ((et) == NULL ? NULL : \
  ((et)->flags & (1u<<(e))) ? NULL : luaT_gettm(et, e, (g)->tmname[e]))

#define fasttm(l,et,e)	gfasttm(G(l), et, e)

#define ttypename(x)	luaT_typenames_[(x) + 1]

LUAI_DDEC const char *const luaT_typenames_[LUA_TOTALTAGS];


LUAI_FUNC const char *luaT_objtypename (lua_State *L, const TValue *o);

LUAI_FUNC const TValue *luaT_gettm (Table *events, TMS event, TString *ename);
LUAI_FUNC const TValue *luaT_gettmbyobj (lua_State *L, const TValue *o,
                                                       TMS event);
LUAI_FUNC void luaT_init (lua_State *L);

LUAI_FUNC void luaT_callTM (lua_State *L, const TValue *f, const TValue *p1,
                            const TValue *p2, TValue *p3, int hasres);
LUAI_FUNC int luaT_callbinTM (lua_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
LUAI_FUNC void luaT_trybinTM (lua_State *L, const TValue *p1, const TValue *p2,
                              StkId res, TMS event);
LUAI_FUNC int luaT_callorderTM (lua_State *L, const TValue *p1,
                                const TValue *p2, TMS event);



#endif
