#include "ui_data.h"

/* Kept as a team integration switch for display-port experiments. */
#ifndef UI_PORT_DEMO
#define UI_PORT_DEMO 0
#endif

#ifdef UI_HOST_SIM
uint32_t sim_millis(void);
uint32_t ui_port_millis(void) { return sim_millis(); }
#else
#include "main.h"
uint32_t ui_port_millis(void) { return HAL_GetTick(); }
#endif

