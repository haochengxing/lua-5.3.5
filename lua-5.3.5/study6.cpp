#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lapi.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

/* ①在Lua中调用的函数 */
int average(lua_State* L) {
	//lua->stack，得到Lua调用函数输入的个数
	int lua_args_count = lua_gettop(L);

	//lua->c，得到所有Lua中输入参数的值
	lua_Number sum = 0;
	for (int i = 1; i <= lua_args_count; ++i) {
		sum += lua_tonumber(L, i);
	}

	//c->stack，将平均值和总值返回到Lua中的返回值
	lua_pushnumber(L, sum / lua_args_count);
	lua_pushnumber(L, sum);

	//返回个数给Lua
	return 2;
}

/* ②列出需要封装的全局函数 */
static const luaL_Reg mylibs_funcs[] = {
	 { "average", average },
	 { NULL, NULL }
};

/* ③定义一个注册模块函数，这个函数必须符合如下函数签名： */
int lua_openmylib(lua_State* L) {
	luaL_newlib(L, mylibs_funcs); //将所有函数放到一个table中
	return 1; //将这个table压到stack里
}


void main_6() {
	lua_State* L = luaL_newstate();
	if (L == NULL) {
		return;
	}

	luaL_openlibs(L);

	//lua_register(L, "average", average);

	/* ④将自定义模块加到注册列表里 ⑤注册该模块 */
	luaL_requiref(L, "mylib", lua_openmylib, 1);
	lua_pop(L, 1);
	
	//lua_register宏替代了下面两个函数
	//lua_pushcfunction(L, average);
	//lua_setglobal(L, "average");

	int bRet = luaL_loadfile(L, "hello3.lua");
	if (bRet) {
		return;
	}


	bRet = lua_pcall(L, 0, 0, 0);
	if (bRet) {
		return;
	}


	//lua->stack,全局函数位置-1
	lua_getglobal(L, "add");

	//c->stack，传入函数参数
	lua_pushinteger(L, 10);
	lua_pushinteger(L, 20);

	//lua->stack，调用函数，2表示传入2个参数，1表示一个返回值，并将返回值放到-1的位置
	lua_call(L, 2, 1);

	//stack->c
	printf("add function return value : %d\n", lua_tointeger(L, -1));


	lua_close(L);

	return;
}