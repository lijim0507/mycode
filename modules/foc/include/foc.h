
#ifndef __FOC_H_
#define __FOC_H_
/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

typedef struct foc_driver {
    int  (*init)(void *config);
    void (*write_duty)(uint8_t channel, float duty_cycle);
    int  (*deinit)(void);
} foc_driver_t;

/****************************************************************************/
/*						Exproted Variables								*/
/****************************************************************************/

/****************************************************************************/
/*						Exproted Functions								*/
/****************************************************************************/

int   foc_init(const foc_driver_t *driver, void *port_cfg);
int   foc_deinit(void);

float foc_electrical_angle(float shaft_angle);
float foc_normalize_elect_angle(float angle);
void  foc_clarke_transform(float a, float b, float c, float *alpha, float *beta);
void  foc_inv_clarke_transform(float alpha, float beta, float *a, float *b, float *c);
void  foc_park_transform(float alpha, float beta, float theta, float *d, float *q);
void  foc_inv_park_transform(float d, float q, float theta, float *alpha, float *beta);
void  foc_set_phase_voltage(float Uq, float Ud, float angle_el);

#ifdef __cplusplus
}
#endif

#endif
/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
