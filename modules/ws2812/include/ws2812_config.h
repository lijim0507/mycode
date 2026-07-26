#ifndef __WS2812_CONFIG_H_
#define __WS2812_CONFIG_H_

/****************************************************************************/
/*								Includes									*/
/****************************************************************************/

/****************************************************************************/
/*					  Device Derived Constants								*/
/****************************************************************************/


#define WS2812_DEV_TYPE_WS2812     0
#define WS2812_DEV_TYPE_WS2816     1
#define WS2812_DEVICE_TYPE            WS2812_DEV_TYPE_WS2812


#if WS2812_DEVICE_TYPE == WS2812_DEV_TYPE_WS2812
#define WS2812_BITS_PER_CH      8       //每个单原色的位数，WS2812=8bit, WS2816A=16bit
#define WS2812_RESET_US          50      //复位脉冲持续时间，单位：微秒
#define WS2812_RESET_CYCLES      50U     //复位脉冲周期数
#else
#define WS2812_BITS_PER_CH      16
#define WS2812_RESET_US          280
#define WS2812_RESET_CYCLES      224U
#endif


#if WS2812_DEVICE_TYPE == WS2812_DEV_TYPE_WS2812
typedef uint8_t  ws2812_pixel_t;
#else
typedef uint16_t ws2812_pixel_t;
#endif


#define WS2812_CH_PER_LED        3      //三个通道代表三原色
#define WS2812_BITS_PER_LED      (WS2812_BITS_PER_CH * WS2812_CH_PER_LED)
#define WS2812_BYTES_PER_LED     (WS2812_BITS_PER_LED / 8U)


/****************************************************************************/
/*					  LED Count Configuration								*/
/****************************************************************************/

#ifndef WS2812_MAX_LEDS
#define WS2812_MAX_LEDS          10U
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/