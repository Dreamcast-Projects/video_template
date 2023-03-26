
#include <kos/thread.h>
#include <dc/sound/stream.h>
#include <dc/pvr.h>
#include <arch/timer.h>
#include <arch/cache.h>

#include <stdlib.h>
#include <string.h>

#include "format.h"
#include "format-player.h"

#define SND_STREAM_STATUS_NULL         0x00
#define SND_STREAM_STATUS_READY        0x01
#define SND_STREAM_STATUS_STREAMING    0x02
#define SND_STREAM_STATUS_PAUSING      0x03
#define SND_STREAM_STATUS_STOPPING     0x04
#define SND_STREAM_STATUS_RESUMING     0x05
#define SND_STREAM_STATUS_DONE         0x06
#define SND_STREAM_STATUS_ERROR        0x07

int player_errno = 0;

struct format_player_t {
    format_t* format;
    int paused;
    int vol;
    int initialized_format;
};

#define AUDIO_BUFFER_SIZE 1024*80
#define AUDIO_DECODE_BUFFER_SIZE 1024*128
typedef int snd_stream_hnd_t;

typedef struct {
    unsigned char *buffer;
    int head;
    int tail;
    int size;
    int capacity;
} ring_buffer;

typedef struct {
    snd_stream_hnd_t shnd; 
    volatile int status;
    mutex_t decode_buffer_mut;
    int pcm_size;
    int initialized;
    unsigned int vol;
    unsigned int rate;
    unsigned int channels;
    ring_buffer decode_buffer;
    unsigned char pcm_buffer[AUDIO_BUFFER_SIZE];
} sound_hndlr;

typedef struct {
    int framerate;
    int initialized;
    int frame_index;
    int texture_byte_length;
    pvr_ptr_t textures[2];
    pvr_poly_hdr_t hdr[2];
    pvr_vertex_t vert[4];
} video_hndlr;

static void* player_snd_thread();
static void* aica_callback(snd_stream_hnd_t hnd, int req, int* done);

// The blueprint of these callbacks can be different depending on the format
static void format_video_cb(unsigned short *buf, int width, int height, int stride, int texture_height);
static void format_audio_cb(unsigned char *buf, int size, int channels);

static void initialize_defaults(format_player_t* player, int index);
static int initialize_graphics(int width, int height);
static int initialize_audio();

static int ring_buffer_write(ring_buffer *rb, const unsigned char *data, int data_length);
static int ring_buffer_read(ring_buffer *rb, unsigned char *data, int data_length);

static uint64 dc_get_time();

static video_hndlr vid_stream;
static sound_hndlr snd_stream;

static kthread_t* thread;

static int playing_loop;

// Used to keep video and audio in sync
static unsigned int frame;
static unsigned long samples_done;
static float ATS, VTS;
static double audio_samples_per_sec;

int player_init() {
    snd_stream_init();
    pvr_init_defaults();

    snd_stream.shnd = SND_STREAM_INVALID;
    snd_stream.vol = 240;
    snd_stream.status = SND_STREAM_STATUS_NULL;

    thread = thd_create(0, player_snd_thread, NULL);
    if(thread != NULL) {
        snd_stream.status = SND_STREAM_STATUS_READY;
        return SUCCESS;
    }
    else {
        snd_stream.status = SND_STREAM_STATUS_ERROR;
        return ERROR;
    }
}

void player_shutdown(format_player_t* format_player) {
    snd_stream.status = SND_STREAM_STATUS_DONE;
    frame = 0;
    samples_done = 0;
    playing_loop = 0;

    thd_join(thread, NULL);

    if(snd_stream.shnd != SND_STREAM_INVALID) {
        snd_stream_stop(snd_stream.shnd);
        snd_stream_destroy(snd_stream.shnd);
        snd_stream.shnd = SND_STREAM_INVALID;
        snd_stream.vol = 240;
        snd_stream.status = SND_STREAM_STATUS_NULL;
    }

    if(vid_stream.initialized) {
        vid_stream.initialized = 0;
        vid_stream.frame_index = 0;
        vid_stream.texture_byte_length = 0;
        pvr_mem_free(vid_stream.textures[0]);
        pvr_mem_free(vid_stream.textures[1]);
    }

    if(snd_stream.initialized) {
        snd_stream.initialized = 0;
        free(snd_stream.decode_buffer.buffer);
        mutex_destroy(&snd_stream.decode_buffer_mut);
    }

    if(format_player != NULL && format_player->initialized_format) {
        format_destroy(format_player->format);
        free(format_player);
        format_player = NULL;
    }
}

