#ifndef CTW_VMODE_H
#define CTW_VMODE_H

#include <stdbool.h>

#define VM_LOGICAL_HEIGHT 480

typedef struct {
    int efb_height;
    int xfb_height;
    bool rescale;
} vm_plan;

vm_plan vm_plan_for(int efb_height, int xfb_height);

#endif
