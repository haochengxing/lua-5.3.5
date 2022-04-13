#include <iostream>  
#include <string.h>  
using namespace std;

extern "C"
{
#include "src/lua.h"  
#include "src/lauxlib.h"  
#include "src/lualib.h"  
#include "mLualib.h"
}
void main_2()
{
    //1.创建Lua状态  
    lua_State* L = luaL_newstate();
    if (L == NULL)
    {
        return;
    }

    luaL_openlibs(L);

    luaopen_mLualib(L);

    //2.加载Lua文件  
    int bRet = luaL_loadfile(L, "mLualib.lua");
    if (bRet)
    {
        cout << "load file error" << endl;
        return;
    }

    //3.运行Lua文件  
    bRet = lua_pcall(L, 0, 0, 0);
    if (bRet)
    {
        cout << "pcall error" << endl;
        return;
    }

    

    //7.关闭state  
    lua_close(L);
    return;
}