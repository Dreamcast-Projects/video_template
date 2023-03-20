#ifndef FORMAT_H
#define FORMAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>

#define FORMAT_SUCCESS           0
#define FORMAT_FILE_OPEN_FAILURE 1
#define FORMAT_FILE_READ_FAILURE 2
#define FORMAT_CHUNK_TOO_LARGE   3
#define FORMAT_BAD_CODEBOOK      4
#define FORMAT_INVALID_PIC_SIZE  5
#define FORMAT_NO_MEMORY         6
#define FORMAT_BAD_VQ_STREAM     7
#define FORMAT_INVALID_DIMENSION 8
#define FORMAT_RENDER_PROBLEM    9
#define FORMAT_CLIENT_PROBLEM    10

#define FORMAT_BUFFER_DEFAULT_SIZE (64 * 1024)

extern int format_errno;

// Object types for the various interfaces
typedef struct format_t format_t;

format_t* format_create_with_filename(const char* filename);
format_t* format_create_with_file(FILE* fh, int close_when_done);
format_t* format_create_with_memory(const unsigned char* bytes, size_t length, int free_when_done);

/* The library calls this function when it has a frame ready for display. */
typedef void(*format_video_decode_callback)
	(unsigned short *frame_data, int width, int height, int stride, int texture_height);
void format_set_video_decode_callback(format_t* format, format_video_decode_callback cb);

/* The library calls this function when it has pcm samples ready for output. */
typedef void(*format_audio_decode_callback)
	(unsigned char *audio_frame_data, int size, int channels);
void format_set_audio_decode_callback(format_t* format, format_audio_decode_callback cb);

int format_get_loop(format_t* format);

void format_set_loop(format_t* format, int loop);

int format_decode(format_t* format);

int format_get_framerate(format_t* format);

int format_get_width(format_t* format);

int format_get_height(format_t* format);

int format_has_ended(format_t* format);

void format_destroy(format_t* format);

#ifdef __cplusplus
}
#endif

#endif  /* DREAMROQ_H */