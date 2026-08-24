/*
 * lfs_port.c
 *
 * Created on: Jul 16, 2026
 * Author: Gangadhar S
 *
 * Purpose:
 * This file connects LittleFS with the W25N01 SPI NAND Flash.
 *
 * LittleFS does not know how to communicate with our Flash.
 * Therefore, LittleFS calls these callback functions:
 *
 *      lfs_read()
 *      lfs_prog()
 *      lfs_erase()
 *      lfs_sync()
 *
 * These functions then use the W25N01 driver to access
 * the actual NAND Flash.
 */


#include "lfs_port.h"
#include "w25n01.h"


/*
 * LittleFS file-system object.
 *
 * This contains the current state of the LittleFS file system.
 */
lfs_t lfs;


/*
 * LittleFS configuration structure.
 *
 * This tells LittleFS:
 *
 *      - How to read
 *      - How to write
 *      - How to erase
 *      - Flash size
 *      - Page size
 *      - Block size
 *      - Cache size
 *      - Wear leveling settings
 */
struct lfs_config cfg;


/*----------------------------------------------------------
 * LittleFS Callback Function Declarations
 *
 * LittleFS will internally call these functions whenever
 * it needs to access the Flash.
 *----------------------------------------------------------*/

static int lfs_read(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    void *buffer,
                    lfs_size_t size);


static int lfs_prog(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    const void *buffer,
                    lfs_size_t size);


static int lfs_erase(const struct lfs_config *c,
                     lfs_block_t block);


static int lfs_sync(const struct lfs_config *c);


/*==========================================================
 * READ CALLBACK
 *==========================================================
 *
 * LittleFS calls this function when it wants to READ data
 * from the Flash.
 *
 * Parameters:
 *
 * block  -> LittleFS logical block number
 * off    -> Offset inside that block
 * buffer -> RAM buffer where data must be stored
 * size   -> Number of bytes to read
 *
 * Return:
 *      0  -> Success
 *     -1  -> Error
 *==========================================================*/

static int lfs_read(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    void *buffer,
                    lfs_size_t size)
{
    /*
     * We are not using the configuration pointer here.
     *
     * Prevent compiler warning.
     */
    (void)c;


    /*
     * Convert generic void pointer into byte pointer.
     *
     * This allows us to move through the buffer
     * byte by byte.
     */
    uint8_t *buf = (uint8_t *)buffer;


    /*
     * Continue until all requested bytes are read.
     */
    while (size > 0)
    {

        /*
         * Calculate physical NAND page.
         *
         * One LittleFS block contains:
         *
         *      128 KB
         *
         * One NAND page contains:
         *
         *      2 KB
         *
         * Therefore:
         *
         *      128 KB / 2 KB = 64 pages
         *
         * So every LittleFS block contains 64 NAND pages.
         *
         * block * 64
         *      -> first NAND page belonging to the block
         *
         * off / 2048
         *      -> page number inside that block
         */
        uint32_t page = (block * 64) + (off / 2048);


        /*
         * Calculate the column position inside the NAND page.
         *
         * NAND page size = 2048 bytes.
         *
         * Example:
         *
         * off = 100
         *
         * column = 100
         *
         * Data starts from byte 100 of the page.
         */
        uint16_t column = off % 2048;


        /*
         * Calculate how many bytes are remaining
         * in the current NAND page.
         */
        uint16_t bytes = 2048 - column;


        /*
         * If LittleFS requested fewer bytes than what
         * remains in the page, only read the requested amount.
         */
        if (bytes > size)
        {
            bytes = size;
        }


        /*
         * STEP 1:
         *
         * Transfer the selected NAND page into
         * the W25N01 internal cache.
         *
         * Command used inside W25N01_PageRead():
         *
         *      0x13
         */
        if (W25N01_PageRead(page) != HAL_OK)
        {
            return -1;
        }


        /*
         * STEP 2:
         *
         * Read the requested bytes from the NAND
         * internal cache into the STM32 RAM buffer.
         *
         * Command used inside W25N01_ReadData():
         *
         *      0x0B
         */
        if (W25N01_ReadData(column, buf, bytes) != HAL_OK)
        {
            return -1;
        }


        /*
         * Move RAM buffer pointer forward.
         */
        buf += bytes;


        /*
         * Move Flash offset forward.
         */
        off += bytes;


        /*
         * Reduce number of remaining bytes.
         */
        size -= bytes;
    }


    /*
     * Read completed successfully.
     */
    return 0;
}


