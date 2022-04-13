#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lapi.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

/* ����Lua�е��õĺ��� */
int average(lua_State* L) {
	//lua->stack���õ�Lua���ú�������ĸ���
	int lua_args_count = lua_gettop(L);

	//lua->c���õ�����Lua�����������ֵ
	lua_Number sum = 0;
	for (int i = 1; i <= lua_args_count; ++i) {
		sum += lua_tonumber(L, i);
	}

	//c->stack����ƽ��ֵ����ֵ���ص�Lua�еķ���ֵ
	lua_pushnumber(L, sum / lua_args_count);
	lua_pushnumber(L, sum);

	//���ظ�����Lua
	return 2;
}

/* ���г���Ҫ��װ��ȫ�ֺ��� */
static const luaL_Reg mylibs_funcs[] = {
	 { "average", average },
	 { NULL, NULL }
};

/* �۶���һ��ע��ģ�麯���������������������º���ǩ���� */
int lua_openmylib(lua_State* L) {
	luaL_newlib(L, mylibs_funcs); //�����к����ŵ�һ��table��
	return 1; //�����tableѹ��stack��
}


void main_6() {
	lua_State* L = luaL_newstate();
	if (L == NULL) {
		return;
	}

	luaL_openlibs(L);

	//lua_register(L, "average", average);

	/* �ܽ��Զ���ģ��ӵ�ע���б��� ��ע���ģ�� */
	luaL_requiref(L, "mylib", lua_openmylib, 1);
	lua_pop(L, 1);
	
	//lua_register�������������������
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


	//lua->stack,ȫ�ֺ���λ��-1
	lua_getglobal(L, "add");

	//c->stack�����뺯������
	lua_pushinteger(L, 10);
	lua_pushinteger(L, 20);

	//lua->stack�����ú�����2��ʾ����2��������1��ʾһ������ֵ����������ֵ�ŵ�-1��λ��
	lua_call(L, 2, 1);

	//stack->c
	printf("add function return value : %d\n", lua_tointeger(L, -1));


	lua_close(L);

	return;
}