#ifndef _LOG_H_
#define _LOG_H_

#include <stdint.h>

void log_init(void);
void log_process(void);

#define LOG_INFO(...)    NRF_LOG_INFO(__VA_ARGS__)
#define LOG_WARNING(...) NRF_LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)   NRF_LOG_ERROR(__VA_ARGS__)

#endif