format_player_t* player_create(const char* filename) {
    snd_stream_hnd_t index;
    format_player_t* player = NULL;
    
    if(filename == NULL) {
        player_errno = SOURCE_ERROR;
        return NULL;
    }

    index = snd_stream_alloc(aica_callback, SND_STREAM_BUFFER_MAX/4);

    if(index == SND_STREAM_INVALID) {
        snd_stream_destroy(index);
        player_errno = SND_INIT_FAILURE;
        return NULL;
    }

    player = malloc(sizeof(format_player_t));
    if(!player) {
        snd_stream_destroy(index);
        player_errno = OUT_OF_MEMORY;
        return NULL;
    }

    player->format = format_create_with_filename(filename);
    if(!player->format) {
        snd_stream_destroy(index);
        player_errno = FORMAT_INIT_FAILURE;
        return NULL;
    }

    initialize_defaults(player, index);

    return player;
}

format_player_t* player_create_file(FILE* file) {
    snd_stream_hnd_t index;
    format_player_t* player = NULL;

    if(file == NULL) {
        player_errno = SOURCE_ERROR;
        return NULL;
    }
    
    index = snd_stream_alloc(aica_callback, SND_STREAM_BUFFER_MAX/4);

    if(index == SND_STREAM_INVALID) {
        snd_stream_destroy(index);
        player_errno = SND_INIT_FAILURE;
        return NULL;
    }

    player = malloc(sizeof(format_player_t));
    if(!player) {
        snd_stream_destroy(index);
        player_errno = OUT_OF_MEMORY;
        return NULL;
    }

    player->format = format_create_with_file(file, 1);
    if(!player->format) {
        snd_stream_destroy(index);
        player_errno = FORMAT_INIT_FAILURE;
        return NULL;
    }

    initialize_defaults(player, index);

    return player;
}

format_player_t* player_create_memory(unsigned char* memory, const unsigned int length) {
    snd_stream_hnd_t index;
    format_player_t* player = NULL;

    index = snd_stream_alloc(aica_callback, SND_STREAM_BUFFER_MAX/4);

    if(memory == NULL) {
        player_errno = SOURCE_ERROR;
        return NULL;
    }

    if(index == SND_STREAM_INVALID) {
        snd_stream_destroy(index);
        player_errno = SND_INIT_FAILURE;
        return NULL;
    }

    player = malloc(sizeof(format_player_t));
    if(!player) {
        snd_stream_destroy(index);
        player_errno = OUT_OF_MEMORY;
        return NULL;
    }

    player->format = format_create_with_memory(memory, length, 1);
    if(!player->format) {
        snd_stream_destroy(index);
        player_errno = FORMAT_INIT_FAILURE;
        return NULL;
    }

    initialize_defaults(player, index);

    return player;
}

void player_seek(format_player_t* format_player, long int offset, int whence) {
   switch (whence) {
	case FORMAT_SEEK_CUR:
		
		break;
	case FORMAT_SEEK_SET:
		
		break;
	case FORMAT_SEEK_END:
		
		break;
	}
}

void player_play(format_player_t* format_player, frame_callback frame_cb) {
    if(snd_stream.status == SND_STREAM_STATUS_STREAMING)
       return;

    format_player->paused = 0;
    snd_stream.status = SND_STREAM_STATUS_RESUMING;

    // Protect against recursion bc we can call player_play() in
    // frame_cb()
    if(!playing_loop)
    {
        playing_loop = 1;

        do {
            if(frame_cb)
                frame_cb();

            // We shutdown the player, exit the loop
            if(snd_stream.status == SND_STREAM_STATUS_NULL) {
                break;
            }

            if(!format_player->paused)
                format_decode(format_player->format);
        } while (!format_has_ended(format_player->format));
    }
}

void player_pause(format_player_t* format_player) {
    format_player->paused = 1;
    if(snd_stream.status != SND_STREAM_STATUS_READY &&
       snd_stream.status != SND_STREAM_STATUS_PAUSING)
        snd_stream.status = SND_STREAM_STATUS_PAUSING;
}

void player_stop(format_player_t* format_player) {
    frame = 0;
    samples_done = 0;
    format_player->paused = 1;
    format_seek(format_player->format);

    if(snd_stream.status != SND_STREAM_STATUS_READY &&
       snd_stream.status != SND_STREAM_STATUS_STOPPING)
        snd_stream.status = SND_STREAM_STATUS_STOPPING;
}

void player_volume(format_player_t* format_player, int vol) {
    if(snd_stream.shnd == SND_STREAM_INVALID)
        return;

    if(vol > 255)
        vol = 255;

    if(vol < 0)
        vol = 0;

    snd_stream.vol = vol;
    snd_stream_volume(snd_stream.shnd, snd_stream.vol);
}

int player_isplaying(format_player_t* format_player) {
    return snd_stream.status == SND_STREAM_STATUS_STREAMING;
}

