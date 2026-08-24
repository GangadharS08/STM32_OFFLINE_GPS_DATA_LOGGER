/*
 * w25n01.c
 *
 * W25N01 SPI NAND Flash driver
 *
 * Created on: Jun 2, 2026
 * Author: dell
 */

#include "w25n01.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi1;


/*----------------------------------------------------------
 * Flash Chip Select (CS) control
 *
 * CS LOW  -> Flash is selected
 * CS HIGH -> Flash is deselected
 *----------------------------------------------------------*/

#define FLASH_CS_LOW() \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_RESET)

#define FLASH_CS_HIGH() \
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET)


/*----------------------------------------------------------
 * W25N01_Init()
 *
 * Initializes the control pins of the Flash.
 *
 * HOLD and WP must be HIGH during normal operation.
 * CS is kept HIGH when Flash is not being accessed.
 *----------------------------------------------------------*/

void W25N01_Init(void)
{
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET); // HOLD HIGH
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8, GPIO_PIN_SET); // WP HIGH
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_4, GPIO_PIN_SET); // CS HIGH
}


/*----------------------------------------------------------
 * W25N01_ReadJEDECID()
 *
 * Reads the Manufacturer ID and Device ID.
 *
 * Command:
 *     0x9F = JEDEC ID Read
 *
 * The Flash sends ID bytes back through MISO.
 *----------------------------------------------------------*/

void W25N01_ReadJEDECID(uint8_t *id)
{
    /*
     * First byte = command 0x9F
     * Remaining bytes = dummy bytes used to generate
     * SPI clock so that Flash can send its response.
     */
    uint8_t tx[5] = {0x9F, 0xFF, 0xFF, 0xFF, 0xFF};

    uint8_t rx[5] = {0};

    /* Select Flash */
    FLASH_CS_LOW();

    /* Send command and receive response */
    HAL_SPI_TransmitReceive(&hspi1,
                            tx,
                            rx,
                            5,
                            100);

    /* Deselect Flash */
    FLASH_CS_HIGH();

    /*
     * rx[0] contains data received while sending command.
     * Actual ID bytes start from rx[2].
     */
    id[0] = rx[2];     // Manufacturer ID
    id[1] = rx[3];     // Device ID byte 1
    id[2] = rx[4];     // Device ID byte 2
}


/*----------------------------------------------------------
 * W25N01_ReadRegister()
 *
 * Reads one register from the Flash.
 *
 * Command:
 *     0x0F = Get Feature / Read Register
 *
 * Format:
 *     0x0F
 *     Register address
 *     Dummy byte
 *     Dummy byte -> Register value received here
 *----------------------------------------------------------*/

uint8_t W25N01_ReadRegister(uint8_t reg)
{
    uint8_t tx[4] = {
        0x0F,       // Read Feature command
        reg,        // Register address
        0x00,       // Dummy byte
        0x00        // Dummy byte
    };

    uint8_t rx[4] = {0};

    FLASH_CS_LOW();

    /* Send command and receive register value */
    HAL_SPI_TransmitReceive(&hspi1,
                            tx,
                            rx,
                            4,
                            100);

    FLASH_CS_HIGH();

    /* Register data is received in the last byte */
    return rx[3];
}


/*----------------------------------------------------------
 * W25N01_WriteEnable()
 *
 * Enables write/erase operation inside the Flash.
 *
 * Command:
 *     0x06 = Write Enable
 *
 * This must be done before operations such as
 * program and erase.
 *----------------------------------------------------------*/

HAL_StatusTypeDef W25N01_WriteEnable(void)
{
    uint8_t cmd = 0x06;

    FLASH_CS_LOW();

    /* Send Write Enable command */
    HAL_StatusTypeDef status =
        HAL_SPI_Transmit(&hspi1,
                         &cmd,
                         1,
                         HAL_MAX_DELAY);

    FLASH_CS_HIGH();

    /*
     * Small delay after command.
     */
    HAL_Delay(1);

    return status;
}


/*----------------------------------------------------------
 * W25N01_Reset()
 *
 * Resets the Flash device.
 *
 * Command:
 *     0xFF = Reset
 *----------------------------------------------------------*/

void W25N01_Reset(void)
{
    uint8_t cmd = 0xFF;

    FLASH_CS_LOW();

    /* Send reset command */
    HAL_SPI_Transmit(&hspi1,
                     &cmd,
                     1,
                     100);

    FLASH_CS_HIGH();

    /*
     * Give Flash time to complete reset.
     */
    HAL_Delay(5);
}


/*----------------------------------------------------------
 * W25N01_GetFeature()
 *
 * Reads a Feature Register.
 *
 * Command:
 *     0x0F
 *
 * Example:
 *     0xC0 = Status Register
 *
 * Important status bits:
 *
 * Bit 0 -> OIP (Operation In Progress)
 *          1 = Flash busy
 *          0 = Flash ready
 *
 * Bit 3 -> ECC status / failure indication
 *----------------------------------------------------------*/

