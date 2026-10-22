#include "vmode.h"

vm_plan vm_plan_for(int efb_height, int xfb_height)
{
    vm_plan p;
    p.efb_height = VM_LOGICAL_HEIGHT;
    p.xfb_height = xfb_height;
    p.rescale = efb_height != VM_LOGICAL_HEIGHT;
    return p;
}