/*==========================================================
 * PROGRAM / WRITE CALLBACK
 *==========================================================
 *
 * LittleFS calls this function when it wants to WRITE
 * data into the Flash.
 *
 * Parameters:
 *
 * block  -> LittleFS logical block
 * off    -> Offset inside block
 * buffer -> Data stored in RAM
 * size   -> Number of bytes to write
 *
 * Return:
 *      0  -> Success
 *     -1  -> Error
 *==========================================================*/

static int lfs_prog(const struct lfs_config *c,
                    lfs_block_t block,
                    lfs_off_t off,
                    const void *buffer,
                    lfs_size_t size)
{
    /*
     * Configuration pointer is not used.
     */
    (void)c;


    /*
     * Convert input buffer to byte pointer.
     */
    const uint8_t *buf = (const uint8_t *)buffer;


    /*
     * Continue until all requested data is written.
     */
    while (size > 0)
    {

        /*
         * Calculate physical NAND page.
         *
         * Each LittleFS block contains 64 NAND pages.
         *
         * page =
         *
         *      block * 64
         *      +
         *      page offset inside block
         */
        uint32_t page = (block * 64) + (off / 2048);
        //converts files system block into NAND page

        /*
         * Find the starting byte inside the NAND page.
         */
        uint16_t column = off % 2048;
        //calculates starting byte inside that page

        /*
         * Calculate how much space remains
         * in the current page.
         */
        uint16_t bytes = 2048 - column;


        /*
         * Do not write more data than requested by LittleFS.
         */
        if (bytes > size)
        {
            bytes = size;
        }


        /*
         * STEP 1:
         *
         * Enable Flash programming.
         *
         * Command:
         *      0x06
         */
        if (W25N01_WriteEnable() != HAL_OK)
        {
            return -1;
        }


        /*
         * STEP 2:
         *
         * Load data into the NAND internal cache.
         *
         * Command:
         *      0x02
         *
         * This does NOT yet permanently write the data
         * into NAND memory.
         */
        if (W25N01_ProgramLoad(column,
                               (uint8_t *)buf,
                               bytes) != HAL_OK)
        {
            return -1;
        }


        /*
         * STEP 3:
         *
         * Program the cache contents into the actual
         * NAND Flash page.
         *
         * Command:
         *      0x10
         */
        if (W25N01_ProgramExecute(page) != HAL_OK)
        {
            return -1;
        }


        /*
         * Wait until NAND finishes programming.
         */
        W25N01_WaitBusy();


        /*
         * Move to next data in RAM.
         */
        buf += bytes;


        /*
         * Move to next Flash offset.
         */
        off += bytes;


        /*
         * Reduce remaining data size.
         */
        size -= bytes;
    }


    /*
     * Programming completed successfully.
     */
    return 0;
}


/*==========================================================
 * ERASE CALLBACK
 *==========================================================
 *
 * LittleFS calls this function when it needs to erase
 * a logical block.
 *
 * Important:
 *
 * LittleFS block size:
 *
 *      128 KB
 *
 * NAND page size:
 *
 *      2 KB
 *
 * Therefore:
 *
 *      128 KB / 2 KB = 64 pages
 *
 * One LittleFS block corresponds to one NAND erase block.
 *==========================================================*/

static int lfs_erase(const struct lfs_config *c,
                     lfs_block_t block)
{
    /*
     * Configuration pointer is not used.
     */
    (void)c;


    /*
     * Calculate the first page of this
     * physical NAND erase block.
     *
     * Each block contains 64 pages.
     */
    uint32_t page = block * 64;


    /*
     * Enable erase/program operation.
     */
    if (W25N01_WriteEnable() != HAL_OK)
    {
        return -1;
    }


    /*
     * Erase the NAND block.
     *
     * W25N01_BlockErase() uses command:
     *
     *      0xD8
     */
    W25N01_BlockErase(page);


    /*
     * Erase takes some time.
     *
     * Wait until NAND becomes ready.
     */
    W25N01_WaitBusy();


    /*
     * Erase completed successfully.
     */
    return 0;
}