uint8_t W25N01_GetFeature(uint8_t addr)
{
    uint8_t tx[4] = {
        0x0F,       // Get Feature command
        addr,       // Register address
        0x00,       // Dummy
        0x00        // Dummy
    };

    uint8_t rx[4] = {0};

    FLASH_CS_LOW();

    /* Send command and receive register data */
    HAL_SPI_TransmitReceive(&hspi1,
                            tx,
                            rx,
                            4,
                            100);

    FLASH_CS_HIGH();

    /* Return the Feature Register value */
    return rx[3];
}


/*----------------------------------------------------------
 * W25N01_BlockErase()
 *
 * Erases a block of NAND Flash.
 *
 * Command:
 *     0xD8 = Block Erase
 *
 * pageAddr identifies the location to erase.
 *
 * IMPORTANT:
 * Write Enable must be performed before erase.
 *----------------------------------------------------------*/

void W25N01_BlockErase(uint16_t pageAddr)
{
    uint8_t tx[4];

    /*
     * Erase command
     */
    tx[0] = 0xD8;

    /*
     * Address bytes
     */
    tx[1] = (pageAddr >> 16) & 0xFF;
    tx[2] = (pageAddr >> 8) & 0xFF;
    tx[3] = pageAddr & 0xFF;

    FLASH_CS_LOW();

    /* Send erase command and address */
    HAL_SPI_Transmit(&hspi1,
                     tx,
                     4,
                     HAL_MAX_DELAY);

    FLASH_CS_HIGH();
}


/*----------------------------------------------------------
 * W25N01_WaitBusy()
 *
 * Waits until Flash finishes its internal operation.
 *
 * Status Register:
 *     Address = 0xC0
 *
 * Bit 0:
 *     1 -> Busy
 *     0 -> Ready
 *
 * Used after:
 *     - Erase
 *     - Program Execute
 *     - Page Read
 *----------------------------------------------------------*/

void W25N01_WaitBusy(void)
{
    /*
     * Keep checking status register while
     * operation is in progress.
     */
    while (W25N01_GetFeature(0xC0) & 0x01)
    {
        HAL_Delay(1);
    }

    /*
     * When bit 0 becomes 0, Flash is ready.
     */
}


/*----------------------------------------------------------
 * W25N01_SetFeature()
 *
 * Writes a value into a Feature Register.
 *
 * Command:
 *     0x1F = Set Feature
 *
 * Format:
 *     0x1F
 *     Register address
 *     Data
 *----------------------------------------------------------*/

void W25N01_SetFeature(uint8_t reg, uint8_t value)
{
    uint8_t tx[3];

    tx[0] = 0x1F;    // Set Feature command
    tx[1] = reg;     // Register address
    tx[2] = value;   // Value to write

    FLASH_CS_LOW();

    /* Send command, register address and value */
    HAL_SPI_Transmit(&hspi1,
                     tx,
                     3,
                     HAL_MAX_DELAY);

    FLASH_CS_HIGH();

    HAL_Delay(1);
}


/*----------------------------------------------------------
 * W25N01_ProgramLoad()
 *
 * Loads data into the NAND Flash cache/register.
 *
 * Command:
 *     0x02 = Program Load
 *
 * IMPORTANT:
 *
 * Program Load does NOT actually write the data
 * permanently into the NAND memory.
 *
 * It first loads the data into the internal cache.
 *
 * Later Program Execute (0x10) is required to
 * actually program the page.
 *
 * Flow:
 *
 *     Write Enable
 *          ↓
 *     Program Load
 *          ↓
 *     Program Execute
 *          ↓
 *     Wait Busy
 *----------------------------------------------------------*/

HAL_StatusTypeDef W25N01_ProgramLoad(uint16_t column,
                                      uint8_t *data,
                                      uint16_t length)
{
    /*
     * Enable write operation before programming.
     */
    W25N01_WriteEnable();

    uint8_t header[3];

    HAL_StatusTypeDef status;


    /*
     * Program Load command
     */
    header[0] = 0x02;

    /*
     * Column address tells Flash where inside
     * the page the data should start.
     */
    header[1] = (column >> 8) & 0xFF;
    header[2] = column & 0xFF;


    /* Select Flash */
    FLASH_CS_LOW();


    /*
     * Send:
     *
     * 0x02
     * Column address MSB
     * Column address LSB
     */
    status = HAL_SPI_Transmit(&hspi1,
                              header,
                              3,
                              HAL_MAX_DELAY);

    /*
     * Check whether command transmission succeeded.
     */
    if(status != HAL_OK)
    {
        FLASH_CS_HIGH();

        return status;
    }


    /*
     * Send actual data to Flash cache.
     */
    status = HAL_SPI_Transmit(&hspi1,
                              data,
                              length,
                              HAL_MAX_DELAY);


    /* Deselect Flash */
    FLASH_CS_HIGH();


    return status;
}


