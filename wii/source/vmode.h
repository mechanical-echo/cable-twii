#ifndef CTW_VMODE_H
#define CTW_VMODE_H

#include <stdbool.h>

#define VM_LOGICAL_HEIGHT 480
#define VM_UNSCALED_REG 256u
#define VM_MIN_SCALE_REG 128u

typedef struct {
    int efb_height;
    int xfb_height;
    bool rescale;
    unsigned scale_reg;
    int copy_lines;
} vm_plan;

vm_plan vm_plan_for(int efb_height, int xfb_height);
int vm_copy_lines(int efb_height, unsigned scale_reg);

#endif
