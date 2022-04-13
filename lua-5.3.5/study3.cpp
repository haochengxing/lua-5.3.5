#include <iostream>  
#include <string.h>  
using namespace std;

extern "C"
{
#include "src/lua.h"  
#include "src/lauxlib.h"  
#include "src/lualib.h"  
}
void main____()
{
    //1.创建一个state
    lua_State* L = luaL_newstate();
    if (L==NULL)
    {
        return;
    }

    luaL_openlibs(L);

    //2.加载Lua文件
    int bRet = luaL_loadfile(L,"hello1.lua");
    if (bRet)
    {
        return;
    }

    //c->stack
    lua_pushstring(L, "test new value");
    //stack-lua
    lua_setglobal(L, "global_c_write_val");

    //3.运行Lua文件
    bRet = lua_pcall(L, 0, 0, 0);
    if (bRet) {
        return;
    }


    //lua->stack
    lua_getglobal(L, "global_c_read_val");

    //stack->c
    if (const char * val = lua_tostring(L,-1))
    {
        cout << val << endl;
    }

    //4.关闭state
    lua_close(L);
    return;
}