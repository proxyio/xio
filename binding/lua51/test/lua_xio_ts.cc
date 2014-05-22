#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <lua51/lua_xio.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace std;

static string xio_ts_code;

TEST(xio, lua) {
    lua_State * l = luaL_newstate();
    const char *xio_source_path = getenv("XIO_SOURCE_PATH");
    if (!xio_source_path)
	return;
    string lua_file = xio_source_path;
    lua_file += "/binding/lua51/test/ts.lua";
    luaopen_xio(l);
    luaL_openlibs(l);
    int erred = luaL_dofile(l, lua_file.c_str());
    if(erred)
	std::cout << "Lua error: " << luaL_checkstring(l, -1) << std::endl;
    lua_close(l);
}
