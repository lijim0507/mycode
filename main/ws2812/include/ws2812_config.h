#ifndef __WS2812_CONFIG_H_
#define __WS2812_CONFIG_H_

/****************************************************************************/
/*								Includes									*/
/****************************************************************************/

/****************************************************************************/
/*						  Device Selection									*/
/****************************************************************************/

#define WS281X_DEV_WS2812       1
#define WS281X_DEV_WS2816A      2

#ifndef WS281X_DEVICE
#define WS281X_DEVICE           WS281X_DEV_WS2812
#endif

/****************************************************************************/
/*					  Device Derived Constants								*/
/****************************************************************************/

#if WS281X_DEVICE == WS281X_DEV_WS2816A
#define WS281X_BITS_PER_CH      16
#define WS281X_BYTES_PER_CH     2
#define WS281X_HAS_GAIN          1
#define WS281X_GAIN_BITS         16
#define WS281X_GAIN_BYTES        2
#define WS281X_RESET_US          280
#define WS281X_RESET_CYCLES      224U
#else
#define WS281X_BITS_PER_CH      8
#define WS281X_BYTES_PER_CH     1
#define WS281X_HAS_GAIN          0
#define WS281X_GAIN_BITS         0
#define WS281X_GAIN_BYTES        0
#define WS281X_RESET_US          50
#define WS281X_RESET_CYCLES      50U
#endif

#define WS281X_CH_PER_LED        3
#define WS281X_BITS_PER_LED      (WS281X_BITS_PER_CH * WS281X_CH_PER_LED)
#define WS281X_BYTES_PER_LED     (WS281X_BYTES_PER_CH * WS281X_CH_PER_LED)
#define WS281X_FRAME_HEADER_BITS  (WS281X_HAS_GAIN ? WS281X_GAIN_BITS : 0)
#define WS281X_FRAME_HEADER_BYTES (WS281X_HAS_GAIN ? WS281X_GAIN_BYTES : 0)

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