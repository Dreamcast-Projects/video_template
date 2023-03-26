<!-- PROJECT LOGO -->
<br />
<div align="center">
  <h1 align="center">video_template</h3>
</div>

<!-- ABOUT THE PROJECT -->
## About The Project

This project serves as template C code layout for adding different video/audio decoding/streaming support to the Sega Dreamcast using [KOS](https://github.com/KallistiOS/KallistiOS).  This encapsulates all the PVR and sound work we have to do behind the scenes to make it easy for developers to produce a simple/flexible video playing api with support to pause, stop, loop, and adjust audio volume. All that you need to do is create a decoder library for your video format and fill out the stub functions listed in format.h.


## Instructions

1. Use format.h/c as a starting point to build your decoder and fill out ALL the stub functions. 
2. Make sure you decode the video to RGB565, and your audio to nothing higher than 44100 Hz.
3. Depending on your video and audio callbacks you implement in your decoder, you may have to edit the format-player.c file to support those by editing the function prototypes/implementations format_video_cb(), format_audio_cb().

For a sample of this actually being used checkout this [project](https://github.com/Dreamcast-Projects/dreamroq). Look at (roq-player.h/c, dreamroqlib.h/c)


<!-- LICENSE -->
## License

Distributed under the BSD 2 License. See `LICENSE` for more information.
