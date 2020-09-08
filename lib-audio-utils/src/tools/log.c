#include "log.h"
#include <pthread.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

#ifdef __ANDROID__
#include <android/log.h>
#define TAG "ap-log"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGF(...) __android_log_print(ANDROID_LOG_FATAL, TAG, __VA_ARGS__)
#endif

#define MAX_BUFFER_SIZE 400
#define FFMIN(a, b) ((a) > (b) ? (b) : (a))

#define NONE "\e[0m"
#define BLACK "\e[0;30m"
#define L_BLACK "\e[1;30m"
#define RED "\e[0;31m"
#define L_RED "\e[1;31m"
#define GREEN "\e[0;32m"
#define L_GREEN "\e[1;32m"
#define YELLOW "\e[0;33m"
#define L_YELLOW "\e[1;33m"
#define BLUE "\e[0;34m"
#define L_BLUE "\e[1;34m"
#define PURPLE "\e[0;35m"
#define L_PURPLE "\e[1;35m"
#define CYAN "\e[0;36m"
#define L_CYAN "\e[1;36m"
#define GRAY "\e[0;37m"
#define WHITE "\e[1;37m"

static LogMode self_log_mode = LOG_MODE_NONE;
static LogLevel self_log_level = LOG_LEVEL_INFO;
static FILE *self_log_file = NULL;
static char self_log_buffer[MAX_BUFFER_SIZE];
static pthread_mutex_t self_log_lock = PTHREAD_MUTEX_INITIALIZER;

static char GetLeveFlag(const LogLevel level) {
    switch (level) {
        case LOG_LEVEL_TRACE:
            return 'T';
        case LOG_LEVEL_DEBUG:
            return 'D';
        case LOG_LEVEL_VERBOSE:
            return 'V';
        case LOG_LEVEL_INFO:
            return 'I';
        case LOG_LEVEL_WARNING:
            return 'W';
        case LOG_LEVEL_ERROR:
            return 'E';
        case LOG_LEVEL_FATAL:
            return 'F';
        case LOG_LEVEL_PANIC:
            return 'P';
        case LOG_LEVEL_QUIET:
            return 'Q';
        default:
            break;
    }
    return 'E';
}

static void UpdateBuffer(const LogLevel level, const char *filename,
                         const int line, const char *format, va_list args) {
    char *basename = strrchr(filename, '/') + 1;
    size_t len = 0;

    if (LOG_MODE_ANDROID == self_log_mode) {
        len += snprintf(self_log_buffer + len, MAX_BUFFER_SIZE - len,
                        "/%s:%d:", basename, line);
    } else {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
#ifdef __linux__
        len += strftime(self_log_buffer, sizeof(self_log_buffer), "%Y/%m/%d %T",
                        localtime(&ts.tv_sec));
        len +=
            snprintf(self_log_buffer + len, MAX_BUFFER_SIZE - len,
                     ".%06ld %d-%ld/%c/%s:%d:", ts.tv_nsec / 1000, getpid(),
                     syscall(__NR_gettid), GetLeveFlag(level), basename, line);
#elif defined(__APPLE__)
        uint64_t tid;
        pthread_threadid_np(NULL, &tid);
        len = strftime(self_log_buffer, sizeof(self_log_buffer), "%Y/%m/%d %T",
                       localtime(&ts.tv_sec));
        len += snprintf(self_log_buffer + len, MAX_BUFFER_SIZE - len,
                        ".%06ld %d-%llu/%c/%s:%d:", ts.tv_nsec / 1000, getpid(),
                        tid, GetLeveFlag(level), basename, line);
#endif
    }
    vsnprintf(self_log_buffer + len, MAX_BUFFER_SIZE - len, format, args);
}

static void Log2File() {
    if (NULL == self_log_file) return;
    fprintf(self_log_file, "%s", self_log_buffer);
}

static void Log2Screen(const LogLevel level) {
    switch (level) {
        case LOG_LEVEL_VERBOSE:
            printf(L_BLUE "%s" NONE, self_log_buffer);
            break;
        case LOG_LEVEL_DEBUG:
            printf(L_BLUE "%s" NONE, self_log_buffer);
            break;
        case LOG_LEVEL_INFO:
            printf(L_GREEN "%s" NONE, self_log_buffer);
            break;
        case LOG_LEVEL_WARNING:
            printf(L_YELLOW "%s" NONE, self_log_buffer);
            break;
        case LOG_LEVEL_ERROR:
            printf(L_RED "%s" NONE, self_log_buffer);
            break;
        case LOG_LEVEL_FATAL:
            printf(L_RED "%s" NONE, self_log_buffer);
            break;
        default:
            printf(L_BLUE "%s" NONE, self_log_buffer);
            break;
    }
}

#ifdef __ANDROID__
static void Log2Android(const LogLevel level) {
    switch (level) {
        case LOG_LEVEL_VERBOSE:
            ALOGV("%s", self_log_buffer);
            break;
        case LOG_LEVEL_DEBUG:
            ALOGD("%s", self_log_buffer);
            break;
        case LOG_LEVEL_INFO:
            ALOGI("%s", self_log_buffer);
            break;
        case LOG_LEVEL_WARNING:
            ALOGW("%s", self_log_buffer);
            break;
        case LOG_LEVEL_ERROR:
            ALOGE("%s", self_log_buffer);
            break;
        case LOG_LEVEL_FATAL:
            ALOGF("%s", self_log_buffer);
            break;
        default:
            ALOGV("%s", self_log_buffer);
            break;
    }
}
#endif

void AePrintLog(const LogLevel level, const char *filename, const int line,
                const char *format, ...) {
    if (level < self_log_level || LOG_MODE_NONE == self_log_mode) return;

    pthread_mutex_lock(&self_log_lock);
    va_list args;
    va_start(args, format);

    UpdateBuffer(level, filename, line, format, args);
    switch (self_log_mode) {
        case LOG_MODE_FILE:
            Log2File();
            break;
        case LOG_MODE_ANDROID:
#ifdef __ANDROID__
            Log2Android(level);
#endif
            break;
        case LOG_MODE_SCREEN:
            Log2Screen(level);
            break;
        default:
            break;
    }

    va_end(args);
    pthread_mutex_unlock(&self_log_lock);
    if (level >= LOG_LEVEL_FATAL) {
        abort();
    }
}

void AeCloseLogFile() {
    if (self_log_file && self_log_file != stderr) {
        fclose(self_log_file);
        self_log_file = NULL;
    }
}

int AeSetLogPath(const char *path) {
    int ret = 0;

    pthread_mutex_lock(&self_log_lock);

    if (self_log_file && self_log_file != stderr) {
        fclose(self_log_file);
        self_log_file = NULL;
    }
    self_log_file = fopen(path, "wb");

    if (!self_log_file) {
        ret = ERROR_OPEN_LOG_FILE;
        self_log_file = stderr;
    }
    if (setvbuf(self_log_file, NULL, _IONBF, 0) != 0) ret = ERROR_OPEN_LOG_FILE;

    pthread_mutex_unlock(&self_log_lock);
    return ret;
}

void AeSetLogMode(const LogMode mode) {
    pthread_mutex_lock(&self_log_lock);
    self_log_mode = mode;
    pthread_mutex_unlock(&self_log_lock);
}

void AeSetLogLevel(const LogLevel level) {
    pthread_mutex_lock(&self_log_lock);
    self_log_level = level;
    pthread_mutex_unlock(&self_log_lock);
}