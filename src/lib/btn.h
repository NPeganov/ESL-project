#ifndef _btn_H_
#define _btn_H_

#include "pca10059.h"

static const uint8_t m_btn_list[BUTTONS_NUMBER] = BUTTONS_LIST;

void btn_init(void);
bool btn_pressed(void);

#endif