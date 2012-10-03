#ifndef __LOG_H__12346362342
#define __LOG_H__12346362342

#ifdef __ANDROID__
#include <android/log.h>

#define LOG_TAG "freeserf"

#define LOGV(...)  __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG ,__VA_ARGS__)
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...)  __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#else

#include <stdio.h>

#define LOGV(...) do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while ( 0 )
#define LOGD(...) do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while ( 0 )
#define LOGI(...) do { fprintf(stdout, __VA_ARGS__); fprintf(stdout, "\n"); } while ( 0 )
#define LOGW(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while ( 0 )
#define LOGE(...) do { fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while ( 0 )

#endif

#endif // __LOG_H__12346362342