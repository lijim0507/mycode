/****************************************************************************/
/*								Includes									*/
/****************************************************************************/
#include "can_simple.h"
#include "can_simple_port.h"

#include <string.h>
/****************************************************************************/
/*								Macros										*/
/****************************************************************************/

/****************************************************************************/
/*								Typedefs									*/
/****************************************************************************/

/****************************************************************************/
/*						Prototypes Of Local Functions						*/
/****************************************************************************/
static int  can_simple_send_cmd(uint8_t node_id, uint8_t cmd_id,
                                 const uint8_t *data, uint8_t len);
static void pack_float(uint8_t *buf, float val);
static void pack_int16(uint8_t *buf, int16_t val);

/****************************************************************************/
/*							Global Variables								*/
/****************************************************************************/
static const can_simple_driver_t *g_driver;
static bool                       g_initialized;
/****************************************************************************/
/*							Exported Functions    						    */
/****************************************************************************/

int can_simple_init(const can_simple_driver_t *driver, void *port_cfg)
{
    if (!driver || !driver->init || !driver->send) {
        return -1;
    }

    if (g_initialized) {
        can_simple_deinit();
    }

    if (driver->init(port_cfg) != 0) {
        return -2;
    }

    g_driver      = driver;
    g_initialized = true;
    return 0;
}

int can_simple_deinit(void)
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

void can_simple_set_axis_state(uint8_t node_id, uint8_t axis_state)
{
    uint8_t data[8] = {0};
    data[0] = axis_state;
    can_simple_send_cmd(node_id, CAN_CMD_SET_AXIS_STATE, data, 8);
}

void can_simple_set_ctrl_mode(uint8_t node_id, uint8_t ctrl_mode, uint8_t input_mode)
{
    uint8_t data[8] = {0};
    data[0] = ctrl_mode;
    data[1] = input_mode;
    can_simple_send_cmd(node_id, CAN_CMD_SET_CTRL_MODE, data, 8);
}

void can_simple_set_input_pos(uint8_t node_id, float pos, int16_t vel_ff, int16_t torque_ff)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], pos);
    pack_int16(&data[4], vel_ff);
    pack_int16(&data[6], torque_ff);
    can_simple_send_cmd(node_id, CAN_CMD_SET_INPUT_POS, data, 8);
}

void can_simple_set_input_vel(uint8_t node_id, float vel, float torque_ff)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], vel);
    pack_float(&data[4], torque_ff);
    can_simple_send_cmd(node_id, CAN_CMD_SET_INPUT_VEL, data, 8);
}

void can_simple_set_input_torque(uint8_t node_id, float torque)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], torque);
    can_simple_send_cmd(node_id, CAN_CMD_SET_INPUT_TORQUE, data, 8);
}

void can_simple_set_vel_limit(uint8_t node_id, float vel_limit)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], vel_limit);
    can_simple_send_cmd(node_id, CAN_CMD_SET_VEL_LIMIT, data, 8);
}

void can_simple_set_traj_vel_limit(uint8_t node_id, float traj_vel_limit)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], traj_vel_limit);
    can_simple_send_cmd(node_id, CAN_CMD_SET_TRAJ_VEL_LIMIT, data, 8);
}

void can_simple_set_traj_accel_limit(uint8_t node_id, float traj_accel_limit, float traj_decel_limit)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], traj_accel_limit);
    pack_float(&data[4], traj_decel_limit);
    can_simple_send_cmd(node_id, CAN_CMD_SET_TRAJ_ACCEL_LIMIT, data, 8);
}

void can_simple_set_pos_gain(uint8_t node_id, float pos_gain)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], pos_gain);
    can_simple_send_cmd(node_id, CAN_CMD_SET_POS_GAIN, data, 8);
}

void can_simple_set_vel_gains(uint8_t node_id, float vel_gain, float vel_integrator_gain)
{
    uint8_t data[8] = {0};
    pack_float(&data[0], vel_gain);
    pack_float(&data[4], vel_integrator_gain);
    can_simple_send_cmd(node_id, CAN_CMD_SET_VEL_GAINS, data, 8);
}

void can_simple_reboot(uint8_t node_id)
{
    can_simple_send_cmd(node_id, CAN_CMD_REBOOT, NULL, 0);
}

void can_simple_clear_errors(uint8_t node_id)
{
    can_simple_send_cmd(node_id, CAN_CMD_CLEAR_ERRORS, NULL, 0);
}

void can_simple_estop(uint8_t node_id)
{
    can_simple_send_cmd(node_id, CAN_CMD_ESTOP, NULL, 0);
}

/****************************************************************************/
/*							Static Functions    						    */
/****************************************************************************/

static int can_simple_send_cmd(uint8_t node_id, uint8_t cmd_id,
                                const uint8_t *data, uint8_t len)
{
    if (!g_initialized || !g_driver) {
        return -1;
    }

    uint16_t can_id = CAN_ID_GEN(node_id, cmd_id);
    return g_driver->send(can_id, data, len);
}

static void pack_float(uint8_t *buf, float val)
{
    memcpy(buf, &val, sizeof(float));
}

static void pack_int16(uint8_t *buf, int16_t val)
{
    buf[0] = (uint8_t)(val & 0xFF);
    buf[1] = (uint8_t)((val >> 8) & 0xFF);
}

/****************************************************************************/
/*								EOF											*/
/****************************************************************************/
