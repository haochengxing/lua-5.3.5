#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lua.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

void main_4() {
	lua_State *L = luaL_newstate();
	if (L==NULL)
	{
		return;
	}

	luaL_openlibs(L);

	int bRet = luaL_loadfile(L, "hello1.lua");
	if (bRet) {
		return;
	}

	
	//c->stack，创建一个新的table放到-1的位置
	lua_newtable(L);

	//c->stack,添加一个key，放到-1的位置，table位置变为-2
	lua_pushstring(L, "integer_val");

	//c->stack，增加key对应的值，放到-1位置，key位置变为-2，table位置变为-3
	lua_pushinteger(L,1);

	//将key和val设置到table中，并弹出key和val，此时table位置变为-1
	lua_settable(L,-3);

	//stack->lua，将table赋值到lua，并弹出table
	lua_setglobal(L, "global_c_write_table");

	bRet = lua_pcall(L,0,0,0);
	if (bRet) {
		return;
	}

	//lua->stack,得到全局表，位置-1
	lua_getglobal(L, "global_c_read_table");

	//得到第一个值
	//c->stack,设置key值,位置-1(上面的-1变为-2)
	lua_pushstring(L, "integer_val");

	//lua->stack,将-1位置的key值弹出,从lua中得到对应key的值，并将结果放在-1的位置
	//如果没有值，则结果为TNIL
	lua_gettable(L, -2);

	//stack->c
	if (lua_isinteger(L, -1)) {
		cout << "integer_val:" << lua_tointeger(L, -1) << endl;
	}

	//弹出-1位置的结果，之后全局表的位置恢复到-1
	lua_pop(L, 1);

	//重复上述步骤，得到第二个值
	lua_pushstring(L, "double_val");
	lua_gettable(L, -2);//返回值为值的类型
	if (lua_isnumber(L, -1)) {
		cout << "double_val:" << lua_tonumber(L, -1) << endl;
	}
	lua_pop(L, 1);

	//重复上述步骤，得到第三个值
	lua_pushstring(L, "string_val");
	lua_gettable(L, -2);//返回值为值的类型
	if (lua_isstring(L, -1)) {
		cout << "string_val:" << lua_tostring(L, -1) << endl;
	}
	lua_pop(L, 1);

	//使用lua_getfield代替lua_pushxxx和lua_gettable两个函数，返回值为值的类型
	lua_getfield(L, -1, "integer_val");
	cout << "integer_val:" << lua_tointeger(L, -1) << endl;

	lua_close(L);

	return;
}