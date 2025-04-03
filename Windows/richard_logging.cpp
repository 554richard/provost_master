#include "richard_logging.h"

void setup_log(char* directory_path)
{
	char logpath[300];

	strcpy_s(logpath, sizeof(logpath), directory_path);
	strcat_s(logpath, sizeof(logpath), "SimmilP3D.log");

	g_fp_log = fopen(logpath, "w");
	if (g_fp_log == NULL)
	{
		printf("Failed to open log file: .\\SimmilP3D.log\n");
		return;
	}
}

void close_log(void)
{
	fclose(g_fp_log);

}

void my_log(char* log_msg, int whereitgoes)
{

//	printf(log_msg); //Have it go to the screen for initial debugging
	if ((whereitgoes & 1) == LOG_TO_CONSOLE)
	{
		printf(log_msg);
	}

	if ((whereitgoes & 2) == LOG_TO_FILE)
	{
		fprintf(g_fp_log, log_msg); 
	}
}
