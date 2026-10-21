#ifndef __DEBUG_H
#define __DEBUG_H

#include <stdio.h>
#include "esp_log.h"

#define DEBUG 1

#if DEBUG
    #define Debug(...) printf(__VA_ARGS__)
#else
    #define Debug(...)
#endif

#endif
