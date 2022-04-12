#pragma once  
extern "C" {
#include "src/lua.h"  
#include "src/lualib.h"  
#include "src/lauxlib.h"  
}

#ifdef LUA_EXPORTS  
#define LUA_API __declspec(dllexport)  
#else  
#define LUA_API __declspec(dllimport)  
#endif  

extern "C" LUA_API int luaopen_mLualib(lua_State * L);//定义导出函数