
#ifndef __DRV_UDISK_H_
#define __DRV_UDISK_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ff.h"
#include "ffconf.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/
typedef enum {
    UDISK_OK = 0,
    UDISK_ERROR = 1,
    UDISK_NOT_READY = 2,
    UDISK_BUSY = 3,
    UDISK_DISK_ERR = 4,
    UDISK_NO_DISK = 5,
    UDISK_QUEUE_FULL = 6
} udisk_state_t;


typedef enum {
	FR_OK = 0,				/* (0) Succeeded */
	FR_DISK_ERR,			/* (1) A hard error occurred in the low level disk I/O layer */
	FR_INT_ERR,				/* (2) Assertion failed */
	FR_NOT_READY,			/* (3) The physical drive cannot work */
	FR_NO_FILE,				/* (4) Could not find the file */
	FR_NO_PATH,				/* (5) Could not find the path */
	FR_INVALID_NAME,		/* (6) The path name format is invalid */
	FR_DENIED,				/* (7) Access denied due to prohibited access or directory full */
	FR_EXIST,				/* (8) Access denied due to prohibited access */
	FR_INVALID_OBJECT,		/* (9) The file/directory object is invalid */
	FR_WRITE_PROTECTED,		/* (10) The physical drive is write protected */
	FR_INVALID_DRIVE,		/* (11) The logical drive number is invalid */
	FR_NOT_ENABLED,			/* (12) The volume has no work area */
	FR_NO_FILESYSTEM,		/* (13) There is no valid FAT volume */
	FR_MKFS_ABORTED,		/* (14) The f_mkfs() aborted due to any problem */
	FR_TIMEOUT,				/* (15) Could not get a grant to access the volume within defined period */
	FR_LOCKED,				/* (16) The operation is rejected according to the file sharing policy */
	FR_NOT_ENOUGH_CORE,		/* (17) Buffer could not be allocated */
	FR_TOO_MANY_OPEN_FILES,	/* (18) Number of open files > FF_FS_LOCK */
	FR_INVALID_PARAMETER	/* (19) Given parameter is invalid */
} FRESULT;

/****************************************************************************/
/*							Exported Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions								*/
/****************************************************************************/
void drv_udisk_init(void);
udisk_state_t drv_udisk_file_open(FIL *p_file, const char *p_filename, uint8_t mode);
udisk_state_t drv_udisk_file_close(FIL *p_file);
udisk_state_t drv_udisk_file_write(FIL *p_file, const void *p_data, uint32_t size);
udisk_state_t drv_udisk_file_read(FIL *p_file, void *p_data, uint32_t size);
udisk_state_t drv_udisk_file_sync(FIL *p_file);
udisk_state_t drv_udisk_file_rename(const char *old_filepath, const char *new_filepath);
uint32_t drv_udisk_get_time(void);


#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/