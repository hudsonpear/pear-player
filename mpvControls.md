If you're building a **media player with libmpv**, you can expose a very large portion of mpv's functionality. The important distinction is:

* **Commands** → actions such as `play`, `pause`, `seek`, `stop`, `screenshot`
* **Properties** → values you can read/write such as `time-pos`, `volume`, `fullscreen`
* **Events** → notifications such as playback ending, file loaded, property changed
* **Options** → mpv configuration such as hardware decoding, subtitle settings, cache, audio output
* **libmpv API** → C API used by your application to control all of the above

Below is a practical comprehensive list of what you can implement.

---

# 1. Playback commands

### Basic playback

| Command               | What it does                        |
| --------------------- | ----------------------------------- |
| `loadfile`            | Load a media file/URL               |
| `loadlist`            | Load a playlist                     |
| `stop`                | Stop playback                       |
| `quit`                | Close mpv                           |
| `quit-watch-later`    | Quit while saving playback position |
| `playlist-next`       | Next item                           |
| `playlist-prev`       | Previous item                       |
| `playlist-play-index` | Play specific playlist item         |
| `playlist-remove`     | Remove playlist item                |
| `playlist-move`       | Move playlist item                  |
| `playlist-clear`      | Clear playlist                      |
| `playlist-shuffle`    | Shuffle playlist                    |
| `playlist-unshuffle`  | Restore playlist order              |
| `playlist-play-index` | Select playlist index               |

### Pause / play

| Command         | What it does       |
| --------------- | ------------------ |
| `cycle pause`   | Toggle pause       |
| `set pause yes` | Pause              |
| `set pause no`  | Play               |
| `cycle idle`    | Control idle state |

You will generally use the `pause` property rather than creating separate play/pause functions.

---

# 2. Seeking

You can build a full seek bar.

```text
seek 10
seek -10
seek 10 relative
seek 50 absolute-percent
```

Commands/features include:

* Forward 5/10/30 seconds
* Backward 5/10/30 seconds
* Seek to exact timestamp
* Seek to percentage
* Seek to beginning
* Seek to end
* Frame stepping
* Chapter seeking
* Playlist seeking
* Exact seeking
* Relative seeking
* Absolute seeking

Useful commands:

```text
seek
frame-step
frame-back-step
```

Properties:

```text
time-pos
duration
percent-pos
playback-time
remaining
```

This gives you everything needed for:

**Timeline → current position → duration → dragging → seeking.**

---

# 3. Speed control

You can implement:

* 0.25×
* 0.5×
* 0.75×
* 1×
* 1.25×
* 1.5×
* 2×
* Custom speed
* Speed increase/decrease

Property:

```text
speed
```

Commands:

```text
multiply speed 1.1
multiply speed 0.9
```

You can therefore make something like:

> Playback Speed: `1.00×`

---

# 4. Volume / audio

You can implement a complete audio mixer interface.

### Volume

```text
volume
mute
```

Features:

* Volume 0–100
* Volume >100
* Mute
* Unmute
* Volume slider
* Volume boost

### Audio track selection

```text
audio
aid
```

You can:

* Select audio track
* Disable audio
* Automatically select audio
* Display language
* Display codec
* Display channel count

For example:

```text
Audio
├── English 5.1
├── Japanese Stereo
└── Commentary
```

---

# 5. Audio device selection

You can expose:

* Default output
* Speakers
* Headphones
* HDMI
* Bluetooth
* USB DAC
* Other audio devices

Depending on the audio backend/platform.

Properties/options around:

```text
audio-device
audio-device-list
```

This can give you an audio-device selector in your UI.

---

# 6. Audio channels

You can implement:

* Stereo
* Mono
* 5.1
* 7.1
* Channel remapping
* Downmixing
* Upmixing

Useful mpv options include audio channel configuration and remixing.

---

# 7. Audio filters

One of libmpv's biggest advantages.

You can add/remove audio filters dynamically.

Command:

```text
af
```

Possible features:

* Equalizer
* Volume
* Compressor
* Limiter
* Normalizer
* Resampler
* Channel conversion
* Echo
* Delay
* Audio visualization
* Custom filter chains

