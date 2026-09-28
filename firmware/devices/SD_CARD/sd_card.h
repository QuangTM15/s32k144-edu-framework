/**
 * @file sd_card.h
 * @brief SD card and filesystem device library for EduFramework.
 *
 * @details
 * This module provides SD card storage and filesystem access through the
 * EduFramework SPI interface.
 *
 * Hardware connection:
 *
 * @code
 * SD Card SCK  -> EduFramework SPI_SCK
 * SD Card MOSI -> EduFramework SPI_SOUT
 * SD Card MISO -> EduFramework SPI_SIN
 * SD Card CS   -> User-selected digital GPIO used as software chip-select
 * @endcode
 *
 * The library provides:
 *
 * - SD card initialization in SPI mode.
 * - Automatic FAT filesystem mounting.
 * - File creation, reading, writing, and appending.
 * - File position and size operations.
 * - File and directory management.
 * - Directory browsing.
 * - Storage capacity information.
 * - Raw 512-byte block access.
 *
 * Filesystem implementation details are hidden from the application.
 * Applications interact only with the EduFramework SD API.
 */

#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdbool.h>
#include <stdint.h>

/* ========================================================================= */
/* Public Constants                                                          */
/* ========================================================================= */

/**
 * @brief Size of one SD card data block in bytes.
 */
#define SD_BLOCK_SIZE (512U)

/**
 * @brief Maximum supported file or directory name length.
 */
#define SD_MAX_NAME_LENGTH (255U)

/**
 * @brief Invalid file or directory handle value.
 */
#define SD_INVALID_HANDLE (0xFFU)

/* ========================================================================= */
/* File Modes                                                                */
/* ========================================================================= */

/**
 * @brief SD file access mode type.
 */
typedef uint8_t SD_FileMode_t;

/**
 * @brief Open an existing file for reading.
 */
#define SD_FILE_READ ((SD_FileMode_t)0U)

/**
 * @brief Create a file for writing.
 *
 * If the file already exists, its previous contents are overwritten.
 */
#define SD_FILE_WRITE ((SD_FileMode_t)1U)

/**
 * @brief Open a file for appending.
 *
 * Data is written at the end of the file. The file is created if it
 * does not already exist.
 */
#define SD_FILE_APPEND ((SD_FileMode_t)2U)

/**
 * @brief Open an existing file for both reading and writing.
 */
#define SD_FILE_READ_WRITE ((SD_FileMode_t)3U)

/* ========================================================================= */
/* Public Types                                                              */
/* ========================================================================= */

/**
 * @brief SD file handle.
 *
 * @details
 * This structure identifies an open file managed internally by the
 * SD library. Applications should not modify the handle value directly.
 */
typedef struct
{
    uint8_t handle;
} SD_File_t;

/**
 * @brief SD directory handle.
 *
 * @details
 * This structure identifies an open directory managed internally by the
 * SD library. Applications should not modify the handle value directly.
 */
typedef struct
{
    uint8_t handle;
} SD_Dir_t;

/**
 * @brief File or directory information.
 */
typedef struct
{
    char name[SD_MAX_NAME_LENGTH + 1U];
    uint32_t size;
    bool isDirectory;
    bool isReadOnly;
    bool isHidden;
} SD_FileInfo_t;

/* ========================================================================= */
/* Card and Filesystem API                                                   */
/* ========================================================================= */

/**
 * @brief Initialize the SD card and mount its filesystem.
 *
 * @details
 * The function initializes the SD card in SPI mode using the selected
 * software chip-select pin. After successful card initialization, the
 * FAT filesystem is mounted automatically.
 *
 * @param[in] csPin
 * Digital-capable logical pin connected to the SD card CS signal.
 *
 * @return Initialization state.
 *
 * @retval true
 * The SD card was initialized and the filesystem was mounted successfully.
 *
 * @retval false
 * Initialization or filesystem mounting failed.
 */
bool SD_Begin(uint8_t csPin);

/**
 * @brief Stop SD card operation.
 *
 * @details
 * Open filesystem resources are released and the filesystem is unmounted.
 * The SD card must be initialized again with SD_Begin() before further use.
 */
void SD_End(void);

/**
 * @brief Check whether the SD card is initialized.
 *
 * @return Initialization state.
 *
 * @retval true
 * The SD card is initialized.
 *
 * @retval false
 * The SD card is not initialized.
 */
bool SD_IsInitialized(void);

/**
 * @brief Check whether the filesystem is mounted.
 *
 * @return Mount state.
 *
 * @retval true
 * The filesystem is mounted.
 *
 * @retval false
 * The filesystem is not mounted.
 */
bool SD_IsMounted(void);

