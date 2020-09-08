#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>
#include "log.h"

static const int times = 100;

static void *DebugLogThread(__attribute__((unused)) void *arg) {
    for (int i = 0; i < times; ++i) {
        LogDebug("Debug\n");
        usleep(1000);
    }
    return NULL;
}

static void *InfoLogThread(__attribute__((unused)) void *arg) {
    for (int i = 0; i < times; ++i) {
        LogInfo("Info\n");
        usleep(1000);
    }
    return NULL;
}

static void *WaringLogThread(__attribute__((unused)) void *arg) {
    for (int i = 0; i < times; ++i) {
        LogWarning("Waring\n");
        usleep(1000);
    }
    return NULL;
}

static void *ErrorLogThread(__attribute__((unused)) void *arg) {
    for (int i = 0; i < times; ++i) {
        LogError("Error\n");
        usleep(1000);
    }
    return NULL;
}

int main() {
    int ret = 0;
    pthread_t debug_log_tid = 0;
    pthread_t info_log_tid = 0;
    pthread_t waring_log_tid = 0;
    pthread_t error_log_tid = 0;
    struct timeval start;
    struct timeval end;
    unsigned long timer;
    gettimeofday(&start, NULL);

    AeSetLogPath("TestLog.log");
    AeSetLogLevel(LOG_LEVEL_TRACE);
    AeSetLogMode(LOG_MODE_FILE);

    ret = pthread_create(&debug_log_tid, NULL, DebugLogThread, NULL);
    if (ret) {
        fprintf(stderr, "Error:unable to create thread, %d\n", ret);
        exit(-1);
    }

    ret = pthread_create(&info_log_tid, NULL, InfoLogThread, NULL);
    if (ret) {
        fprintf(stderr, "Error:unable to create thread, %d\n", ret);
        exit(-1);
    }

    ret = pthread_create(&waring_log_tid, NULL, WaringLogThread, NULL);
    if (ret) {
        fprintf(stderr, "Error:unable to create thread, %d\n", ret);
        exit(-1);
    }

    ret = pthread_create(&error_log_tid, NULL, ErrorLogThread, NULL);
    if (ret) {
        fprintf(stderr, "Error:unable to create thread, %d\n", ret);
        exit(-1);
    }

    pthread_join(debug_log_tid, NULL);
    pthread_join(info_log_tid, NULL);
    pthread_join(waring_log_tid, NULL);
    pthread_join(error_log_tid, NULL);
    AeCloseLogFile();
    gettimeofday(&end, NULL);
    timer = 1000000 * (end.tv_sec - start.tv_sec) + end.tv_usec - start.tv_usec;
    LogInfo("time consuming %ld us\n", timer);
    return 0;
}
