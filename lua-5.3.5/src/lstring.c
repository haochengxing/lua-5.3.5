/*
** $Id: lstring.c,v 2.56.1.1 2017/04/19 17:20:42 roberto Exp $
** String table (keeps all strings handled by Lua)
** See Copyright Notice in lua.h
*/

#define lstring_c
#define LUA_CORE

#include "lprefix.h"


#include <string.h>

#include "lua.h"

#include "ldebug.h"
#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"


#define MEMERRMSG       "not enough memory"


/*
** Lua will use at most ~(2^LUAI_HASHLIMIT) bytes from a string to
** compute its hash
*/
#if !defined(LUAI_HASHLIMIT)
#define LUAI_HASHLIMIT		5
#endif


/*
** equality for long strings
* 长字符串需要先比较长度,在比较内容
* 短字符串的比较直接比较地址,因为在lua中相同的短字符串只会存在一份
*/
int luaS_eqlngstr (TString *a, TString *b) {
  size_t len = a->u.lnglen;
  lua_assert(a->tt == LUA_TLNGSTR && b->tt == LUA_TLNGSTR);
  return (a == b) ||  /* same instance or... */
    ((len == b->u.lnglen) &&  /* equal length and ... */
     (memcmp(getstr(a), getstr(b), len) == 0));  /* equal contents */
}


unsigned int luaS_hash (const char *str, size_t l, unsigned int seed) {
  unsigned int h = seed ^ cast(unsigned int, l);
  size_t step = (l >> LUAI_HASHLIMIT) + 1;
  for (; l >= step; l -= step)
    h ^= ((h<<5) + (h>>2) + cast_byte(str[l - 1]));
  return h;
}

/*
** 长字符串是属于惰性求hash值,如果已经错在hash值,就直接返回,不再重新求
*/
unsigned int luaS_hashlongstr (TString *ts) {
  lua_assert(ts->tt == LUA_TLNGSTR);
  if (ts->extra == 0) {  /* no hash? */
    ts->hash = luaS_hash(getstr(ts), ts->u.lnglen, ts->hash);
    ts->extra = 1;  /* now it has its hash */
  }
  return ts->hash;
}


/*
** resizes the string table
*/
void luaS_resize (lua_State *L, int newsize) {
  int i;
  stringtable *tb = &G(L)->strt;
  if (newsize > tb->size) {  /* grow table if needed */
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
    for (i = tb->size; i < newsize; i++)
      tb->hash[i] = NULL;
  }
  for (i = 0; i < tb->size; i++) {  /* rehash */
    TString *p = tb->hash[i];
    tb->hash[i] = NULL;
    while (p) {  /* for each node in the list */
      TString *hnext = p->u.hnext;  /* save next */
      unsigned int h = lmod(p->hash, newsize);  /* new position */
      p->u.hnext = tb->hash[h];  /* chain it */
      tb->hash[h] = p;
      p = hnext;
    }
  }
  if (newsize < tb->size) {  /* shrink table if needed */
    /* vanishing slice should be empty */
    lua_assert(tb->hash[newsize] == NULL && tb->hash[tb->size - 1] == NULL);
    luaM_reallocvector(L, tb->hash, tb->size, newsize, TString *);
  }
  tb->size = newsize;
}


/*
 ** Clear API string cache. (Entries cannot be empty, so fill them with
 ** a non-collectable string.)
 ** 清楚缓存
 */
void luaS_clearcache(global_State *g) {
	int i, j;
	for (i = 0; i < STRCACHE_N; i++)
		for (j = 0; j < STRCACHE_M; j++) {
			if (iswhite(g->strcache[i][j])) /* will entry be collected? */
				g->strcache[i][j] = g->memerrmsg; /* replace it with something fixed */
		}
}


/*
 ** Initialize the string table and the string cache
 ** 初始化字符串链表和字符串缓存
 */
void luaS_init(lua_State *L) {
	global_State *g = G(L);
	int i, j;
	luaS_resize(L, MINSTRTABSIZE); /* 默认大小128 字符串table initial size of string table */
	/* pre-create memory-error message */
	g->memerrmsg = luaS_newliteral(L, MEMERRMSG); //错误处理字符串
	luaC_fix(L, obj2gco(g->memerrmsg)); /* it should never be collected */
	for (i = 0; i < STRCACHE_N; i++) /* 缓存表中填充默认支付 fill cache with valid strings */
		for (j = 0; j < STRCACHE_M; j++)
			g->strcache[i][j] = g->memerrmsg;
}


/*
** creates a new string object
*/
static TString *createstrobj (lua_State *L, size_t l, int tag, unsigned int h) {
  TString *ts;
  GCObject *o;
  size_t totalsize;  /* total size of TString object */
  totalsize = sizelstring(l);
  o = luaC_newobj(L, tag, totalsize);
  ts = gco2ts(o);
  ts->hash = h;
  ts->extra = 0;//在extra的赋值上看到赋值为0,不是虚拟器需要保存的字符串
  getstr(ts)[l] = '\0';  /* ending 0 */
  return ts;
}


