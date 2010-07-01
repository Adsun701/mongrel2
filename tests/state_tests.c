#include "minunit.h"
#include <state.h>
#include <events.h>

FILE *LOG_FILE = NULL;


/**
 * Runs a bunch of events on the given state, printing out the
 * results, and then returning whether it exited cleanly or not.
 */
int run_events(State *state, const char *name, int *events)
{
    int i = 0;
    int rc = 0;
    State_init(state);

    debug(">>> RUNNING %s", name);

    for(i = 0; events[i] != 0; i++) {
        debug("EVENT[%d]: %d", i, events[i]);
        rc = State_exec(state, events[i]);
        check(rc != -1, "Failed on processing %d event.", events[i]);
    }

    debug("<<< FINAL RESULT: %d", rc);
    return rc == 1;
error:
    return 0;
}

/**
 * A fancy macro that just sets up a test run with an array of 
 * integers matching the events.h events enum.
 */
#define RUN(N, ...) int N[] = { __VA_ARGS__, 0 };\
          mu_assert(run_events(&state, "TEST " #N, N), "Test " #N " failed.");

/**
 * Same thing but should fail.
 */
#define FAILS(N, ...) int N[] = { __VA_ARGS__, 0 };\
          mu_assert(!run_events(&state, "TEST " #N, N), "Test " #N " failed.");

char *test_State_msg()
{
    State state;

    // Simulates the most basic MSG message request for a handler.
    RUN(msg_handler, 
            OPEN, ACCEPT, 
            REQ_RECV, MSG_REQ, HANDLER, REQ_SENT, MSG_RESP, RESP_SENT,
            CLOSE);

    // Simulates a basic socket request start
    RUN(socket_start, OPEN, ACCEPT, REQ_RECV, SOCKET_REQ);

    // Simulates a full async setup of one req and 2 responses
    RUN(msg_handler_2_resp, 
            OPEN, ACCEPT, 
            REQ_RECV, MSG_REQ, HANDLER, REQ_SENT,
            MSG_RESP, RESP_SENT,
            MSG_RESP, RESP_SENT,
            CLOSE);


    return NULL;
}


char *test_State_http() 
{
    State state;

    // Simulates doing a basic HTTP request then closing the connection.
    RUN(http_dir,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, DIRECTORY, RESP_SENT, CLOSE);

    // Simulates two keep-alive handler requests then a close.
    RUN(http_handler,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT, CLOSE);

    // Simulates an accept then idle timeout
    RUN(http_idle_timeout, OPEN, ACCEPT, TIMEOUT);


    // Simulates a request for a dir then timeout
    RUN(http_dir_timeout,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, DIRECTORY, TIMEOUT);

    // Simulates a handler, then a dir, then timeout
    RUN(http_handler_dir_timeout,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT,
            REQ_RECV, HTTP_REQ, DIRECTORY, RESP_SENT, TIMEOUT);

    // Simulates handler, dir, proxy attempt, then proxy closes abruptly
    RUN(http_handler_dir_proxy_remote_close,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT,
            REQ_RECV, HTTP_REQ, DIRECTORY, RESP_SENT,
            REQ_RECV, HTTP_REQ, PROXY, REMOTE_CLOSE, CLOSE); 

    // Simulates a dir, then proxy, then back to a handler
    RUN(http_handler_dir_proxy_handler,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT,
            REQ_RECV, HTTP_REQ, DIRECTORY, RESP_SENT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, RESP_RECV, RESP_SENT,
            REQ_RECV, HTTP_REQ, HANDLER, REQ_SENT, CLOSE);


    // Simulates two requests over a proxy connection followed by
    // the remote closing the connection so we have to shutdown.
    RUN(http_proxy,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, RESP_RECV, RESP_SENT,
            REQ_RECV, HTTP_REQ, PROXY, REQ_SENT, RESP_RECV, RESP_SENT, REMOTE_CLOSE,
            CLOSE);

    // Simulates a proxy connection that is then routed to a directory
    // access, so has to change from proxying to serving a file.
    RUN(http_proxy_dir, 
            OPEN, ACCEPT, 
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, RESP_RECV, RESP_SENT, 
            REQ_RECV, HTTP_REQ, DIRECTORY, RESP_SENT, CLOSE);

    /**
     * These all simulate timeouts on different proxy operations.
     */
    RUN(http_proxy_timeout_send,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, TIMEOUT);

    RUN(http_proxy_timeout_expecting,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, TIMEOUT);

    RUN(http_proxy_timeout_responding,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, RESP_RECV, TIMEOUT);

    RUN(http_proxy_timeout_connected,
            OPEN, ACCEPT,
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, REQ_SENT, RESP_RECV, RESP_SENT,
            TIMEOUT);


    return NULL;
}


char *test_State_exec_error() 
{
    State state;

    // Basic client opens, then closes right away
    FAILS(failed_connect, OPEN, CLOSE);

    // Proxy error where client closed connection mid-connect
    FAILS(failed_proxy, 
            OPEN, ACCEPT, 
            REQ_RECV, HTTP_REQ, PROXY, CONNECT, CLOSE);

    return NULL;
}

char * all_tests() {
    mu_suite_start();

    mu_run_test(test_State_exec_error);
    mu_run_test(test_State_msg);
    mu_run_test(test_State_http);

    return NULL;
}

RUN_TESTS(all_tests);