For example, your application could have:

```text
Audio Effects

☐ Equalizer
☐ Bass Boost
☐ Compressor
☐ Normalizer
☐ Night Mode
```

---

# 8. Equalizer

You can build a full EQ UI using mpv's audio filtering system.

Example:

```text
60 Hz
170 Hz
310 Hz
600 Hz
1 kHz
3 kHz
6 kHz
12 kHz
```

You can create presets:

* Flat
* Rock
* Pop
* Classical
* Bass Boost
* Vocal
* Movie
* Night

---

# 9. Subtitle system

libmpv has an extremely powerful subtitle system.

You can implement:

* Enable/disable subtitles
* Select subtitle track
* External subtitle files
* Subtitle delay
* Subtitle size
* Subtitle color
* Subtitle position
* Subtitle font
* Subtitle border
* Subtitle shadow
* Subtitle opacity
* Subtitle alignment
* Subtitle margins
* Subtitle scaling
* ASS/SSA subtitles
* Subtitle track language
* Subtitle track title

Properties include:

```text
sid
sub-delay
sub-pos
sub-scale
sub-visibility
```

---

# 10. Subtitle loading

You can dynamically add subtitles:

```text
sub-add
```

Support can include:

```text
.srt
.ass
.ssa
.vtt
```

and other formats supported by mpv/FFmpeg.

You could therefore make:

> Add Subtitle...

and allow the user to load a subtitle independently of the video.

---

# 11. Subtitle synchronization

Very useful:

```text
sub-delay
```

You can create:

```text
Subtitle Delay
[-] 0.1s [+]
```

Or:

```text
Subtitle ahead:  -2.3s
```

---

# 12. Video tracks

You can implement:

* Multiple video streams
* Alternate camera angles
* Different resolutions
* Commentary videos
* Disable video
* Select video track

Property:

```text
vid
```

---

# 13. Video filters

Another major feature.

Command:

```text
vf
```

You can implement filters such as:

* Scaling
* Cropping
* Rotation
* Debanding
* Sharpening
* Denoising
* Deinterlacing
* Color adjustment
* HDR processing
* Frame interpolation-related processing
* Format conversion

A UI could have:

```text
Video Filters

+ Add Filter

Sharpen
Denoise
Deband
Crop
Rotate
Scale
```

---

# 14. Video rotation

You can implement:

```text
0°
90°
180°
270°
```

through video filtering/options depending on how you want to handle rotation.

Useful for phone videos.

---

# 15. Crop / aspect ratio

You can implement:

* Original
* 16:9
* 4:3
* 21:9
* Custom
* Crop
* Stretch
* Letterbox

Properties/options include:

```text
video-aspect-override
video-zoom
video-pan-x
video-pan-y
```

---

# 16. Zoom

Very easy to make a zoom interface.

```text
video-zoom
```

For example:

```text
Zoom
90%
100%
110%
125%
150%
200%
```

---

# 17. Pan

When zoomed in:

```text
video-pan-x
video-pan-y
```

You can create:

```text
↑
←  → 
↓
```

or mouse-drag panning.

---

# 18. Screenshot

You can implement:

```text
screenshot
screenshot-to-file
screenshot-raw
```

Features:

* Screenshot current frame
* Save PNG
* Save JPEG
* Save screenshot to custom location
* Screenshot subtitles
* Screenshot video only
* Screenshot with OSD

Perfect for:

> Ctrl + S → Save current frame

---

# 19. Frame-by-frame playback

Commands:

```text
frame-step
frame-back-step
```

You can implement:

```text
Previous Frame
Next Frame
```

Useful for video editing and anime/movie analysis.

---

# 20. Chapters

You can display:

```text
Chapter 1 — Introduction
Chapter 2 — The Journey
Chapter 3 — Battle
Chapter 4 — Ending
```

Properties:

```text
chapter
chapter-list
chapter-metadata
```

Commands include chapter seeking.

---

# 21. A-B looping

Very useful feature.

You can create:

```text
A ───────── B
```

Commands:

