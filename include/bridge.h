#ifndef __BRIDGE_H__
#define __BRIDGE_H__

#include <ctype.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "app.h"
#include "config.h"

uc_err bridge_init(uc_engine* uc, app* _app);


#endif
