#ifndef COMMON_H
#define COMMON_H
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <circle/bcmwatchdog.h>
#include "kernel.hpp"

#define CHECK_CALLW(call, msg) if ((err = call) != 0) {CKernel::kernel->LogW(TAG, msg ": %s (%d)", strerror(err), err); return 0;}
#define CHECK_CALLE(call, msg) if ((err = call) != 0) {CKernel::kernel->LogE(TAG, msg ": %s (%d)", strerror(err), err); return err;}
#define ESP_LOGD CKernel::kernel->LogD
#define ESP_LOGI CKernel::kernel->LogI
#define ESP_LOGW CKernel::kernel->LogW
#define ESP_LOGE CKernel::kernel->LogE
typedef int esp_err_t;

extern esp_err_t common_init(void);
extern void common_deinit(void);

#endif