```text
ab-loop
```

Use cases:

* Music practice
* Language learning
* Studying
* Repeating a scene
* Learning guitar
* Exam videos

---

# 22. Looping

You can implement:

```text
Loop Off
Loop File
Loop Playlist
```

Properties/options:

```text
loop-file
loop-playlist
```

---

# 23. Playlist

libmpv can power a full playlist system.

Features:

* Add files
* Remove files
* Reorder
* Shuffle
* Repeat
* Play next
* Play previous
* Clear
* Save playlist
* Load playlist
* Current item
* Playlist position

You can build:

```text
PLAYLIST

01  Song A.mp3
02  Song B.mp3
03  Song C.mp3
04  Song D.mp3
```

---

# 24. Media metadata

You can retrieve metadata such as:

```text
title
artist
album
album_artist
genre
date
track
disc
comment
```

This is particularly useful for a music player.

You can display:

```text
Artist
Album
Track
Genre
Artwork
```

---

# 25. Cover artwork

If the media contains embedded artwork, you can use mpv/FFmpeg-related metadata/attachments to obtain it depending on your integration.

You can build:

```text
┌───────────────┐
│   ALBUM ART   │
└───────────────┘
```

For a music player this is one of the most useful integrations.

---

# 26. Media information

You can expose detailed technical information:

### Video

* Resolution
* FPS
* Codec
* Pixel format
* HDR
* Bit depth
* Color space
* Bitrate
* Track ID

### Audio

* Codec
* Sample rate
* Channels
* Bit depth
* Bitrate
* Language

### Container

* Format
* Duration
* Chapters
* Metadata

This allows you to build an:

> Media Information

window similar to VLC.

---

# 27. Hardware decoding

This is one of libmpv's major advantages.

You can use hardware decoding through mpv/FFmpeg.

Depending on platform/GPU:

* DXVA2
* D3D11VA
* NVDEC
* VA-API
* VideoToolbox
* Vulkan-related paths
* Other FFmpeg hardware acceleration methods

This lets you build a player capable of efficiently playing high-resolution video.

---

# 28. HDR

You can expose options for:

* HDR playback
* HDR → SDR conversion
* Tone mapping
* HDR metadata
* Color management

This becomes particularly important for:

```text
HDR10
HLG
Dolby Vision
```

with actual support depending on the media, GPU, OS, and mpv/FFmpeg build.

---

# 29. Color management

You can expose:

* Gamma
* Contrast
* Brightness
* Saturation
* Hue
* Color profile
* Color space
* ICC-related color management

Useful properties include:

```text
brightness
contrast
saturation
gamma
hue
```

---

# 30. Fullscreen

You can implement:

```text
fullscreen
```

Features:

* Fullscreen
* Windowed
* Borderless fullscreen
* Fullscreen toggle

---

# 31. Window controls

Depending on your libmpv embedding architecture, you can integrate:

* Window resizing
* Window size
* Window position
* Fullscreen
* Always-on-top
* Borderless mode
* Aspect ratio handling

Some of this is controlled by your host application rather than libmpv itself.

---

# 32. OSD

mpv has an extensive on-screen display system.

You can display:

```text
▶ 01:23 / 04:32
━━━━━━━━━━●━━━━━━
```

and messages such as:

```text
Volume: 80%
Speed: 1.25x
Subtitle Delay: +0.4s
```

Commands include:

```text
show-text
show-progress
osd-msg
osd-bar
```

---

# 33. Keyboard commands

You don't have to use mpv's default UI.

Your application can map its own shortcuts.

For example:

```text
Space      Play/Pause
←          -5 sec
→          +5 sec
↑          Volume +
↓          Volume -
F          Fullscreen
M          Mute
S          Screenshot
N          Next
P          Previous
```

---

# 34. Mouse controls

You can implement:

* Click to pause
* Double click fullscreen
* Mouse wheel volume
* Mouse wheel seek
* Drag timeline
* Right click menu
* Click subtitle
* Drag video
* Zoom with Ctrl + wheel

The exact mouse interaction is implemented by your application, while libmpv provides the playback functionality.

