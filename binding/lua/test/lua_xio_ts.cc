#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <utils/thread.h>
#include <lua/lua_xio.h>
#include <lualib.h>
#include <lauxlib.h>
}

using namespace std;

static int lua_do_file(void *arg) {
    lua_State * l = luaL_newstate();
    const char *xio_source_path = getenv("XIO_SOURCE_PATH");
    assert (xio_source_path);
    string lua_file = string(xio_source_path) + string((char *)arg);
    luaL_xio(l);
    luaL_openlibs(l);
    int erred = luaL_dofile(l, lua_file.c_str());
    if(erred)
	std::cout << "Lua error: " << luaL_checkstring(l, -1) << std::endl;
    lua_close(l);
    return 0;
}

static void lua_exec(thread_t *runner, char *file) {
    thread_start(runner, lua_do_file, file);
}

TEST(xio, lua) {
    thread_t t[2];
    lua_exec(&t[0], "/binding/lua/test/sp_reqrep.lua");
    thread_stop(&t[0]);
}