int player_get_loop(format_player_t* format_player) {
    return format_get_loop(format_player->format);
}

void player_set_loop(format_player_t* format_player, int loop) {
    format_set_loop(format_player->format, loop);
}

int player_has_ended(format_player_t* format_player) {
    return format_has_ended(format_player->format);
}

static void format_video_cb(unsigned short *texture_data, int width, int height, int stride, int texture_height) {

    // send the video frame as a texture over to video RAM 
    dcache_flush_range((uint32)texture_data, vid_stream.texture_byte_length);   // dcache flush is needed when using DMA
    pvr_txr_load_dma(texture_data, vid_stream.textures[vid_stream.frame_index], vid_stream.texture_byte_length, 1, NULL, 0);

    VTS = ++frame / (double)vid_stream.framerate;
    while(ATS < VTS)
        thd_pass();

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);

    pvr_prim(&vid_stream.hdr[vid_stream.frame_index], sizeof(pvr_poly_hdr_t));
    pvr_prim(&vid_stream.vert[0], sizeof(pvr_vertex_t));
    pvr_prim(&vid_stream.vert[1], sizeof(pvr_vertex_t));
    pvr_prim(&vid_stream.vert[2], sizeof(pvr_vertex_t));
    pvr_prim(&vid_stream.vert[3], sizeof(pvr_vertex_t));

    pvr_list_finish();
    pvr_scene_finish();

    vid_stream.frame_index = !vid_stream.frame_index;
}

static void format_audio_cb(unsigned char *audio_data, int data_length, int channels) {
    snd_stream.channels = channels;

    mutex_lock(&snd_stream.decode_buffer_mut);

    int success = ring_buffer_write(&snd_stream.decode_buffer, audio_data, data_length);
    if (!success) {
        printf("Buffer Overflow - decoding audio too fast and there is no room in buffer\n\n");
        fflush(stdout);
    }

    mutex_unlock(&snd_stream.decode_buffer_mut);
}

static void* aica_callback(snd_stream_hnd_t hnd, int bytes_needed, int* bytes_returning) {
    while (snd_stream.decode_buffer.size < bytes_needed) {
        thd_pass();
    }

    mutex_lock(&snd_stream.decode_buffer_mut);

    int success = ring_buffer_read(&snd_stream.decode_buffer, snd_stream.pcm_buffer, bytes_needed);
    if (!success) {
        printf("Buffer Underflow - didnt have enough data in the buffer to give to AICA\n\n");
        fflush(stdout);
    }

    mutex_unlock(&snd_stream.decode_buffer_mut);

    samples_done += bytes_needed; /* Record the Audio Time Stamp */
    ATS = samples_done/audio_samples_per_sec;

    *bytes_returning = bytes_needed;

    return snd_stream.pcm_buffer;
}

static void initialize_defaults(format_player_t* player, int index) {
    frame = 0;
    samples_done = 0;
    ATS = 0, VTS = 0;
    audio_samples_per_sec = 0;

    format_set_video_decode_callback(player->format, format_video_cb);
    format_set_audio_decode_callback(player->format, format_audio_cb);

    vid_stream.framerate = format_get_framerate(player->format);

    snd_stream.shnd = index;
    snd_stream.status = SND_STREAM_STATUS_READY;
    audio_samples_per_sec = (double)(snd_stream.rate*snd_stream.channels*2.0);

    player->initialized_format = 1;

    initialize_graphics(format_get_width(player->format), format_get_height(player->format));
    initialize_audio();
}