---

# 35. URL playback

mpv/FFmpeg supports far more than local files.

Depending on protocols/build and site-specific support:

* HTTP
* HTTPS
* RTSP
* RTP
* UDP
* TCP
* Network streams
* Live streams

You can therefore build:

> Open URL...

---

# 36. Network streaming

You can build a player for:

```text
http://...
https://...
rtsp://...
```

and other supported protocols.

---

# 37. Network buffering

You can configure:

* Cache
* Demuxer cache
* Buffer size
* Readahead
* Network timeout
* Cache pause behavior

This allows you to build streaming-oriented players.

---

# 38. File formats

Because mpv is built around FFmpeg and related libraries, you get broad format support.

Typical examples:

### Video

```text
MP4
MKV
AVI
MOV
WebM
MPEG
TS
FLV
WMV
```

### Audio

```text
MP3
FLAC
AAC
WAV
OGG
Opus
M4A
WMA
ALAC
```

### Subtitles

```text
SRT
ASS
SSA
VTT
```

Exact support depends on your mpv/FFmpeg build.

---

# 39. Audio visualization

mpv can expose/use audio visualization capabilities through filters and rendering paths.

You can build:

```text
▂▃▅▇▆▄▂
```

or a spectrum analyzer.

Possible player features:

* Spectrum
* Waveform
* Oscilloscope
* Audio bars

---

# 40. External files

You can dynamically load:

* External subtitles
* External audio
* External video
* Playlist files

Useful commands include:

```text
audio-add
sub-add
video-add
```

---

# 41. Track management

You can build a track menu:

```text
VIDEO
  ✓ 1080p
  ○ 720p

AUDIO
  ✓ English
  ○ Japanese
  ○ Commentary

SUBTITLES
  ✓ English
  ○ Portuguese
  ○ Spanish
  ○ Off
```

This is one of the best uses of libmpv's property system.

---

# 42. DVD / Blu-ray / disc-like media

mpv has support for various disc/container structures depending on build and platform.

You can potentially expose:

* DVD playback
* Chapters
* Multiple audio tracks
* Multiple subtitle tracks
* Menus/playlist structures

Blu-ray support is more complicated and depends heavily on external libraries/content protection.

---

# 43. Image playback

mpv can also display images.

You can therefore create a player that handles:

```text
.jpg
.jpeg
.png
.webp
.gif
```

and image sequences.

Potential features:

* Image slideshow
* Next/previous image
* Zoom
* Pan
* Rotation

---

# 44. GIF / animated media

You can use mpv's playback engine for animated formats supported by the underlying libraries.

---

# 45. Audio-only mode

A media player can detect:

```text
MP3
FLAC
WAV
OGG
...
```

and switch to an audio UI:

```text
┌────────────────────────────┐
│                            │
│        ALBUM ART            │
│                            │
│      Song Title             │
│      Artist                 │
│                            │
│   ───────●──────────        │
│     02:31 / 04:12           │
│                            │
│  ◀   ▶   ▶   🔊             │
└────────────────────────────┘
```

---

# 46. ReplayGain

You can implement volume normalization for music collections.

Useful for preventing:

```text
Song A → very quiet
Song B → extremely loud
```

from happening when changing tracks.

---

# 47. Gapless playback

Very important for a music player.

With appropriate formats and configuration, you can make album transitions essentially seamless.

This is especially useful for:

* Live albums
* DJ mixes
* Classical albums
* Concept albums

---

# 48. Crossfade

This is where you need to distinguish **native mpv functionality from application-level functionality**.

A player can implement crossfade by coordinating two playback instances or using appropriate audio processing/architecture.

So:

> libmpv gives you the playback engine, but your application may need to implement the actual crossfade behavior.

---

# 49. Sleep timer

Not specifically a libmpv command, but extremely easy to implement.

Your application can monitor:

```text
time-pos
```

and issue:

```text
stop
quit
```

after:

```text
15 min
30 min
60 min
End of current track
```

---

# 50. Remember playback position

mpv supports watch-later functionality.

You can implement:

> Resume from 01:23:45?

This is excellent for:

