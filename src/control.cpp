#include "control.h"
#include <memory.h>

KEY_STATUS s_KayStatus = { 0 };

void _kbd_get_status(KEY_STATUS* ks)
{
	memcpy(ks, &s_KayStatus, sizeof(s_KayStatus));
}

static void updateKey(int press, uint32_t key)
{
    if (press)
    {
        s_KayStatus.status |= (1 << key);
    }
    else
    {
        s_KayStatus.status &= (~(1 << key));
    }
}

void updateKeyStatus(int type, SDL_Keycode code)
{
    switch (code)
    {
    case SDLK_0:
        updateKey(type, CONTROL_BUTTON_SELECT);
        // select
        break;
    case SDLK_1:
        updateKey(type, CONTROL_BUTTON_START);
        // start
        break;
    case SDLK_w:
    case SDLK_UP:  // ио
        updateKey(type, CONTROL_DPAD_UP);
        break;
    case SDLK_s:
    case SDLK_DOWN:  // об
        updateKey(type, CONTROL_DPAD_DOWN);
        break;
    case SDLK_a:
    case SDLK_LEFT:  // вС
        updateKey(type, CONTROL_DPAD_LEFT);
        break;
    case SDLK_d:
    case SDLK_RIGHT:  // ср
        updateKey(type, CONTROL_DPAD_RIGHT);
        break;
    case SDLK_k: // a
        updateKey(type, CONTROL_BUTTON_A);
        break;
    case SDLK_l: // b
        updateKey(type, CONTROL_BUTTON_B);
        break;
    case SDLK_o: //y
        updateKey(type, CONTROL_BUTTON_Y);
        break;
    case SDLK_i: //x
        updateKey(type, CONTROL_BUTTON_X);
        break;
    default:
        break;
    }
}