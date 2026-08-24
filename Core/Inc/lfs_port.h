/*
 * lfs_port.h
 *
 *  Created on: Jul 16, 2026
 *      Author: Gangadhar S
 */
#ifndef LFS_PORT_H
#define LFS_PORT_H

#include "lfs.h"

extern struct lfs_config cfg;
extern lfs_t lfs;

int littlefs_init(void);

#endif
