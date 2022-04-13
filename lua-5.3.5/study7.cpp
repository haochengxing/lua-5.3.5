#include <iostream>
#include <string.h>
using namespace std;

extern "C" {
#include "src/lapi.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

#include "StudentRegFuncs.h"

void main() {
	lua_State* L = luaL_newstate();
	if (L == NULL) {
		return;
	}

	luaL_openlibs(L);

	luaL_requiref(L,"Student",luaopen_student_libs,1);
	lua_pop(L, 1);

	int bRet = luaL_loadfile(L,"student.lua");
	if (bRet) {
		return;
	}

	bRet = lua_pcall(L,0,0,0);
	if (bRet) {
		return;
	}

	lua_close(L);
	return;
}