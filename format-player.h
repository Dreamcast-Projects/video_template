#ifndef FORMATPLAYER_H
#define FORMATPLAYER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>

#define ERROR                 0x00
#define SUCCESS               0x01
#define OUT_OF_MEMORY         0x02
#define OUT_OF_VID_MEMORY     0x03
#define SND_INIT_FAILURE      0x04
#define FORMAT_INIT_FAILURE   0x05
#define SOURCE_ERROR          0x06

#define FORMAT_SEEK_CUR  0
#define FORMAT_SEEK_SET  1
#define FORMAT_SEEK_END  2

extern int player_errno;

/* The library calls this function to ask whether it should quit playback.
 * Return non-zero if it's time to quite. */
typedef void (*frame_callback)();

typedef struct format_player_t format_player_t;

int player_init();
void player_shutdown(format_player_t* format_player);

format_player_t* player_create(const char* filename);
format_player_t* player_create_fd(FILE* f);
format_player_t* player_create_buf(unsigned char* buf, const unsigned int length);

void player_seek(format_player_t* format_player, long int offset, int whence);
void player_play(format_player_t* format_player, frame_callback frame_cb);
void player_pause(format_player_t* format_player);
void player_stop(format_player_t* format_player);
void player_volume(format_player_t* format_player, int vol);
int player_isplaying(format_player_t* format_player);
int player_get_loop(format_player_t* format_player);
void player_set_loop(format_player_t* format_player, int loop);
int player_has_ended(format_player_t* format_player);

#ifdef __cplusplus
}
#endif

#endif
