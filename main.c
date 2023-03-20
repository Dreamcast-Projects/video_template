#include <kos.h>

int main()
{
    vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
    pvr_init_defaults();

    return 0;
}