/*==========================================================
 * SYNC CALLBACK
 *==========================================================
 *
 * LittleFS calls this function when it wants to make sure
 * all Flash operations have completed.
 *==========================================================*/

static int lfs_sync(const struct lfs_config *c)
{
    /*
     * Configuration pointer is not used.
     */
    (void)c;


    /*
     * Wait until NAND is no longer busy.
     */
    W25N01_WaitBusy();


    /*
     * Synchronization completed.
     */
    return 0;
}


/*==========================================================
 * LITTLEFS INITIALIZATION
 *==========================================================
 *
 * This function configures LittleFS so that it knows
 * how our W25N01 NAND Flash is organized.
 *==========================================================*/

int littlefs_init(void)
{
    /*
     * No user context is required.
     */
    cfg.context = NULL;


    /*------------------------------------------------------
     * Connect LittleFS with our Flash driver.
     *
     * Whenever LittleFS wants to:
     *
     *      READ  -> call lfs_read()
     *      WRITE -> call lfs_prog()
     *      ERASE -> call lfs_erase()
     *      SYNC  -> call lfs_sync()
     *------------------------------------------------------*/

    cfg.read  = lfs_read;
    cfg.prog  = lfs_prog;
    cfg.erase = lfs_erase;
    cfg.sync  = lfs_sync;


    /*------------------------------------------------------
     * NAND FLASH GEOMETRY
     *------------------------------------------------------*/

    /*
     * Minimum unit LittleFS can read.
     *
     * NAND page size = 2048 bytes.
     */
    cfg.read_size = 2048;


    /*
     * Minimum unit LittleFS can program.
     *
     * Here we are using the complete 2048-byte page.
     */
    cfg.prog_size = 2048;


    /*
     * Size of one LittleFS block.
     *
     *      128 KB = 128 * 1024 bytes
     *
     * W25N01 erase block size is 128 KB.
     */
    cfg.block_size = 128 * 1024;


    /*
     * Number of logical blocks available to LittleFS.
     *
     *      1024 blocks
     *
     * Total:
     *
     *      1024 × 128 KB
     *      = 128 MB
     */
    cfg.block_count = 1024;


    /*------------------------------------------------------
     * LITTLEFS CACHE
     *------------------------------------------------------*/

    /*
     * RAM cache used by LittleFS for read operations.
     *
     * 2048 bytes = one NAND page.
     */
    cfg.cache_size = 2048;


    /*
     * Lookahead bitmap size.
     *
     * LittleFS uses this to track which blocks are free
     * or already used.
     */
    cfg.lookahead_size = 128;


    /*------------------------------------------------------
     * METADATA SETTINGS
     *------------------------------------------------------*/

    /*
     * Let LittleFS decide when metadata should be compacted.
     */
    cfg.compact_thresh = 0;


    /*
     * Maximum metadata size.
     */
    cfg.metadata_max = 4096;


    /*
     * Disable inline file storage.
     */
    cfg.inline_max = 0;


    /*------------------------------------------------------
     * WEAR LEVELING
     *------------------------------------------------------*/

    /*
     * Number of erase cycles before LittleFS tries to
     * move data around to reduce wear on a block.
     *
     * This helps distribute erase operations across
     * the Flash.
     */
    cfg.block_cycles = 500;


    /*------------------------------------------------------
     * OPTIONAL RAM BUFFERS
     *
     * NULL means LittleFS will manage/use its internal
     * mechanism rather than using user-provided buffers.
     *------------------------------------------------------*/

    cfg.read_buffer = NULL;

    cfg.prog_buffer = NULL;

    cfg.lookahead_buffer = NULL;


    /*------------------------------------------------------
     * FILE SYSTEM LIMITS
     *------------------------------------------------------*/

    /*
     * Maximum file/directory name length.
     */
    cfg.name_max = 255;


    /*
     * 0 means use LittleFS default/maximum supported limit.
     */
    cfg.file_max = 0;


    /*
     * No custom file attributes.
     */
    cfg.attr_max = 0;


    /*
     * Configuration completed successfully.
     */
    return 0;
}
