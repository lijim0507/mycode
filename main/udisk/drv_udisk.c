/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "drv_udisk.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define UDISK_INIT_TIMEOUT_MS 5000 /* U盘初始化超时时间 */
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
extern volatile ApplicationTypeDef Appli_state; // 声明外部变量，表示USB应用状态

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/



uint8_t  drv_udisk_init(void)
{
    //usb主机驱动的初始化
    if(drv_udisk_host_init() != UDISK_OK)
    {
        return UDISK_ERROR;
    }

    //文件系统挂载
    if(drv_udisk_file_sys_mount() != UDISK_OK)
    {
        return UDISK_ERROR;
    }

    return UDISK_OK;
}

uint8_t drv_udisk_host_init(void)
{

    //usb主机驱动的初始化
    uint32_t start_tick = drv_udisk_get_time();

    while (drv_udisk_host_status() != UDISK_OK)
    {
        uint32_t elapsed = drv_udisk_get_time() - start_tick;
        if (elapsed >= UDISK_INIT_TIMEOUT_MS)
        {
            return UDISK_ERROR;
        }
        drv_udisk_delay(10);
    }

    return UDISK_OK;
}

uint8_t drv_udisk_host_status(void)
{
    if (Appli_state == APPLICATION_READY)
    {
        return UDISK_OK;
    }
    else
    {
        return UDISK_ERROR;
    }
}

uint8_t drv_udisk_file_sys_mount(void)
{
    if (f_mount(&USBHFatFS, USBHPath, 1) == FR_OK)
    {
        return UDISK_OK;
    }

    return UDISK_ERROR;
}


uint32_t drv_udisk_get_file_sys_status(void)
{
    return 0;
}


/**
 * @brief 打开文件
 * @param p_file 指向文件对象的指针
 * @param p_filename 文件名字符串
 * @param mode 打开模式
 * @return uint8_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
uint8_t drv_udisk_file_open(FIL *p_file, const char *p_filename, uint8_t mode)
{
    if ( p_filename == NULL)
    {
        return UDISK_ERROR;
    }

    if (f_open(p_file, p_filename, mode) == FR_OK)
    {
        return UDISK_OK;
    }

    return UDISK_ERROR;
}

/**
 * @brief 关闭文件
 * @param p_file 指向文件对象的指针
 * @return uint8_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
uint8_t drv_udisk_file_close(FIL *p_file)
{
    if (p_file == NULL)
    {
        return UDISK_ERROR;
    }

    if (f_close(p_file) == FR_OK)
    {
        p_file = NULL; // 将指针置空，防止悬空指针
        return UDISK_OK;
    }

	return UDISK_ERROR;
}

uint8_t drv_udisk_is_file_open(FIL *p_file) 
{
    return (p_file != NULL && p_file->obj.fs != NULL) ? 1 : 0;
}

/**
 * @brief 写入文件
 * @param p_file 指向文件对象的指针
 * @param p_data 指向数据的指针
 * @param size 数据大小
 * @return uint8_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
uint8_t drv_udisk_file_write(FIL *p_file, const void *p_data, uint32_t size)
{
    uint16_t written_size = 0;

     if (p_file == NULL || p_data == NULL || size == 0)
    {
        return UDISK_ERROR;
    }

    if (f_write(p_file, p_data, size, &written_size) != FR_OK)
    {
        return UDISK_ERROR;
    }

    if(written_size != size)
    {
        return UDISK_ERROR;
    }

    return UDISK_OK;
}

/**
 * @brief 读取文件
 * @param p_file 指向文件对象的指针
 * @param p_data 指向数据缓冲区的指针
 * @param size 要读取的数据大小
 * @return uint8_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
uint8_t drv_udisk_file_read(FIL *p_file, void *p_data, uint32_t size)
{

    uint16_t read_size = 0;

    if (p_file == NULL || p_data == NULL || size == 0)
    {
        return UDISK_ERROR;
    }

    if (f_read(p_file, p_data, size, &read_size) != FR_OK)
    {
        return UDISK_ERROR;
    }

    //读取到的数据大小不等于请求的大小，可能是文件末尾或发生错误
    if(read_size != size)
    {
        return UDISK_ERROR;
    }

    return UDISK_OK;
}

/**
 * @brief 同步文件
 * @param p_file 指向文件对象的指针
 * @return uint8_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
uint8_t drv_udisk_file_sync(FIL *p_file)
{
    if (p_file == NULL)
    {
        return UDISK_ERROR;
    }

    if (f_sync(p_file) != FR_OK)
    {
        return UDISK_ERROR;
    }

    return UDISK_OK;
}


uint8_t drv_udisk_file_rename(const char *old_filepath, const char *new_filepath)
{
    uint8_t res = 0;

    if (old_filepath == NULL || new_filepath == NULL)
    {
        return UDISK_ERROR;
    }

    if (old_filepath == NULL || new_filepath == NULL)
    {
        return UDISK_ERROR;
    }

    res = f_rename(old_filepath, new_filepath);
    if (res != FR_OK)
    {
        return UDISK_ERROR;
    }
    return UDISK_OK;
}



uint32_t drv_udisk_get_time(void)
{
    return 0;
}

void drv_udisk_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

