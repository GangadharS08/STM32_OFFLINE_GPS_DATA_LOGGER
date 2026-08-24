/*
 * gps_logger.h
 *
 *  Created on: Jul 17, 2026
 *      Author: Gangadhar S
 */

#ifndef GPS_LOGGER_H
#define GPS_LOGGER_H

#include "main.h"      // <-- Add this line
// or #include <stdint.h>

void GPS_Log(char *text);
int GPS_ReadLog(const char *filename);
void GPS_ListFiles(void);
int GPS_FileExists(const char *filename);
int GPS_DeleteLog(const char *filename);
int GPS_GetFileSize(const char *filename);
void GPS_ScanFiles(void);
void GPS_SendAllLogs(void);
int GPS_DownloadLog(const char *filename);

#define MAX_FILES      20
#define MAX_FILENAME   32

extern char fileList[MAX_FILES][MAX_FILENAME];
extern uint8_t fileCount;

#endif