TString *luaS_createlngstrobj (lua_State *L, size_t l) {
  TString *ts = createstrobj(L, l, LUA_TLNGSTR, G(L)->seed);
  ts->u.lnglen = l;//长字符串的长度是保存在联合结构体内的lnglen中,和短字符串不同.
  return ts;
}


void luaS_remove (lua_State *L, TString *ts) {
  stringtable *tb = &G(L)->strt;
  TString **p = &tb->hash[lmod(ts->hash, tb->size)];
  while (*p != ts)  /* find previous element */
    p = &(*p)->u.hnext;
  *p = (*p)->u.hnext;  /* remove element from its list */
  tb->nuse--;
}


/*
** checks whether short string exists and reuses it or creates a new one
*/
static TString *internshrstr (lua_State *L, const char *str, size_t l) {
  TString *ts;
  global_State *g = G(L);
  unsigned int h = luaS_hash(str, l, g->seed);//通过str和长度以及seed(种子)算出该str的hash值
  TString **list = &g->strt.hash[lmod(h, g->strt.size)];//根据hash值在全局字符串table中找到链表(短字符串存在全局表中global_state)
  lua_assert(str != NULL);  /* otherwise 'memcmp'/'memcpy' are undefined */
  for (ts = *list; ts != NULL; ts = ts->u.hnext) {
    if (l == ts->shrlen &&
        (memcmp(str, getstr(ts), l * sizeof(char)) == 0)) {
      /* found! */
      if (isdead(g, ts))  /* dead (but not collected yet)? */
        changewhite(ts);  /* resurrect it */
      return ts;//果长度和所有的字符都相同,就重复利用.否则需要重新生成
    }
  }
  if (g->strt.nuse >= g->strt.size && g->strt.size <= MAX_INT/2) {//从代码中看出如果链表的字符串对象的个数不小于size,就重新调用luaS_resize,空间是之前的2倍.
    luaS_resize(L, g->strt.size * 2);
    list = &g->strt.hash[lmod(h, g->strt.size)];  /* recompute with new size */
  }
  ts = createstrobj(L, l, LUA_TSHRSTR, h);
  memcpy(getstr(ts), str, l * sizeof(char));
  ts->shrlen = cast_byte(l);
  ts->u.hnext = *list;
  *list = ts;
  g->strt.nuse++;
  return ts;
}


/*
 ** new string (with explicit length)
 ** 创建一个存新的字符串，不带缓存
 ** 字符串不能超过最大限制
 ** 新的字符串会memcpy拷贝一个副本，挂载到TString结构上
 */
TString *luaS_newlstr(lua_State *L, const char *str, size_t l) {
	if (l <= LUAI_MAXSHORTLEN) /* short string?  40字节 */
		return internshrstr(L, str, l);
	else {
		TString *ts;
		if (l >= (MAX_SIZE - sizeof(TString)) / sizeof(char))
			luaM_toobig(L);
		ts = luaS_createlngstrobj(L, l);
		memcpy(getstr(ts), str, l * sizeof(char)); //内存拷贝副本
		return ts;
	}
}

/*
 ** Create or reuse a zero-terminated string, first checking in the
 ** cache (using the string address as a key). The cache can contain
 ** only zero-terminated strings, so it is safe to use 'strcmp' to
 ** check hits.
 ** 创建一个新的字符串，带缓存方式
 ** 会调用luaS_newlstr方法
 ** 1. 先通过字符串，获取字符串hash值
 ** 2. 通过hash值取字符串，如果相同的字符串已经存在，则复用
 ** 3. 否则创建一个新的字符串
 **
 ** 字符串Table表，通过字符串的hash值找到list
 ** 但是list长度是STRCACHE_M=2，list比较小，估计作者认为hash冲突的概率会非常小
 ** 同时每次都会将最早的元素element淘汰出去
 */
TString *luaS_new(lua_State *L, const char *str) {
	unsigned int i = point2uint(str) % STRCACHE_N; /* hash */
	int j;
	TString **p = G(L)->strcache[i];
	for (j = 0; j < STRCACHE_M; j++) {
		if (strcmp(str, getstr(p[j])) == 0) /* hit? */
			return p[j]; /* that is it */
	}
	/* normal route */
	for (j = STRCACHE_M - 1; j > 0; j--)
		p[j] = p[j - 1]; /* move out last element */
	/* new element is first in the list */
	p[0] = luaS_newlstr(L, str, strlen(str));
	return p[0];
}


Udata *luaS_newudata (lua_State *L, size_t s) {
  Udata *u;
  GCObject *o;
  if (s > MAX_SIZE - sizeof(Udata))
    luaM_toobig(L);
  o = luaC_newobj(L, LUA_TUSERDATA, sizeludata(s));
  u = gco2u(o);
  u->len = s;
  u->metatable = NULL;
  setuservalue(L, u, luaO_nilobject);
  return u;
}