/**
 * @brief Mount the SD card filesystem.
 *
 * @return Mount state.
 *
 * @retval true
 * The filesystem was mounted successfully.
 *
 * @retval false
 * The card is not initialized or the filesystem could not be mounted.
 */
bool SD_Mount(void);

/**
 * @brief Unmount the SD card filesystem.
 *
 * @return Unmount state.
 *
 * @retval true
 * The filesystem was unmounted successfully.
 *
 * @retval false
 * The filesystem could not be unmounted.
 */
bool SD_Unmount(void);

/* ========================================================================= */
/* File API                                                                  */
/* ========================================================================= */

/**
 * @brief Open a file.
 *
 * @param[out] file
 * File handle to initialize.
 *
 * @param[in] path
 * Path of the file to open.
 *
 * @param[in] mode
 * File access mode.
 *
 * @return Open state.
 *
 * @retval true
 * The file was opened successfully.
 *
 * @retval false
 * The file could not be opened.
 */
bool SD_Open(SD_File_t *file,
             const char *path,
             SD_FileMode_t mode);

/**
 * @brief Close an open file.
 *
 * @param[in,out] file
 * File handle to close.
 *
 * @return Close state.
 *
 * @retval true
 * The file was closed successfully.
 *
 * @retval false
 * The file handle is invalid or the file could not be closed.
 */
bool SD_Close(SD_File_t *file);

/**
 * @brief Check whether a file handle represents an open file.
 *
 * @param[in] file
 * File handle to check.
 *
 * @return File state.
 *
 * @retval true
 * The file is open.
 *
 * @retval false
 * The file is not open or the handle is invalid.
 */
bool SD_IsOpen(const SD_File_t *file);

/* ========================================================================= */
/* File Read and Write API                                                   */
/* ========================================================================= */

/**
 * @brief Read data from an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[out] buffer
 * Destination buffer.
 *
 * @param[in] length
 * Maximum number of bytes to read.
 *
 * @param[out] bytesRead
 * Number of bytes actually read.
 *
 * @return Read state.
 *
 * @retval true
 * The read operation completed successfully.
 *
 * @retval false
 * The parameters are invalid or the read operation failed.
 */
bool SD_Read(SD_File_t *file,
             void *buffer,
             uint32_t length,
             uint32_t *bytesRead);

/**
 * @brief Write data to an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[in] buffer
 * Source buffer.
 *
 * @param[in] length
 * Number of bytes to write.
 *
 * @param[out] bytesWritten
 * Number of bytes actually written.
 *
 * @return Write state.
 *
 * @retval true
 * The write operation completed successfully.
 *
 * @retval false
 * The parameters are invalid or the write operation failed.
 */
bool SD_Write(SD_File_t *file,
              const void *buffer,
              uint32_t length,
              uint32_t *bytesWritten);

/**
 * @brief Read one byte from an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[out] data
 * Destination for the byte read from the file.
 *
 * @return Read state.
 */
bool SD_ReadByte(SD_File_t *file,
                 uint8_t *data);

/**
 * @brief Write one byte to an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[in] data
 * Byte to write.
 *
 * @return Write state.
 */
bool SD_WriteByte(SD_File_t *file,
                  uint8_t data);

/**
 * @brief Write a null-terminated string to an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[in] text
 * Null-terminated string to write.
 *
 * @return Write state.
 */
bool SD_WriteString(SD_File_t *file,
                    const char *text);

/**
 * @brief Write a string followed by a line ending to an open file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[in] text
 * Null-terminated string to write.
 *
 * @return Write state.
 */
bool SD_WriteLine(SD_File_t *file,
                  const char *text);

/* ========================================================================= */
/* File Position and Control API                                             */
/* ========================================================================= */

/**
 * @brief Move the current file position.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @param[in] position
 * Absolute byte position from the beginning of the file.
 *
 * @return Seek state.
 */
bool SD_Seek(SD_File_t *file,
             uint32_t position);

/**
 * @brief Move the file position to the beginning of the file.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @return Rewind state.
 */
bool SD_Rewind(SD_File_t *file);

/**
 * @brief Get the current file position.
 *
 * @param[in] file
 * Open file handle.
 *
 * @return Current byte position.
 *
 * @note
 * Zero is returned for an invalid file handle.
 */
uint32_t SD_Position(const SD_File_t *file);

/**
 * @brief Get the size of an open file.
 *
 * @param[in] file
 * Open file handle.
 *
 * @return File size in bytes.
 *
 * @note
 * Zero is returned for an invalid file handle.
 */
uint32_t SD_Size(const SD_File_t *file);

/**
 * @brief Get the number of unread bytes remaining in a file.
 *
 * @param[in] file
 * Open file handle.
 *
 * @return Number of bytes between the current position and end of file.
 *
 * @note
 * Zero is returned if the file is at the end or the handle is invalid.
 */
uint32_t SD_Available(const SD_File_t *file);

