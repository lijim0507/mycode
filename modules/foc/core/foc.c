/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "foc.h"
#include "foc_port.h"

#include "utilities.h"
#include <math.h>
#include <stdbool.h>
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define FOC_PI              3.14159265358979323846f
#define FOC_POLE_PAIRS      (7)
#define FOC_POLE_NUM        (FOC_POLE_PAIRS * 2)
#define POWER_SUPPLY_VOLTAGE 24.0f
/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static void foc_set_pwm(float Ua, float Ub, float Uc);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const foc_driver_t *g_driver;
static bool                g_initialized;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int foc_init(const foc_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->init || !driver->write_duty) {
        return -1;
    }

    if (g_initialized) {
        foc_deinit();
    }

    if (driver->init(port_cfg) != 0) {
        return -2;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

int foc_deinit(void)
{
    if (!g_initialized) {
        return 0;
    }

    if (g_driver && g_driver->deinit) {
        g_driver->deinit();
    }

    g_driver      = NULL;
    g_initialized = false;
    return 0;
}

float foc_electrical_angle(float shaft_angle)
{
    return (shaft_angle * FOC_POLE_PAIRS);
}

float foc_normalize_elect_angle(float angle)
{
    float a = fmod(angle, 2.0f * FOC_PI);
    return a >= 0.0f ? a : (a + 2.0f * FOC_PI);
}

void foc_clarke_transform(float a, float b, float c, float *alpha, float *beta)
{
    *alpha = a;
    *beta  = (a + 2.0f * b) / sqrtf(3.0f);
}

void foc_inv_clarke_transform(float alpha, float beta, float *a, float *b, float *c)
{
    *a = alpha;
    *b = -0.5f * alpha + (sqrtf(3.0f) / 2.0f) * beta;
    *c = -0.5f * alpha - (sqrtf(3.0f) / 2.0f) * beta;
}

void foc_park_transform(float alpha, float beta, float theta, float *d, float *q)
{
    *d =  alpha * cosf(theta) + beta * sinf(theta);
    *q = -alpha * sinf(theta) + beta * cosf(theta);
}

void foc_inv_park_transform(float d, float q, float theta, float *alpha, float *beta)
{
    *alpha = d * cosf(theta) - q * sinf(theta);
    *beta  = d * sinf(theta) + q * cosf(theta);
}

void foc_set_phase_voltage(float Uq, float Ud, float angle_el)
{
    float Ualpha, Ubeta;
    float Ua, Ub, Uc;

    angle_el = foc_normalize_elect_angle(angle_el);

    foc_inv_park_transform(Ud, Uq, angle_el, &Ualpha, &Ubeta);
    foc_inv_clarke_transform(Ualpha, Ubeta, &Ua, &Ub, &Uc);

    foc_set_pwm(Ua, Ub, Uc);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static void foc_set_pwm(float Ua, float Ub, float Uc)
{
    float dc_a, dc_b, dc_c;

    Ua = clamp_float(Ua, 0.0f, POWER_SUPPLY_VOLTAGE);
    Ub = clamp_float(Ub, 0.0f, POWER_SUPPLY_VOLTAGE);
    Uc = clamp_float(Uc, 0.0f, POWER_SUPPLY_VOLTAGE);

    dc_a = clamp_float(Ua / POWER_SUPPLY_VOLTAGE, 0.0f, 1.0f);
    dc_b = clamp_float(Ub / POWER_SUPPLY_VOLTAGE, 0.0f, 1.0f);
    dc_c = clamp_float(Uc / POWER_SUPPLY_VOLTAGE, 0.0f, 1.0f);

    g_driver->write_duty(0, dc_a);
    g_driver->write_duty(1, dc_b);
    g_driver->write_duty(2, dc_c);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
