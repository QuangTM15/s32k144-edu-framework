/**
 * @file sd_diskio.c
 * @brief FatFs disk I/O adapter for the EduFramework SD card device.
 */

#include "ff.h"
#include "diskio.h"
#include "sd_card.h"
#include <stddef.h>

/* Physical drive number used by the SD card. */
#define SD_DISK_DRIVE_NUMBER (0U)

/**
 * @brief Initialize the physical drive.
 *
 * The SD card itself is initialized by SD_Begin() before FatFs is mounted.
 *
 * @param pdrv Physical drive number.
 *
 * @return Disk status.
 */
DSTATUS disk_initialize(BYTE pdrv)
{
    DSTATUS status = STA_NOINIT;

    if ((SD_DISK_DRIVE_NUMBER == pdrv) &&
        (true == SD_IsInitialized()))
    {
        status = 0U;
    }

    return status;
}

/**
 * @brief Get the current status of the physical drive.
 *
 * @param pdrv Physical drive number.
 *
 * @return Disk status.
 */
DSTATUS disk_status(BYTE pdrv)
{
    DSTATUS status = STA_NOINIT;

    if ((SD_DISK_DRIVE_NUMBER == pdrv) &&
        (true == SD_IsInitialized()))
    {
        status = 0U;
    }

    return status;
}

/**
 * @brief Read one or more sectors from the SD card.
 *
 * @param pdrv Physical drive number.
 * @param buff Destination buffer.
 * @param sector First sector number.
 * @param count Number of sectors to read.
 *
 * @return Disk operation result.
 */
DRESULT disk_read(BYTE pdrv,
                  BYTE *buff,
                  LBA_t sector,
                  UINT count)
{
    DRESULT result = RES_PARERR;
    UINT index = 0U;
    bool success = true;

    if ((SD_DISK_DRIVE_NUMBER == pdrv) &&
        (NULL != buff) &&
        (0U < count))
    {
        if (true == SD_IsInitialized())
        {
            while ((index < count) && (true == success))
            {
                success = SD_ReadBlock(
                    (uint32_t)(sector + (LBA_t)index),
                    &buff[index * SD_BLOCK_SIZE]);

                index++;
            }

            if (true == success)
            {
                result = RES_OK;
            }
            else
            {
                result = RES_ERROR;
            }
        }
        else
        {
            result = RES_NOTRDY;
        }
    }

    return result;
}

#if FF_FS_READONLY == 0

/**
 * @brief Write one or more sectors to the SD card.
 *
 * @param pdrv Physical drive number.
 * @param buff Source buffer.
 * @param sector First sector number.
 * @param count Number of sectors to write.
 *
 * @return Disk operation result.
 */
DRESULT disk_write(BYTE pdrv,
                   const BYTE *buff,
                   LBA_t sector,
                   UINT count)
{
    DRESULT result = RES_PARERR;
    UINT index = 0U;
    bool success = true;

    if ((SD_DISK_DRIVE_NUMBER == pdrv) &&
        (NULL != buff) &&
        (0U < count))
    {
        if (true == SD_IsInitialized())
        {
            while ((index < count) && (true == success))
            {
                success = SD_WriteBlock(
                    (uint32_t)(sector + (LBA_t)index),
                    &buff[index * SD_BLOCK_SIZE]);

                index++;
            }

            if (true == success)
            {
                result = RES_OK;
            }
            else
            {
                result = RES_ERROR;
            }
        }
        else
        {
            result = RES_NOTRDY;
        }
    }

    return result;
}

#endif

/**
 * @brief Perform a miscellaneous disk control operation.
 *
 * @param pdrv Physical drive number.
 * @param cmd Control command.
 * @param buff Command-dependent buffer.
 *
 * @return Disk operation result.
 */
DRESULT disk_ioctl(BYTE pdrv,
                   BYTE cmd,
                   void *buff)
{
    DRESULT result = RES_PARERR;

    (void)buff;

    if (SD_DISK_DRIVE_NUMBER == pdrv)
    {
        if (true == SD_IsInitialized())
        {
            if (CTRL_SYNC == cmd)
            {
                /*
                 * SD_WriteBlock() is blocking and waits until the card
                 * finishes the write operation, therefore no additional
                 * synchronization is required here.
                 */
                result = RES_OK;
            }
            else
            {
                result = RES_PARERR;
            }
        }
        else
        {
            result = RES_NOTRDY;
        }
    }

    return result;
}