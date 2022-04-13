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
            //����
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
    // ����ջ��ѹ��2��ֵ  
    lua_pushnumber(L, 10);
    lua_pushstring(L, "hello");

    return 2;
}


void main_1()
{
    //1.����Lua״̬  
    lua_State* L = luaL_newstate();
    if (L == NULL)
    {
        return;
    }

    //2.����Lua�ļ�  
    int bRet = luaL_loadfile(L, "hello.lua");
    if (bRet)
    {
        cout << "load file error" << endl;
        return;
    }

    //3.����Lua�ļ�  
    bRet = lua_pcall(L, 0, 0, 0);
    if (bRet)
    {
        cout << "pcall error" << endl;
        return;
    }

    cout << "top : " << lua_gettop(L) << endl;

    // ����һ���µ�table����ѹ��ջ  
    lua_newtable(L);
    cout << "newtable : " << lua_gettop(L) << endl;
    // ��table������ֵ  
    lua_pushstring(L, "Give me a girl friend !"); //��ֵѹ��ջ  
    cout << "pushstring : " << lua_gettop(L) << endl;
    //lua_setfield(L, -2, "str"); //��ֵ���õ�table�У�����Give me a girl friend ��ջ
    lua_setfield(L, 1, "str"); //��ֵ���õ�table�У�����Give me a girl friend ��ջ
    
    cout << "setfield {str=''} : " << lua_gettop(L) << endl;

    //4.��ȡ����  
    lua_getglobal(L, "str");
    string str = lua_tostring(L, -1);
    cout << "str = " << str.c_str() << endl;        //str = I am so cool~  

    cout << "getglobal str : " << lua_gettop(L) << endl;

    //5.��ȡtable  
    lua_getglobal(L, "tbl");
    cout << "getglobal tbl : " << lua_gettop(L) << endl;
    lua_getfield(L, -1, "name");
    str = lua_tostring(L, -1);
    cout << "tbl:name = " << str.c_str() << endl; //tbl:name = shun  

    cout << "getfield tbl.name : " << lua_gettop(L) << endl;

    //6.��ȡ����  
    lua_getglobal(L, "add");        // ��ȡ������ѹ��ջ��  
    cout << "getglobal add : " << lua_gettop(L) << endl;
    lua_pushnumber(L, 10);          // ѹ���һ������  
    cout << "pushnumber : " << lua_gettop(L) << endl;
    lua_pushnumber(L, 20);          // ѹ��ڶ�������  
    cout << "pushnumber : " << lua_gettop(L) << endl;
    int iRet = lua_pcall(L, 2, 1, 0);// ���ú�������������Ժ󣬻Ὣ����ֵѹ��ջ�У�2��ʾ����������1��ʾ���ؽ��������  
    cout << "lua_pcall : " << lua_gettop(L) << endl;
    if (iRet)                       // ���ó���  
    {
        const char* pErrorMsg = lua_tostring(L, -1);
        cout << pErrorMsg << endl;
        lua_close(L);
        return;
    }
    if (lua_isnumber(L, -1))        //ȡֵ���  
    {
        double fValue = lua_tonumber(L, -1);
        cout << "Result is " << fValue << endl;
    }

    lua_pushstring(L, "����һ����˧����");  //   
    cout << "pushstring : " << lua_gettop(L) << endl;
    lua_setfield(L, 1, "name");             // �Ὣ"����һ����˧����"��ջ
    cout << "setfield : " << lua_gettop(L) << endl;

    lua_pushcfunction(L, getTwoVar); //����������ջ��  

    cout << "pushcfunction : " << lua_gettop(L) << endl;

    lua_setglobal(L, "getTwoVar");   //����luaȫ�ֱ���getTwoVar

    cout << "setglobal getTwoVar : " << lua_gettop(L) << endl;

    lua_getglobal(L, "getTwoVar");        // ��ȡ������ѹ��ջ�� 

    cout << "getglobal getTwoVar : " << lua_gettop(L) << endl;

    int Ret2 = lua_pcall(L, 0, 2, 0);

    cout << "lua_pcall result : " << Ret2 << endl;

    cout << "lua_pcall getTwoVar : " << lua_gettop(L) << endl;

    //cout << "Result number is " << lua_tonumber(L, -2) << endl;
    //cout << "Result string is " << lua_tostring(L, -1) << endl;

    cout << "Result number is " << lua_tonumber(L, 6) << endl;
    cout << "Result string is " << lua_tostring(L, 7) << endl;

    int stackTop = lua_gettop(L);//��ȡջ��������ֵ  


    //printf(" element count: %d\n", stackTop);  

    cout << "--ջ�� " << stackTop<<endl;

    //��ʾջ�е�Ԫ��  
    stack_dump(L);

    //���ˣ�ջ�е�����ǣ�  
    //=================== ջ�� ===================   
    //  ����  ����      ֵ  
    //   4   int��      30   
    //   3   string��   shun   
    //   2   table:     tbl  
    //   1   string:    I am so cool~  
    //=================== ջ�� ===================   

    //7.�ر�state  
    lua_close(L);
    return;
}


