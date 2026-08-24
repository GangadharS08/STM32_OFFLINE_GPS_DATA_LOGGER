/*
 * w25n01.h
 *
 *  Created on: Jun 2, 2026
 *      Author: dell
 */

//#ifndef INC_W25N01_H_
//#define INC_W25N01_H_
//
//
//
//#endif /* INC_W25N01_H_ */
#ifndef W25N01_H
#define W25N01_H

#include "main.h"

void W25N01_Init(void);
void W25N01_ReadJEDECID(uint8_t *id);
HAL_StatusTypeDef W25N01_WriteEnable(void);
uint8_t W25N01_ReadRegister(uint8_t reg);
void W25N01_Reset(void);
uint8_t W25N01_GetFeature(uint8_t addr);
void W25N01_BlockErase(uint16_t pageAddr);
void W25N01_WaitBusy(void);
void W25N01_SetFeature(uint8_t reg, uint8_t value);
HAL_StatusTypeDef W25N01_ProgramLoad(uint16_t column,
                                     uint8_t *data,
                                     uint16_t length);
HAL_StatusTypeDef W25N01_ProgramExecute(uint16_t pageAddr);
HAL_StatusTypeDef W25N01_PageRead(uint16_t pageAddr);
HAL_StatusTypeDef W25N01_ReadData(uint16_t column,
                                  uint8_t *buffer,
                                  uint16_t length);
#endif
