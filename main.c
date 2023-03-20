#include <kos.h>

#include "format-player.h"

int main()
{
    vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
    pvr_init_defaults();

    format_player_t* player = player_create("/rd/roguelogo.roq", 0);

    player_play(player, 0);
    // Decode
    // do {
    //     if(controller_state(CONT_START))
    //         break;

    //     if(controller_state(CONT_A))
    //         ;
	
    //     // Decode
    //     player_decode(player);
    // } while (!player_has_ended(player));


    return 0;
}