* Movies
* TV shows
* Long videos
* Audiobooks

---

# 51. Screenshot hotkeys

You can provide:

```text
Ctrl + S
```

or:

```text
F12
```

for screenshots.

---

# 52. Media properties

Some of the most useful properties you'll want to observe are:

```text
filename
media-title
path

time-pos
duration
percent-pos
remaining

pause
speed
volume
mute

aid
sid
vid

chapter
chapter-metadata

width
height
video-format
video-codec
audio-codec

container-fps
estimated-vf-fps

audio-params
video-params

playlist-pos
playlist-count
```

You can observe properties through libmpv rather than constantly polling them.

---

# 53. Events

This is extremely important when writing a GUI.

libmpv can notify your application about things such as:

```text
START_FILE
FILE_LOADED
VIDEO_RECONFIG
AUDIO_RECONFIG
END_FILE
IDLE
SHUTDOWN
GET_PROPERTY_REPLY
SET_PROPERTY_REPLY
COMMAND_REPLY
PROPERTY_CHANGE
```

This allows your application to react to playback state.

For example:

```text
FILE_LOADED
     ↓
Read metadata
     ↓
Update UI
     ↓
Display duration
     ↓
Display artwork
```

---

# 54. Property observation

This is one of the most useful libmpv APIs.

Instead of asking:

> "What's the current position?"

every 100 ms, you can observe:

```text
time-pos
pause
volume
duration
track-list
playlist
```

and receive updates when they change.

---

# 55. JSON IPC concepts

If you're familiar with mpv's IPC, libmpv provides very similar control concepts directly through the API.

Commands:

```text
mpv_command()
mpv_command_async()
```

Properties:

```text
mpv_get_property()
mpv_set_property()
mpv_get_property_async()
mpv_set_property_async()
```

Observation:

```text
mpv_observe_property()
mpv_unobserve_property()
```

Events:

```text
mpv_wait_event()
```

This is the core of building your application around libmpv.

---

# 56. Rendering

If you're building your own UI, libmpv can render video into your application rather than forcing you to use the normal mpv window.

This is particularly powerful for:

* Electron
* Qt
* GTK
* SDL
* Win32
* custom OpenGL/Vulkan interfaces
* custom media players

You can have:

```text
┌──────────────────────────────────────────┐
│ Your UI                                  │
│                                          │
│            ┌──────────────┐              │
│            │              │              │
│            │   libmpv     │              │
│            │    VIDEO     │              │
│            │              │              │
│            └──────────────┘              │
│                                          │
│ Timeline   ━━━━━━━●━━━━━━━━              │
│ Controls   ◀  ▶  ▶  🔊                   │
└──────────────────────────────────────────┘
```

---

# 57. Custom UI

This is probably the biggest reason to use libmpv rather than simply launching mpv.

You can completely replace mpv's UI.

Your application can provide:

* Custom title bar
* Custom controls
* Custom playlist
* Custom media library
* Custom settings
* Custom equalizer
* Custom subtitles menu
* Custom track menu
* Custom context menu
* Custom keyboard shortcuts
* Custom animations
* Custom themes

while libmpv handles the difficult media-engine work.

---

# 58. Advanced video features

Depending on mpv/FFmpeg/build configuration, you can expose:

* Deinterlacing
* Debanding
* Dithering
* Scaling algorithms
* High-quality chroma scaling
* HDR tone mapping
* Hardware decoding
* Hardware rendering
* Frame timing
* VSync
* Interpolation
* Motion interpolation-related options
* Screenshot capture
* Color management
* ICC profiles

---

# 59. Scaling algorithms

You can give users options for video scaling.

For example:

```text
Bilinear
Bicubic
Spline
Lanczos
Sinc
EWA
```

The exact available algorithms depend on the mpv version/build.

This can make a substantial difference when scaling:

```text
480p → 1080p
720p → 1440p
1080p → 4K
```

---

# 60. Deinterlacing

You can support old interlaced video:

```text
480i
576i
1080i
```

with deinterlacing options.

---

# 61. Screenshot + frame extraction

You can make features such as:

