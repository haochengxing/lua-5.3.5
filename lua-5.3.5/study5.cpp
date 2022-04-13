#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lapi.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

void main_5() {
	lua_State* L = luaL_newstate();
	if (L == NULL) {
		return;
	}

	luaL_openlibs(L);

	int bRet = luaL_loadfile(L,"hello2.lua");
	if (bRet) {
		return;
	}


	//c->stack，创建一个新的table放到-1的位置
	lua_newtable(L);

	//c->stack，创建对应的key-val
	lua_pushinteger(L, 7); //设置值
	lua_rawseti(L, -2, 1);//对应key-val

	lua_pushnumber(L, 8.9); //设置值
	lua_rawseti(L, -2, 2);//对应key-val

	lua_pushstring(L, "test_string");//设置值
	lua_rawseti(L, -2, 3);//对应key-val

	//--------------被替代的原操作table函数-------------
	//lua_pushinteger(L, 1); //设置key
	//lua_pushinteger(L, 7);//设置值
	//lua_settable(L, -3); //也可以使用lua_rawset(L,-3);

	//lua_pushinteger(L, 2); //设置key
	//lua_pushnumber(L, 8.9);//设置值
	//lua_settable(L, -3); //也可以使用lua_rawset(L,-3);

	//lua_pushinteger(L, 3); //设置key
	//lua_pushstring(L, "test_string");//设置值
	//lua_settable(L, -3); //也可以使用lua_rawset(L,-3);


	//stack->lua，将数组赋值到lua中，并弹出数组
	lua_setglobal(L, "global_c_write_array");


	bRet = lua_pcall(L, 0, 0, 0);
	if (bRet) {
		return;
	}


	lua_settop(L, 0);

	//lua->stack,得到全局数组，位置-1
	lua_getglobal(L,"global_c_read_array");

	//得到数组长度
	lua_Integer array_len = luaL_len(L,-1);
	for (lua_Integer i = 1; i <= array_len; i++)
	{
		//lua->stack，全局数组位置-1，然后i对应的索引值，将返回值放到-1的位置
		int ret_type = lua_rawgeti(L,-1,i);

		//被替换的原操作table函数
		//lua_pushinteger(L,i);
		//int ret_type = lua_gettable(L, -2);
		//int ret_type = lua_rawget(L,-2);

		//stack->c
		if (ret_type == LUA_TNUMBER){
			if (lua_isinteger(L, -1)) {
				cout << lua_tointeger(L,-1) << endl;
			}
			else if (lua_isnumber(L, -1)) {
				cout << lua_tonumber(L, -1) << endl;
			}
		}
		else if (ret_type == LUA_TSTRING) {
			cout << lua_tostring(L, -1) << endl;
		}

		lua_pop(L, 1);
	}

	

	lua_close(L);

	return;
}