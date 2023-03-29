#ifndef FORMATPLAYER_H
#define FORMATPLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define PLAYER_ERROR                 0x00
#define PLAYER_SUCCESS               0x01
#define PLAYER_OUT_OF_MEMORY         0x02
#define PLAYER_OUT_OF_VID_MEMORY     0x03
#define PLAYER_SND_INIT_FAILURE      0x04
#define PLAYER_FORMAT_INIT_FAILURE   0x05
#define PLAYER_SOURCE_ERROR          0x06

#define FORMAT_SEEK_CUR  0
#define FORMAT_SEEK_SET  1
#define FORMAT_SEEK_END  2

extern int player_errno;

// We call this function once per frame of playing video.  You
// can use it to pause/stop the video, shutdown the player, exit the app, etc
typedef void (*frame_callback)();

typedef struct format_player_t format_player_t;

int player_init(void);
void player_shutdown(format_player_t* format_player);

format_player_t* player_create(const char* filename);
format_player_t* player_create_file(FILE* f);
format_player_t* player_create_memory(unsigned char* buf, const unsigned int length);

void player_play(format_player_t* player, frame_callback frame_cb);
void player_pause(format_player_t* player);
void player_stop(format_player_t* player);
void player_volume(format_player_t* player, int vol);
int player_isplaying(format_player_t* player);
int player_get_loop(format_player_t* player);
void player_set_loop(format_player_t* player, int loop);
int player_has_ended(format_player_t* player);

#ifdef __cplusplus
}
#endif

#endif
