
#ifndef __DRV_UDISK_H_
#define __DRV_UDISK_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>


#include "fatfs.h"
#include "ff.h"
#include "ffconf.h"
#include "usbh_conf.h"
#include "usb_host.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/
typedef enum 
	{
    UDISK_OK = 0,
    UDISK_ERROR = 1,
} udisk_state_t;


/****************************************************************************/
/*							Exported Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions								*/
/****************************************************************************/
uint8_t  drv_udisk_init(void);
uint8_t drv_udisk_host_init(void);
uint8_t drv_udisk_host_status(void);
udisk_state_t drv_udisk_file_sys_mount(void);
uint32_t drv_udisk_get_file_sys_status(void);
udisk_state_t drv_udisk_file_open(FIL *p_file, const char *p_filename, uint8_t mode);
udisk_state_t drv_udisk_file_close(FIL *p_file);
udisk_state_t drv_udisk_file_write(FIL *p_file, const void *p_data, uint32_t size);
udisk_state_t drv_udisk_file_read(FIL *p_file, void *p_data, uint32_t size);
udisk_state_t drv_udisk_file_sync(FIL *p_file);
udisk_state_t drv_udisk_file_rename(const char *old_filepath, const char *new_filepath);
uint32_t drv_udisk_get_time(void);
void drv_udisk_delay(uint32_t ms);


#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/