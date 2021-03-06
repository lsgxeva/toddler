#ifndef __ARCH_MIPS32_HAL_INCLUDE_PRINT__
#define __ARCH_MIPS32_HAL_INCLUDE_PRINT__


#include "common/include/data.h"


#define TAB_WIDTH   4


extern void draw_char(char ch);
extern void init_video();

extern int asmlinkage kprintf(char *fmt, ...);


#endif