/**
 * @brief Flush pending file data to the SD card.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @return Flush state.
 */
bool SD_Flush(SD_File_t *file);

/**
 * @brief Truncate a file at its current position.
 *
 * @details
 * Data located after the current file position is removed.
 *
 * @param[in,out] file
 * Open file handle.
 *
 * @return Truncate state.
 */
bool SD_Truncate(SD_File_t *file);

/* ========================================================================= */
/* File and Directory Management API                                         */
/* ========================================================================= */

/**
 * @brief Check whether a file or directory exists.
 *
 * @param[in] path
 * File or directory path.
 *
 * @return Existence state.
 */
bool SD_Exists(const char *path);

/**
 * @brief Get information about a file or directory.
 *
 * @param[in] path
 * File or directory path.
 *
 * @param[out] info
 * Destination information structure.
 *
 * @return Operation state.
 */
bool SD_GetInfo(const char *path,
                SD_FileInfo_t *info);

/**
 * @brief Remove a file.
 *
 * @param[in] path
 * Path of the file to remove.
 *
 * @return Operation state.
 */
bool SD_Remove(const char *path);

/**
 * @brief Rename or move a file or directory.
 *
 * @param[in] oldPath
 * Existing path.
 *
 * @param[in] newPath
 * New path.
 *
 * @return Operation state.
 */
bool SD_Rename(const char *oldPath,
               const char *newPath);

/**
 * @brief Create a directory.
 *
 * @param[in] path
 * Directory path to create.
 *
 * @return Operation state.
 */
bool SD_Mkdir(const char *path);

/**
 * @brief Remove an empty directory.
 *
 * @param[in] path
 * Directory path to remove.
 *
 * @return Operation state.
 */
bool SD_Rmdir(const char *path);

/* ========================================================================= */
/* Directory Browsing API                                                    */
/* ========================================================================= */

/**
 * @brief Open a directory for browsing.
 *
 * @param[out] dir
 * Directory handle to initialize.
 *
 * @param[in] path
 * Directory path.
 *
 * @return Open state.
 */
bool SD_OpenDir(SD_Dir_t *dir,
                const char *path);

/**
 * @brief Read the next entry from an open directory.
 *
 * @param[in,out] dir
 * Open directory handle.
 *
 * @param[out] info
 * Information about the next directory entry.
 *
 * @return Entry state.
 *
 * @retval true
 * A directory entry was read successfully.
 *
 * @retval false
 * No more entries are available or an error occurred.
 */
bool SD_ReadDir(SD_Dir_t *dir,
                SD_FileInfo_t *info);

/**
 * @brief Rewind directory browsing to the first entry.
 *
 * @param[in,out] dir
 * Open directory handle.
 *
 * @return Rewind state.
 */
bool SD_RewindDir(SD_Dir_t *dir);

/**
 * @brief Close an open directory.
 *
 * @param[in,out] dir
 * Directory handle to close.
 *
 * @return Close state.
 */
bool SD_CloseDir(SD_Dir_t *dir);

/* ========================================================================= */
/* Storage Information API                                                   */
/* ========================================================================= */

/**
 * @brief Get filesystem storage capacity information.
 *
 * @param[out] totalBytes
 * Total filesystem capacity in bytes.
 *
 * @param[out] freeBytes
 * Available filesystem capacity in bytes.
 *
 * @return Operation state.
 */
bool SD_GetCapacity(uint64_t *totalBytes,
                    uint64_t *freeBytes);

/* ========================================================================= */
/* Raw Block Access API                                                      */
/* ========================================================================= */

/**
 * @brief Read one raw 512-byte block from the SD card.
 *
 * @param[in] block
 * Logical block number.
 *
 * @param[out] pBuffer
 * Destination buffer with space for at least SD_BLOCK_SIZE bytes.
 *
 * @return Read state.
 *
 * @retval true
 * The block was read successfully.
 *
 * @retval false
 * The card is not initialized, the buffer is invalid,
 * or communication failed.
 */
bool SD_ReadBlock(uint32_t block,
                  uint8_t *pBuffer);

/**
 * @brief Write one raw 512-byte block to the SD card.
 *
 * @param[in] block
 * Logical block number.
 *
 * @param[in] pBuffer
 * Source buffer containing SD_BLOCK_SIZE bytes.
 *
 * @return Write state.
 *
 * @retval true
 * The block was written successfully.
 *
 * @retval false
 * The card is not initialized, the buffer is invalid,
 * or communication failed.
 *
 * @warning
 * Raw block writes bypass the filesystem and can corrupt filesystem data
 * when used on blocks managed by the mounted filesystem.
 */
bool SD_WriteBlock(uint32_t block,
                   const uint8_t *pBuffer);

#endif /* SD_CARD_H */