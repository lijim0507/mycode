#ifndef __WS2812_CONFIG_H_
#define __WS2812_CONFIG_H_

/****************************************************************************/
/*								Includes									*/
/****************************************************************************/

/****************************************************************************/
/*					  Device Derived Constants								*/
/****************************************************************************/


#define WS281X_DEV_WS2812     0
#define WS281X_DEV_WS2816     1
#define WS281X_DEVICE            WS281X_DEV_WS2812


#if WS281X_DEVICE == WS281X_DEV_WS2812
#define WS281X_BITS_PER_CH      8       //每个单原色的位数，WS2812=8bit, WS2816A=16bit
#define WS281X_RESET_US          50      //复位脉冲持续时间，单位：微秒
#define WS281X_RESET_CYCLES      50U     //复位脉冲周期数
#else
#define WS281X_BITS_PER_CH      16
#define WS281X_RESET_US          280
#define WS281X_RESET_CYCLES      224U
#endif


#if WS281X_DEVICE == WS281X_DEV_WS2812
typedef uint8_t  ws281x_pixel_t;
#else
typedef uint16_t ws281x_pixel_t;
#endif


#define WS281X_CH_PER_LED        3      //三个通道代表三原色
#define WS281X_BITS_PER_LED      (WS281X_BITS_PER_CH * WS281X_CH_PER_LED)
#define WS281X_BYTES_PER_LED     (WS281X_BITS_PER_LED / 8U)


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