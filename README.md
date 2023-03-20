# video_template
This serves as template C code layout for adding different video/audio decoding/streaming support to the Sega Dreamcast.  This encapsulates all the PVR and Sound work we have to do behind the scenes to make it easy for developers to produce a simple/flexible video playing api like so (main.c):

```
if(player_init()) {
    format_player_t* player = player_create("/PATH/TO/VIDEO.vid");
    player_play(player, frame_cb);
}
```

format.h/c should be replaced by the video format decoder you are going to implement.  The video decoder you implement should at least have the ability to execute these functions.

For a sample of this actually being used checkout this project: https://github.com/Dreamcast-Projects/dreamroq. (roq-player.h/c, dreamroqlib.h/c)
