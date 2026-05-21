/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "foc.h"
#include "math.h"
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/
#define FOC_PI 3.14159265358979323846f

#define FOC_POLE_PAIRS (7) /* 极对数 */
#define FOC_POLE_NUM  (FOC_POLE_PAIRS * 2) /* 极数 */

#define POWER_SUPPLY_VOLTAGE 24.0f /* 电源电压 */
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
float foc_electrical_angle(float shaft_angle) 
{
    return (shaft_angle * FOC_POLE_PAIRS);
}

/**
 * @brief 归一化电气角度
 * @param angle 电气角度 (弧度)
 * @return 归一化后的电气角度 (弧度)
 */
float foc_normalize_elect_angle(float angle)
{
    float a = fmod(angle, 2*FOC_PI);   //取余运算可以用于归一化，列出特殊值例子算便知
    return a >= 0 ? a : (a + 2*FOC_PI);  
}

// 设置PWM到控制器输出
void foc_set_pwm(float Ua, float Ub, float Uc) 
{

  // 限制上限
  Ua = _constrain(Ua, 0.0f, POWER_SUPPLY_VOLTAGE);
  Ub = _constrain(Ub, 0.0f, POWER_SUPPLY_VOLTAGE);
  Uc = _constrain(Uc, 0.0f, POWER_SUPPLY_VOLTAGE);
  // 计算占空比
  // 限制占空比从0到1
  dc_a = _constrain(Ua / POWER_SUPPLY_VOLTAGE, 0.0f , 1.0f );
  dc_b = _constrain(Ub / POWER_SUPPLY_VOLTAGE, 0.0f , 1.0f );
  dc_c = _constrain(Uc / POWER_SUPPLY_VOLTAGE, 0.0f , 1.0f );

  //写入PWM到PWM 0 1 2 通道
  ledcWrite(0, dc_a*255);
  ledcWrite(1, dc_b*255);
  ledcWrite(2, dc_c*255);
}

void foc_set_phase_voltage(float Uq,float Ud, float angle_el) 
{
    float Ua, Ub, Uc; // 三相电压
    float Ualpha, Ubeta; // αβ坐标系电压

    angle_el = foc_normalize_elect_angle(angle_el);

    // 进行Park逆变换得到αβ坐标系电压
    foc_inv_park_transform(Ud, Uq, angle_el, &Ualpha, &Ubeta);

    // 进行Clarke逆变换得到三相电压
    foc_inv_clarke_transform(Ualpha, Ubeta, &Ua, &Ub, &Uc);

    foc_set_pwm(Ua,Ub,Uc);
}

/**
 * @brief 克拉克变换 (Clarke Transform)：三相静止坐标(abc) -> 两相静止坐标(αβ)
 *        (幅值不变形式 amplitude-invariant)
 * @param a A相电流/电压
 * @param b B相电流/电压
 * @param c C相电流/电压
 * @param alpha 输出α轴分量指针
 * @param beta  输出β轴分量指针
 */
void foc_clarke_transform(float a, float b, float c, float *alpha, float *beta)
{
    *alpha = a;
    *beta = (a + 2.0f * b) / sqrtf(3.0f);
}

/**
 * @brief 克拉克逆变换 (Inverse Clarke Transform)：两相静止坐标(αβ) -> 三相静止坐标(abc)
 *        (幅值不变形式 amplitude-invariant)
 * @param alpha α轴分量
 * @param beta  β轴分量
 * @param a 输出A相指针
 * @param b 输出B相指针
 * @param c 输出C相指针
 */
void foc_inv_clarke_transform(float alpha, float beta, float *a, float *b, float *c)
{
    *a = alpha;
    *b = -0.5f * alpha + (sqrtf(3.0f) / 2.0f) * beta;
    *c = -0.5f * alpha - (sqrtf(3.0f) / 2.0f) * beta;
}

/**
 * @brief 帕克变换 (Park Transform)：两相静止坐标(αβ) -> 两相旋转坐标(dq)
 * @param alpha α轴分量
 * @param beta  β轴分量
 * @param theta 转子电角度 (弧度)
 * @param d 输出d轴分量指针
 * @param q 输出q轴分量指针
 */
void foc_park_transform(float alpha, float beta, float theta, float *d, float *q)
{
    *d = alpha * cosf(theta) + beta * sinf(theta);
    *q = -alpha * sinf(theta) + beta * cosf(theta);
}

/**
 * @brief 帕克逆变换 (Inverse Park Transform)：两相旋转坐标(dq) -> 两相静止坐标(αβ)
 * @param d d轴分量
 * @param q q轴分量
 * @param theta 转子电角度 (弧度)
 * @param alpha 输出α轴分量指针
 * @param beta  输出β轴分量指针
 */
void foc_inv_park_transform(float d, float q, float theta, float *alpha, float *beta)
{
    *alpha = d * cosf(theta) - q * sinf(theta);
    *beta = d * sinf(theta) + q * cosf(theta);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/

