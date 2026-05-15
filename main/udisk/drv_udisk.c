/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "drv_udisk.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

#ifndef FILE_SYS
#define FIL void
#endif

#define FILENAME_MAX_SIZE    32      /* 文件名最大长度 */

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/

/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/


/**
 * @brief 打开文件
 * @param p_file 指向文件对象的指针
 * @param p_filename 文件名字符串
 * @param mode 打开模式
 * @return udisk_state_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
udisk_state_t drv_udisk_file_open(FIL *p_file, const char *p_filename, uint8_t mode)
{
    if (p_file == NULL || p_filename == NULL)
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
 * @return udisk_state_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
udisk_state_t drv_udisk_file_close(FIL *p_file)
{
    if (p_file == NULL)
    {
        return UDISK_ERROR;
    }

    if (f_close(p_file) == FR_OK)
    {
        return UDISK_OK;
    }

		return UDISK_ERROR;
}

/**
 * @brief 写入文件
 * @param p_file 指向文件对象的指针
 * @param p_data 指向数据的指针
 * @param size 数据大小
 * @return udisk_state_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
udisk_state_t drv_udisk_file_write(FIL *p_file, const void *p_data, uint32_t size)
{
    uint16_t written_size = 0;

     if (p_file == NULL || p_data == NULL || size == 0)
    {
        g_udisk_status.write_state = UDISK_ERROR;
        return UDISK_ERROR;
    }

    if (f_write(p_file, p_data, size, &written_size) != FR_OK)
    {
        g_udisk_status.write_state = UDISK_ERROR;
        return UDISK_ERROR;
    }

    if(written_size != size)
    {
        g_udisk_status.write_state = UDISK_ERROR;
        return UDISK_ERROR;
    }

    g_udisk_status.write_state = UDISK_OK;
    return UDISK_OK;
}

/**
 * @brief 读取文件
 * @param p_file 指向文件对象的指针
 * @param p_data 指向数据缓冲区的指针
 * @param size 要读取的数据大小
 * @return udisk_state_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
udisk_state_t drv_udisk_file_read(FIL *p_file, void *p_data, uint32_t size)
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
 * @return udisk_state_t 返回UDISK_OK表示成功，UDISK_ERROR表示失败
 */
udisk_state_t drv_udisk_file_sync(FIL *p_file)
{
    if (p_file == NULL)
    {
        g_udisk_status.sync_state = UDISK_ERROR;
        return UDISK_ERROR;
    }

    if (f_sync(p_file) != FR_OK)
    {
        g_udisk_status.sync_state = UDISK_ERROR;
        return UDISK_ERROR;
    }

    g_udisk_status.sync_state = UDISK_OK;
    return UDISK_OK;
}


udisk_state_t drv_udisk_file_rename(FIL *p_file, const char *id)
{
    char new_filename[FILENAME_MAX_SIZE];
    char old_filename[FILENAME_MAX_SIZE];

    drv_udisk_file_close(&g_data_file);


}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