static int initialize_graphics(int width, int height) 
{
    if(vid_stream.initialized)
        return SUCCESS;

    vid_stream.texture_byte_length = width * height * 2;
    vid_stream.textures[0] = pvr_mem_malloc(vid_stream.texture_byte_length);
    vid_stream.textures[1] = pvr_mem_malloc(vid_stream.texture_byte_length);
    if (!vid_stream.textures[0] || !vid_stream.textures[1])
        return OUT_OF_VID_MEMORY;

    pvr_poly_cxt_t cxt;

    /* Precompile the poly headers */
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, width, height, vid_stream.textures[0], PVR_FILTER_BILINEAR);
    pvr_poly_compile(&vid_stream.hdr[0], &cxt);
    pvr_poly_cxt_txr(&cxt, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, width, height, vid_stream.textures[1], PVR_FILTER_BILINEAR);
    pvr_poly_compile(&vid_stream.hdr[1], &cxt);

    float ratio;
    /* screen coordinates of upper left and bottom right corners */
    int ul_x, ul_y, br_x, br_y;

    /* this only works if width ratio <= height ratio */
    ratio = 640.0 / width;
    ul_x = 0;
    br_x = (ratio * width);
    ul_y = ((480 - ratio * height) / 2);
    br_y = ul_y + ratio * height;

    /* Things common to vertices */
    vid_stream.vert[0].z     = vid_stream.vert[1].z     = vid_stream.vert[2].z     = vid_stream.vert[3].z     = 1.0f; 
    vid_stream.vert[0].argb  = vid_stream.vert[1].argb  = vid_stream.vert[2].argb  = vid_stream.vert[3].argb  = PVR_PACK_COLOR(1.0f, 1.0f, 1.0f, 1.0f);    
    vid_stream.vert[0].oargb = vid_stream.vert[1].oargb = vid_stream.vert[2].oargb = vid_stream.vert[3].oargb = 0;  
    vid_stream.vert[0].flags = vid_stream.vert[1].flags = vid_stream.vert[2].flags = PVR_CMD_VERTEX;         
    vid_stream.vert[3].flags = PVR_CMD_VERTEX_EOL; 

    vid_stream.vert[0].x = ul_x;
    vid_stream.vert[0].y = ul_y;
    vid_stream.vert[0].u = 0.0;
    vid_stream.vert[0].v = 0.0;

    vid_stream.vert[1].x = br_x;
    vid_stream.vert[1].y = ul_y;
    vid_stream.vert[1].u = 1.0;
    vid_stream.vert[1].v = 0.0;

    vid_stream.vert[2].x = ul_x;
    vid_stream.vert[2].y = br_y;
    vid_stream.vert[2].u = 0.0;
    vid_stream.vert[2].v = 1.0;

    vid_stream.vert[3].x = br_x;
    vid_stream.vert[3].y = br_y;
    vid_stream.vert[3].u = 1.0;
    vid_stream.vert[3].v = 1.0;

    vid_stream.initialized = 1;

    return SUCCESS;
}

static int initialize_audio() {
    if(snd_stream.initialized)
        return SUCCESS;

    /* allocate PCM buffer */
    snd_stream.decode_buffer.head = 0;
    snd_stream.decode_buffer.tail = 0;
    snd_stream.decode_buffer.size = 0;
    snd_stream.decode_buffer.capacity = AUDIO_DECODE_BUFFER_SIZE;
    snd_stream.decode_buffer.buffer = malloc(AUDIO_DECODE_BUFFER_SIZE);
    if(snd_stream.decode_buffer.buffer == NULL)
        return OUT_OF_MEMORY;
    
    /* Create a mutex to handle the double-threaded buffer */
    mutex_init(&snd_stream.decode_buffer_mut, MUTEX_TYPE_NORMAL);

    snd_stream.initialized = 1;
    
    return SUCCESS;
}

static void* player_snd_thread() {
    while(snd_stream.status != SND_STREAM_STATUS_DONE && snd_stream.status != SND_STREAM_STATUS_ERROR) {
        switch(snd_stream.status)
        {
            case SND_STREAM_STATUS_READY:
                break;
            case SND_STREAM_STATUS_RESUMING:
                snd_stream_start(snd_stream.shnd, snd_stream.rate, snd_stream.channels-1);
                snd_stream.status = SND_STREAM_STATUS_STREAMING;
                break;
            case SND_STREAM_STATUS_PAUSING:
                snd_stream_stop(snd_stream.shnd);
                snd_stream.status = SND_STREAM_STATUS_READY;
                break;
            case SND_STREAM_STATUS_STOPPING:
                snd_stream_stop(snd_stream.shnd);
                snd_stream.decode_buffer.head = 0;
                snd_stream.decode_buffer.tail = 0;
                snd_stream.decode_buffer.size = 0;
                snd_stream.status = SND_STREAM_STATUS_READY;
                break;
            case SND_STREAM_STATUS_STREAMING:
                snd_stream_poll(snd_stream.shnd);
                thd_sleep(20);
                break;
        }
    }

    return NULL;
}

static int ring_buffer_write(ring_buffer *rb, const unsigned char *data, int data_length) {
    if (data_length > rb->capacity - rb->size) {
        return 0;
    }

    for (int i = 0; i < data_length; ++i) {
        rb->buffer[rb->head] = data[i];
        rb->head = (rb->head + 1) % rb->capacity;
    }

    rb->size += data_length;
    return 1;
}

static int ring_buffer_read(ring_buffer *rb, unsigned char *data, int data_length) {
    if (data_length > rb->size) {
        return 0;
    }

    for (int i = 0; i < data_length; ++i) {
        data[i] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->capacity;
    }

    rb->size -= data_length;
    return 1;
}

static uint64_t dc_get_time() {
    uint32_t s, ms;
    uint64_t msec;

    timer_ms_gettime(&s, &ms);
    msec = (((uint64)s) * ((uint64)1000)) + ((uint64)ms);

    return msec;
}
