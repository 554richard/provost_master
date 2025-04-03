#pragma once

#include <stdio.h>
#include <Windows.h>
#include <stdint.h>

#define LOG_TO_CONSOLE 1
#define LOG_TO_FILE    2

#ifdef RHG_MAIN
FILE* g_fp_log;
uint8_t g_log_dest = LOG_TO_CONSOLE;
#else
extern FILE* g_fp_log;
extern uint8_t g_log_dest;
#endif

void setup_log(char* directory_path);
void close_log(void);
void my_log(char* log_msg, int whereitgoes);
