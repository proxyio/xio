#include <errno.h>
#include <time.h>
#include <string.h>
#include <utils/waitgroup.h>
#include <utils/thread.h>
#include <rex/rex.h>
#include "testutil.h"

static int af;
char* addr;
waitgroup_t wg;

int test_client(void* args) {
    struct rex_sock rs;
    int rc;
    int i;
    char buf[128] = {};

    BUG_ON(rex_sock_init(&rs, af));
    BUG_ON(rex_sock_connect(&rs, addr));
    BUG_ON(strcmp(addr, rs.ss_peer) != 0);

    for (i = 0; i < 10; i++) {
        rc = rex_sock_recv(&rs, buf, sizeof(buf));
        BUG_ON(rc != sizeof(buf));
        rc = rex_sock_send(&rs, buf, sizeof(buf));
        BUG_ON(rc != sizeof(buf));
    }

    rex_sock_destroy(&rs);
    return 0;
}

void test_socket() {
    thread_t t;
    struct rex_sock rs;
    struct rex_sock client;
    int i;
    int rc;
    char buf[128] = {};

    BUG_ON(rex_sock_init(&rs, af));
    BUG_ON(rex_sock_init(&client, af));
    BUG_ON(rex_sock_listen(&rs, addr));
    BUG_ON(strcmp(rs.ss_addr, addr) != 0);
    thread_start(&t, test_client, 0);

    BUG_ON(rex_sock_accept(&rs, &client));
    BUG_ON(strcmp(client.ss_addr, addr) != 0);
    BUG_ON(strlen(client.ss_peer) == 0);

    for (i = 0; i < 10; i++) {
        rc = rex_sock_send(&client, buf, sizeof(buf));
        BUG_ON(rc != sizeof(buf));
        rc = rex_sock_recv(&client, buf, sizeof(buf));
        BUG_ON(rc != sizeof(buf));
    }

    BUG_ON(rex_sock_destroy(&rs));
    BUG_ON(rex_sock_destroy(&client));
    thread_stop(&t);
}

int main(int argc, char** argv) {
    waitgroup_init(&wg);

    af = REX_AF_LOCAL;
    addr = "/tmp/rex_af_local";
    test_socket();

    af = REX_AF_TCP;
    addr = "127.0.0.1:1530";
    test_socket();

    waitgroup_destroy(&wg);
    return 0;
}
