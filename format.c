
#include "format.h"

int format_errno = 0;

struct format_t {
    format_loop_callback loop_callback;
    format_video_decode_callback video_decode_callback;
	format_audio_decode_callback audio_decode_callback;
};

format_t* format_create_with_filename(const char* filename) {
    return NULL;
}

format_t* format_create_with_file(FILE* fh, int close_when_done) {
    return NULL;
}

format_t* format_create_with_memory(const unsigned char* bytes, size_t length, int free_when_done) {
    return NULL;
}

void format_rewind(format_t* format) {

}

int format_get_loop(format_t* format) {
    return -1;
}

void format_set_loop(format_t* format, int loop, format_loop_callback cb) {
    format->loop_callback = cb;
}

int format_decode(format_t* format) {
    return -1;
}

int format_get_framerate(format_t* format) {
    return -1;
}

int format_get_width(format_t* format) {
    return -1;
}

int format_get_height(format_t* format) {
    return -1;
}

int format_has_ended(format_t* format) {
    return -1;
}

void format_destroy(format_t* format) {

}

void format_set_video_decode_callback(format_t* format, format_video_decode_callback cb) {
    format->video_decode_callback = cb;
}

void format_set_audio_decode_callback(format_t* format, format_audio_decode_callback cb) {
    format->audio_decode_callback = cb;
}