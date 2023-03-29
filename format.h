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

// Reset the file to play from the beginning.  Used by format-player to stop a video
void format_rewind(format_t* format);

// Is this video set to loop?
int format_get_loop(format_t* format);

// Set if we want to look or not.  format-player uses the callback to reset some variables
// when the video loops
typedef void(*format_loop_callback)
	(void* user_data);
void format_set_loop(format_t* format, int loop, format_loop_callback cb);

// Main decode function.  Should decode a frame of video and audio and execute their respective callbacks
int format_decode(format_t* format);

// Get framerate of the video.
int format_get_framerate(format_t* format);

// Get the video width
int format_get_width(format_t* format);

// Get the video height
int format_get_height(format_t* format);

// Have we reached the EOF of the video?
int format_has_ended(format_t* format);

// Frees all resources used to create an instance of this format
void format_destroy(format_t* format);

// The decoder calls this callback function when it has a frame ready for display.
// You may need to change the params of this depending on your video format.
typedef void(*format_video_decode_callback)
	(unsigned short* frame_data, int width, int height, int stride, int texture_height, void* user_data);

// format-player.c takes care of setting this.  
void format_set_video_decode_callback(format_t* format, format_video_decode_callback cb);

// The decoder calls this callback function when it has pcm samples ready for output.
// You may need to change the params of this depending on your audio format
typedef void(*format_audio_decode_callback)
	(unsigned char* audio_frame_data, int size, int channels, void* user_data);

// format-player.c takes care of setting this.
void format_set_audio_decode_callback(format_t* format, format_audio_decode_callback cb);

#ifdef __cplusplus
}
#endif

#endif  /* DREAMROQ_H */