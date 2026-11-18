#include "vmode.h"

vm_plan vm_plan_for(int efb_height, int xfb_height)
{
    vm_plan p;
    p.efb_height = VM_LOGICAL_HEIGHT;
    p.xfb_height = xfb_height;
    p.rescale = efb_height != VM_LOGICAL_HEIGHT;
    p.scale_reg = VM_UNSCALED_REG;
    if (p.rescale) {
        while (p.scale_reg > VM_MIN_SCALE_REG && vm_copy_lines(p.efb_height, p.scale_reg - 1) <= xfb_height) p.scale_reg--;
    }
    p.copy_lines = vm_copy_lines(p.efb_height, p.scale_reg);
    return p;
}

int vm_copy_lines(int efb_height, unsigned scale_reg)
{
    unsigned efb = (unsigned)efb_height;
    unsigned lines = (((efb - 1) << 8) / scale_reg) + 1;
    if (scale_reg > 128 && scale_reg < 256) {
        unsigned r = scale_reg;
        while (r & 1u) r >>= 1;
        if (r != 0 && efb == r * (efb / r)) lines++;
    }
    return lines > 1024 ? 1024 : (int)lines;
}