> Save current frame

or even:

> Export frame every 1 second

The latter is application logic built around libmpv.

---

# 62. Video statistics

You can expose technical playback statistics such as:

```text
Dropped Frames
Delayed Frames
Decoder
Renderer
FPS
Bitrate
Cache
A/V Sync
```

This is excellent for a:

> Statistics / Debug Information

panel.

---

# 63. A/V synchronization

You can implement:

```text
Audio Delay
Video Delay
```

and synchronization adjustments.

Useful when:

> audio is 500 ms ahead of video

---

# 64. Audio delay

For example:

```text
Audio Delay
-0.5s
```

Useful properties/options include audio synchronization controls.

---

# 65. Video synchronization

Similarly, you can adjust video timing and synchronization behavior.

---

# 66. Screenshot of UI vs video

You can distinguish between:

**Video screenshot**

```text
screenshot
```

and your application-level screenshot:

```text
Entire player window
```

The latter is handled by your GUI framework rather than libmpv.

---

# 67. Playlist persistence

libmpv can load/save playlists, but your application can go much further:

```text
Recently Played
Favorites
Watch Later
Queue
History
Most Played
```

These are **application features**, not necessarily libmpv features.

---

# 68. Media library

This is also application-level.

You could build:

```text
Library
├── Music
├── Videos
├── Movies
├── TV Shows
├── Podcasts
└── Audiobooks
```

and use libmpv purely as the playback engine.

---

# 69. What I would implement in your player

If you're building something like your **Pyrus Player**, I wouldn't expose every obscure mpv feature in the UI.

I'd prioritize:

### Core

* Play/Pause
* Stop
* Previous/Next
* Seek
* Timeline
* Volume
* Mute
* Speed
* Fullscreen
* Playlist

### Music

* Album artwork
* Artist
* Album
* Genre
* Track number
* Metadata
* Gapless playback
* ReplayGain
* Equalizer
* Audio device
* Audio visualization

### Video

* Subtitle selection
* Subtitle delay
* Subtitle size
* Audio track selection
* Video track selection
* Aspect ratio
* Zoom
* Pan
* Crop
* Screenshot
* Frame step
* Chapters
* A-B loop

### Advanced

* Hardware decoding
* HDR
* Video filters
* Audio filters
* Color controls
* Deinterlacing
* Scaling algorithms
* Playback statistics

### Your own player features

These aren't really libmpv features, but are what would make your player substantially more useful:

* Media library
* Search
* Favorites
* Recently played
* Play history
* Queue
* Smart playlists
* Custom playlists
* Folder watching
* Album organization
* Artist pages
* Genre pages
* Resume playback
* Sleep timer
* Crossfade
* Global hotkeys
* Mini-player
* System tray
* Media keys
* Windows taskbar media controls
* Discord Rich Presence
* File tagging/editing
* Lyrics
* Last.fm/ListenBrainz integration

---

## The key thing about libmpv

You **do not need to recreate a video decoder, audio decoder, subtitle renderer, hardware acceleration system, etc.**

Your architecture can essentially be:

```text
                 YOUR PLAYER
                      │
        ┌─────────────┴─────────────┐
        │                           │
     YOUR UI                    YOUR DATABASE
        │                           │
        │                    Library / History
        │                    Playlists / Metadata
        │
        ▼
                  libmpv
                     │
       ┌─────────────┼─────────────┐
       │             │             │
     FFmpeg        Audio         Video
       │             │             │
   Decoding       Filters       Filters
       │             │             │
       └─────────────┼─────────────┘
                     │
                 Rendering
```

So if you're deciding whether **libmpv is powerful enough to be the backend of a serious Electron media player**, the answer is **yes**. It's probably overkill for a simple MP3 player, but for a player intended to handle **music + video + subtitles + streaming + hardware acceleration + advanced playback**, it's an excellent engine.

One important caveat for your Electron setup: **libmpv itself is a native library**, so the difficult part isn't the mpv commands—it is getting the native rendering/API integration working cleanly with Electron/Node. Once that layer is working, the command/property/event interface gives you an enormous amount of control.