/*----------------------------------------------------------
 * W25N01_ProgramExecute()
 *
 * Actually programs the previously loaded data
 * into the NAND Flash page.
 *
 * Command:
 *     0x10 = Program Execute
 *
 * Program Load only puts data into cache.
 *
 * Program Execute writes that cache data
 * into NAND memory.
 *----------------------------------------------------------*/

HAL_StatusTypeDef W25N01_ProgramExecute(uint16_t pageAddr)
{
    uint8_t tx[4];

    /*
     * Program Execute command
     */
    tx[0] = 0x10;

    /*
     * Page address
     */
    tx[1] = (pageAddr >> 16) & 0xFF;
    tx[2] = (pageAddr >> 8) & 0xFF;
    tx[3] = pageAddr & 0xFF;


    FLASH_CS_LOW();


    /*
     * Send Program Execute command
     * and page address.
     */
    HAL_StatusTypeDef status =
        HAL_SPI_Transmit(&hspi1,
                         tx,
                         4,
                         HAL_MAX_DELAY);


    FLASH_CS_HIGH();


    /*
     * Program operation takes some time.
     *
     * Wait until Flash is no longer busy.
     */
    W25N01_WaitBusy();


    /*
     * Read Status Register.
     */
    uint8_t sr = W25N01_GetFeature(0xC0);


    /*
     * Check program failure bit.
     */
    if(sr & 0x08)
    {
        printf("PROGRAM FAIL BIT SET\r\n");
    }


    return status;
}


/*----------------------------------------------------------
 * W25N01_PageRead()
 *
 * Transfers a page from NAND memory into the
 * Flash internal cache.
 *
 * Command:
 *     0x13 = Page Read
 *
 * Important:
 *
 * NAND Flash cannot directly read data from the
 * NAND array.
 *
 * First:
 *
 *     NAND Page
 *          ↓
 *     Internal Cache
 *
 * Then W25N01_ReadData() reads the cache.
 *----------------------------------------------------------*/

HAL_StatusTypeDef W25N01_PageRead(uint16_t pageAddr)
{
    uint8_t tx[4];

    /*
     * Page Read command
     */
    tx[0] = 0x13;

    /*
     * Page address
     */
    tx[1] = (pageAddr >> 16) & 0xFF;
    tx[2] = (pageAddr >> 8) & 0xFF;
    tx[3] = pageAddr & 0xFF;


    FLASH_CS_LOW();


    /*
     * Send Page Read command and address.
     */
    HAL_StatusTypeDef status =
        HAL_SPI_Transmit(&hspi1,
                         tx,
                         4,
                         HAL_MAX_DELAY);


    FLASH_CS_HIGH();


    /*
     * Flash internally transfers the requested
     * page into its cache.
     *
     * Wait until this operation is finished.
     */
    W25N01_WaitBusy();


    return status;
}


/*----------------------------------------------------------
 * W25N01_ReadData()
 *
 * Reads data from the Flash internal cache.
 *
 * Command:
 *     0x0B = Fast Read
 *
 * Flow:
 *
 *     Page Read (0x13)
 *          ↓
 *     NAND page copied to cache
 *          ↓
 *     Fast Read (0x0B)
 *          ↓
 *     MCU receives data
 *----------------------------------------------------------*/

HAL_StatusTypeDef W25N01_ReadData(uint16_t column,
                                  uint8_t *buffer,
                                  uint16_t length)
{
    uint8_t cmd[4];

    HAL_StatusTypeDef status;


    /*
     * Fast Read command
     */
    cmd[0] = 0x0B;

    /*
     * Column address
     */
    cmd[1] = (column >> 8) & 0xFF;
    cmd[2] = column & 0xFF;

    /*
     * Dummy byte required before reading data.
     */
    cmd[3] = 0x00;


    FLASH_CS_LOW();


    /*
     * Send command + column address + dummy byte.
     */
    status = HAL_SPI_Transmit(&hspi1,
                             cmd,
                             4,
                             HAL_MAX_DELAY);


    /*
     * Check SPI transmission.
     */
    if(status != HAL_OK)
    {
        FLASH_CS_HIGH();

        return status;
    }


    /*
     * Read requested number of bytes.
     *
     * SPI is full duplex.
     *
     * To receive data from Flash, we send
     * dummy 0xFF bytes to generate the clock.
     */
    for(uint16_t i = 0; i < length; i++)
    {
        uint8_t dummy = 0xFF;

        status = HAL_SPI_TransmitReceive(&hspi1,
                                         &dummy,
                                         &buffer[i],
                                         1,
                                         HAL_MAX_DELAY);


        /*
         * Check if SPI transfer was successful.
         */
        if(status != HAL_OK)
        {
            FLASH_CS_HIGH();

            return status;
        }
    }


    /*
     * Deselect Flash.
     */
    FLASH_CS_HIGH();


    return HAL_OK;
}
