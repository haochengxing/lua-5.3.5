#include <iostream>
#include <string.h>

extern "C" {
#include "src/lapi.h"
#include "src/lauxlib.h"
#include "src/lualib.h"
}

void main() {
	lua_State* L = luaL_newstate();
	if (L == NULL) {
		return;
	}

	luaopen_base(L);

	int bRet = luaL_loadfile(L,"gc.lua");
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