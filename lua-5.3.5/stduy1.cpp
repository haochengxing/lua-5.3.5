#include <iostream>  
#include <string.h>  
using namespace std;

extern "C"
{
#include "src/lua.h"  
#include "src/lauxlib.h"  
#include "src/lualib.h"  
}


void stack_dump(lua_State* L) {
    printf("\n------ stack dump begin ------\n");
    for (int i = 1; i <= lua_gettop(L); ++i) {
        printf("index : %d , ",i);
        int t = lua_type(L, i);
        switch (t) {
        case LUA_TNONE: {
            printf("LUA_TNONE\n");
        }break;

        case LUA_TNIL: {
            printf("LUA_TNIL\n");
        }break;

        case LUA_TBOOLEAN: {
            printf("LUA_TBOOLEAN : %s\n", lua_toboolean(L, i) ? "true" : "false");
        }break;

        case LUA_TLIGHTUSERDATA: {
            printf("LUA_TLIGHTUSERDATA\n");
        }break;

        case LUA_TNUMBER: {
            //整形
            if (lua_isinteger(L, i)) {
                printf("LUA_TNUMBER integer : %lld \n", lua_tointeger(L, i));
            }
            else if (lua_isnumber(L, i)) {
                printf("LUA_TNUMBER number: %g\n", lua_tonumber(L, i));
            }
        }break;

        case LUA_TSTRING: {
            printf("LUA_TSTRING : %s\n", lua_tostring(L, i));
        }break;

        case LUA_TTABLE: {
            printf("LUA_TTABLE\n");
        }break;

        case LUA_TFUNCTION: {
            printf("LUA_TFUNCTION\n");
        }break;

        case LUA_TUSERDATA: {
            printf("LUA_TUSERDATA\n");
        }break;

        case LUA_TTHREAD: {
            printf("LUA_TTHREAD\n");
        }break;

        case LUA_NUMTAGS: {
            printf("LUA_NUMTAGS\n");
        }break;

        default: {
            printf("%s\n", lua_typename(L, t));
        }break;
        }
    }

    std::cout << "------ stack dump end ------" << std::endl;
}

// This is my function  
static int getTwoVar(lua_State* L)
{
    // 向函数栈中压入2个值  
    lua_pushnumber(L, 10);
    lua_pushstring(L, "hello");

    return 2;
}


void main_1()
{
    //1.创建Lua状态  
    lua_State* L = luaL_newstate();
    if (L == NULL)
    {
        return;
    }

    //2.加载Lua文件  
    int bRet = luaL_loadfile(L, "hello.lua");
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

    cout << "top : " << lua_gettop(L) << endl;

    // 创建一个新的table，并压入栈  
    lua_newtable(L);
    cout << "newtable : " << lua_gettop(L) << endl;
    // 往table中设置值  
    lua_pushstring(L, "Give me a girl friend !"); //将值压入栈  
    cout << "pushstring : " << lua_gettop(L) << endl;
    //lua_setfield(L, -2, "str"); //将值设置到table中，并将Give me a girl friend 出栈
    lua_setfield(L, 1, "str"); //将值设置到table中，并将Give me a girl friend 出栈
    
    cout << "setfield {str=''} : " << lua_gettop(L) << endl;

    //4.读取变量  
    lua_getglobal(L, "str");
    string str = lua_tostring(L, -1);
    cout << "str = " << str.c_str() << endl;        //str = I am so cool~  

    cout << "getglobal str : " << lua_gettop(L) << endl;

    //5.读取table  
    lua_getglobal(L, "tbl");
    cout << "getglobal tbl : " << lua_gettop(L) << endl;
    lua_getfield(L, -1, "name");
    str = lua_tostring(L, -1);
    cout << "tbl:name = " << str.c_str() << endl; //tbl:name = shun  

    cout << "getfield tbl.name : " << lua_gettop(L) << endl;

    //6.读取函数  
    lua_getglobal(L, "add");        // 获取函数，压入栈中  
    cout << "getglobal add : " << lua_gettop(L) << endl;
    lua_pushnumber(L, 10);          // 压入第一个参数  
    cout << "pushnumber : " << lua_gettop(L) << endl;
    lua_pushnumber(L, 20);          // 压入第二个参数  
    cout << "pushnumber : " << lua_gettop(L) << endl;
    int iRet = lua_pcall(L, 2, 1, 0);// 调用函数，调用完成以后，会将返回值压入栈中，2表示参数个数，1表示返回结果个数。  
    cout << "lua_pcall : " << lua_gettop(L) << endl;
    if (iRet)                       // 调用出错  
    {
        const char* pErrorMsg = lua_tostring(L, -1);
        cout << pErrorMsg << endl;
        lua_close(L);
        return;
    }
    if (lua_isnumber(L, -1))        //取值输出  
    {
        double fValue = lua_tonumber(L, -1);
        cout << "Result is " << fValue << endl;
    }

    lua_pushstring(L, "我是一个大帅锅～");  //   
    cout << "pushstring : " << lua_gettop(L) << endl;
    lua_setfield(L, 1, "name");             // 会将"我是一个大帅锅～"出栈
    cout << "setfield : " << lua_gettop(L) << endl;

    lua_pushcfunction(L, getTwoVar); //将函数放入栈中  

    cout << "pushcfunction : " << lua_gettop(L) << endl;

    lua_setglobal(L, "getTwoVar");   //设置lua全局变量getTwoVar

    cout << "setglobal getTwoVar : " << lua_gettop(L) << endl;

    lua_getglobal(L, "getTwoVar");        // 获取函数，压入栈中 

    cout << "getglobal getTwoVar : " << lua_gettop(L) << endl;

    int Ret2 = lua_pcall(L, 0, 2, 0);

    cout << "lua_pcall result : " << Ret2 << endl;

    cout << "lua_pcall getTwoVar : " << lua_gettop(L) << endl;

    //cout << "Result number is " << lua_tonumber(L, -2) << endl;
    //cout << "Result string is " << lua_tostring(L, -1) << endl;

    cout << "Result number is " << lua_tonumber(L, 6) << endl;
    cout << "Result string is " << lua_tostring(L, 7) << endl;

    int stackTop = lua_gettop(L);//获取栈顶的索引值  


    //printf(" element count: %d\n", stackTop);  

    cout << "--栈顶 " << stackTop<<endl;

    //显示栈中的元素  
    stack_dump(L);

    //至此，栈中的情况是：  
    //=================== 栈顶 ===================   
    //  索引  类型      值  
    //   4   int：      30   
    //   3   string：   shun   
    //   2   table:     tbl  
    //   1   string:    I am so cool~  
    //=================== 栈底 ===================   

    //7.关闭state  
    lua_close(L);
    return;
}


