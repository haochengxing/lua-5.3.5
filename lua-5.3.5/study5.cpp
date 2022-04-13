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


	//c->stack������һ���µ�table�ŵ�-1��λ��
	lua_newtable(L);

	//c->stack��������Ӧ��key-val
	lua_pushinteger(L, 7); //����ֵ
	lua_rawseti(L, -2, 1);//��Ӧkey-val

	lua_pushnumber(L, 8.9); //����ֵ
	lua_rawseti(L, -2, 2);//��Ӧkey-val

	lua_pushstring(L, "test_string");//����ֵ
	lua_rawseti(L, -2, 3);//��Ӧkey-val

	//--------------�������ԭ����table����-------------
	//lua_pushinteger(L, 1); //����key
	//lua_pushinteger(L, 7);//����ֵ
	//lua_settable(L, -3); //Ҳ����ʹ��lua_rawset(L,-3);

	//lua_pushinteger(L, 2); //����key
	//lua_pushnumber(L, 8.9);//����ֵ
	//lua_settable(L, -3); //Ҳ����ʹ��lua_rawset(L,-3);

	//lua_pushinteger(L, 3); //����key
	//lua_pushstring(L, "test_string");//����ֵ
	//lua_settable(L, -3); //Ҳ����ʹ��lua_rawset(L,-3);


	//stack->lua�������鸳ֵ��lua�У�����������
	lua_setglobal(L, "global_c_write_array");


	bRet = lua_pcall(L, 0, 0, 0);
	if (bRet) {
		return;
	}


	lua_settop(L, 0);

	//lua->stack,�õ�ȫ�����飬λ��-1
	lua_getglobal(L,"global_c_read_array");

	//�õ����鳤��
	lua_Integer array_len = luaL_len(L,-1);
	for (lua_Integer i = 1; i <= array_len; i++)
	{
		//lua->stack��ȫ������λ��-1��Ȼ��i��Ӧ������ֵ��������ֵ�ŵ�-1��λ��
		int ret_type = lua_rawgeti(L,-1,i);

		//���滻��ԭ����table����
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