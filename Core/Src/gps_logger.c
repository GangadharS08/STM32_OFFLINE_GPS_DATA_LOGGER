/*
 * gps_logger.c
 *
 *  Created on: Jul 17, 2026
 *      Author: Gangadhar S
 */

#include "gps_logger.h"
#include "lfs.h"
#include "lfs_port.h"

#include <stdio.h>
#include <string.h>

#define MAX_FILES       20
#define MAX_FILENAME    32

char fileList[MAX_FILES][MAX_FILENAME];
uint8_t fileCount = 0;

extern char gps_date[16];

void GPS_Log(char *text)
{
	 if(strcmp(gps_date, "NA") == 0)
	    {
	        return;
	    }

	 lfs_file_t file;

    char filename[20];
    sprintf(filename,"%s.txt",gps_date);

    if (lfs_file_open(&lfs,
                      &file,
                      filename,
                      LFS_O_CREAT |
                      LFS_O_WRONLY |
                      LFS_O_APPEND) < 0)
    {
        printf("File Open Failed\r\n");
        return;
    }

    int ret;

    ret = lfs_file_write(&lfs,
                         &file,
                         text,
                         strlen(text));

    if (ret < 0)
    {
        printf("Write Failed\r\n");
    }

    ret = lfs_file_write(&lfs,
                         &file,
                         "\r\n",
                         2);

    if (ret < 0)
    {
        printf("New Line Write Failed\r\n");
    }

    ret = lfs_file_close(&lfs,
                         &file);

    if (ret < 0)
    {
        printf("File Close Failed\r\n");
    }
}






int GPS_ReadLog(const char *filename)
{
    lfs_file_t file;
    char buffer[128];
    int bytesRead;

    printf("\r\nOpening %s...\r\n", filename);

    if (lfs_file_open(&lfs,
                      &file,
                      filename,
                      LFS_O_RDONLY) < 0)
    {
        printf("Failed to open file\r\n");
        return 0;
    }

    while ((bytesRead = lfs_file_read(&lfs,
                                      &file,
                                      buffer,
                                      sizeof(buffer)-1)) > 0)
    {
        buffer[bytesRead] = '\0';

        printf("%s", buffer);
    }

    lfs_file_close(&lfs, &file);

    printf("\r\nRead Complete\r\n");

    return 1;
}





int GPS_DownloadLog(const char *filename)
{
    lfs_file_t file;
    char buffer[128];
    int bytesRead;

    if (lfs_file_open(&lfs,
                      &file,
                      filename,
                      LFS_O_RDONLY) < 0)
    {
        printf("DOWNLOAD_FAILED\r\n");
        return 0;
    }

    printf("BEGIN_FILE:%s\r\n", filename);

    while ((bytesRead = lfs_file_read(&lfs,
                                      &file,
                                      buffer,
                                      sizeof(buffer) - 1)) > 0)
    {
        buffer[bytesRead] = '\0';
        printf("%s", buffer);
    }

    lfs_file_close(&lfs, &file);

    printf("\r\nEND_FILE\r\n");

    return 1;
}








void GPS_ScanFiles(void)
{
    lfs_dir_t dir;
    struct lfs_info info;

    fileCount = 0;

    if (lfs_dir_open(&lfs, &dir, "/") != 0)
    {
        return;
    }

    while (lfs_dir_read(&lfs, &dir, &info) > 0)
    {
        if (info.type == LFS_TYPE_REG)
        {
            if (fileCount < MAX_FILES)
            {
                strcpy(fileList[fileCount], info.name);
                fileCount++;
            }
        }
    }

    lfs_dir_close(&lfs, &dir);
}








void GPS_ListFiles(void)
{
    GPS_ScanFiles();

    printf("\r\n=========== FILES ===========\r\n");

    for(uint8_t i = 0; i < fileCount; i++)
    {
        printf("%d. %s\r\n", i + 1, fileList[i]);
    }

    printf("=============================\r\n");
}








int GPS_FileExists(const char *filename)
{
    struct lfs_info info;

    if (lfs_stat(&lfs, filename, &info) == 0)
    {
        return 1;   // File exists
    }

    return 0;       // File does not exist
}









int GPS_DeleteLog(const char *filename)
{
    int ret;

    ret = lfs_remove(&lfs, filename);

    if(ret == 0)
    {
        printf("File   : %s\r\n", filename);
        printf("Status : Deleted Successfully\r\n");

        return 1;
    }
    else
    {
        printf("File   : %s\r\n", filename);
        printf("Status : Delete Failed\r\n");
\
        return 0;
    }
}












int GPS_GetFileSize(const char *filename)
{
    struct lfs_info info;

    if (lfs_stat(&lfs, filename, &info) < 0)
    {
        printf("%s Not Found\r\n", filename);
        return -1;
    }

    printf("\r\n%s\r\n", filename);
    printf("File Size : %lu Bytes\r\n", (unsigned long)info.size);

    return info.size;
}






void GPS_SendAllLogs(void)
{
    GPS_ScanFiles();

    printf("\r\n=========== DOWNLOAD START ===========\r\n");

    for(uint8_t i = 0; i < fileCount; i++)
    {
        printf("BEGIN_FILE:%s\r\n", fileList[i]);

        GPS_ReadLog(fileList[i]);

        printf("\r\nEND_FILE\r\n");
    }

    printf("DOWNLOAD_COMPLETE\r\n");
}
