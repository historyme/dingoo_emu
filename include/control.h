#ifndef _CONTROL_H_
#define _CONTROL_H_

#include <SDL2/SDL.h>

#define CONTROL_POWER         7 /*!< the power slider (does not track the HOLD position) */

#define CONTROL_BUTTON_A      31 /*!< the A button */
#define CONTROL_BUTTON_B      21 /*!< the B button */
#define CONTROL_BUTTON_X      16 /*!< the X button; Gemei X760+ triangle */
#define CONTROL_BUTTON_Y      6  /*!< the Y button; Gemei X760+ X */
#define CONTROL_BUTTON_START  11 /*!< the START button (does not exist on the Gemei X760+) */
#define CONTROL_BUTTON_SELECT 10 /*!< the SELECT button (does not exist on the Gemei X760+) */

#define CONTROL_TRIGGER_LEFT  8  /*!< the left shoulder button (does not exist on the Gemei X760+) */
#define CONTROL_TRIGGER_RIGHT 29 /*!< the right shoulder button (does not exist on the Gemei X760+) */

#define CONTROL_DPAD_UP       20 /*!< the UP button on the directional pad */
#define CONTROL_DPAD_DOWN     27 /*!< the DOWN button on the directional pad */
#define CONTROL_DPAD_LEFT     28 /*!< the LEFT button on the directional pad */
#define CONTROL_DPAD_RIGHT    18 /*!< the RIGHT button on the directional pad */

typedef struct {
    unsigned long pressed;
    unsigned long released;
    unsigned long status;
} KEY_STATUS;

void _kbd_get_status(KEY_STATUS* ks);

void updateKeyStatus(int type, SDL_Keycode code);


#endif