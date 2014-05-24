#include <gtest/gtest.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <string>
extern "C" {
#include <utils/thread.h>
#include <python/proxyio.py.h>
#include <python2.6/pythonrun.h>
}

using namespace std;

static int py_do_file(void *arg) {
    const char *xio_source_path = getenv("XIO_SOURCE_PATH");
    assert (xio_source_path);
    string py_file = string(xio_source_path) + string((char *)arg);
    FILE *fp = fopen(py_file.c_str(), "r+");
    assert (fp);
    int rc = PyRun_SimpleFile(fp, py_file.c_str());
    if(rc)
	std::cout << "Python error: " << rc << std::endl;
    return 0;
}

static void py_exec(thread_t *runner, char *file) {
    thread_start(runner, py_do_file, file);
}

TEST(xio, python) {
    thread_t t[2];
    Py_Initialize();
    pyopen_xio();
    py_exec(&t[0], "/binding/python/test/sp_reqrep.py");
    thread_stop(&t[0]);
    Py_Finalize();
}
