# Folk Presenter Media Workflow

## Media Types

The presenter system handles three types of media:

1. **Live Camera** - Real-time capture at 60fps
2. **Still Images** - Static frames (photos, slides)
3. **Pre-recorded Video** - Video files to play back

## Frame Rate Strategy

### Source Frame Rates

| Source | Native FPS | Stream FPS | Method |
|--------|-----------|------------|---------|
| Camera | 60fps | 30fps | Decimate (send every other frame) |
| Still Images | N/A | 30fps | Repeat same frame continuously |
| Video Files | Various | 30fps | Re-encode before playback |

### Why 30fps for Streaming?

- **Bandwidth**: Half the data of 60fps
- **CPU**: Easier encoding/decoding
- **Compatibility**: Standard web streaming rate
- **Quality**: Smooth enough for presentations
- **Consistency**: All sources normalized to same rate

## Camera Streaming (60fps → 30fps)

### Implementation

```tcl
When stream "my-stream" is ready &\
     camera /cam/ has color frame /frame/ at timestamp /ts/ {

    # Throttle from 60fps to 30fps
    set targetFps 30
    set frameNum [expr {int($ts * $targetFps)}]

    # Only send when frame number changes
    set lastFrame [Query! stream "my-stream" last sent frame /f/]
    if {[llength $lastFrame] == 0 ||
        [dict get [lindex $lastFrame 0] f] != $frameNum} {

        # Send this frame
        $streamLib writeJpegFrame $frame

        # Track last sent
        Claim stream "my-stream" last sent frame $frameNum
    }
}
```

### Effect

- Camera runs at 60fps
- Folk receives all 60 frames
- Only sends 30 frames/sec to stream
- Result: Smooth 30fps output with low latency

## Still Images (Hold and Repeat)

### The Problem

If you show a still image and stop sending frames:
- Stream freezes
- ffmpeg buffers empty
- Connection may timeout
- Viewer sees frozen frame or black screen

### Solution: Continuous Sending

```tcl
When /someone/ wishes "presentation" streams image /image/ {
    # Hold this as the current frame
    Claim stream "presentation" has current image $image
}

When stream "presentation" has current image /img/ {
    # Send the same frame repeatedly at 30fps
    # This keeps the stream alive
    $streamLib writeJpegFrame $img
}
```

### Effect

- Image displayed on media card
- Same frame sent 30 times/second
- Stream stays active
- Viewer sees stable image
- No bandwidth wasted (ffmpeg compresses duplicates well)

## Pre-recorded Video Files

### Challenge

Video files may have:
- Different frame rates (24, 25, 30, 60 fps)
- Different resolutions
- Different codecs
- Variable frame rate

### Solution: Pre-process to 30fps

#### Option 1: Pre-encode All Videos

```bash
# Convert any video to 30fps raw RGB for Folk
ffmpeg -i input.mp4 \
       -vf "fps=30,scale=640:480" \
       -f rawvideo \
       -pix_fmt rgb24 \
       output.rgb

# Store metadata
echo "640 480 30" > output.rgb.meta
```

Then in Folk:
```tcl
# Read pre-encoded video
set videoData [readFile "videos/demo.rgb"]
set meta [readFile "videos/demo.rgb.meta"]
lassign $meta width height fps

# Play back frame-by-frame
for {set i 0} {$i < [totalFrames]} {incr i} {
    set frameData [extractFrame $videoData $i $width $height]
    Wish "presentation" streams image $frameData
    after [expr {1000 / $fps}]
}
```

#### Option 2: Real-time Re-encoding

Use ffmpeg to decode video and pipe to Folk:

```bash
# Decode video to pipe at 30fps
ffmpeg -re -i input.mp4 \
       -vf "fps=30,scale=640:480" \
       -f rawvideo \
       -pix_fmt rgb24 \
       - | folk-video-receiver
```

Folk receives pre-normalized frames.

#### Option 3: Folk-managed Video Player (Future)

```tcl
# Future: Video player that manages decoding
Claim tag 42 is a video player
Wish tag 42 plays video "demo.mp4"

When tag 42 plays video /path/ at frame /n/ {
    # Folk decodes using ffmpeg subprocess
    # Sends frames to stream at correct rate
    # Handles seeking, pause, etc.
}
```

## Complete Media Card Workflow

### Camera Card

```tcl
# media-card-camera.folk
set this camera-card

Wish $this has color camera image

When $this has color camera image &\
     camera /cam/ has color frame /frame/ at timestamp /ts/ {

    # Show preview on card (60fps)
    Wish to draw an image onto $this with image $frame

    # Claim as media card (30fps for stream)
    set frameNum [expr {int($ts * 30)}]
    Claim $this is a mediacard with image $frame at timestamp $frameNum
}
```

### Still Image Card

```tcl
# media-card-slide.folk
set this slide-card

# Load image from file
set slideImage [loadImage "slides/slide01.png"]

Wish to draw an image onto $this with image $slideImage

# Claim as media card
Claim $this is a mediacard with image $slideImage at timestamp 0
```

### Video Card (Future)

```tcl
# media-card-video.folk
set this video-card

Wish $this plays video "demo.mp4"

When $this plays video /path/ {
    # Decode video at 30fps
    # Update current frame
    # Claim as media card
}
```

## Presenter Integration

### Nearest Card Display

```tcl
# Uses table-plane to find closest card
When nearest-media-card is closest to card /card/ &\
     /card/ is a mediacard with image /img/ at timestamp /ts/ {

    # Display on presenter
    Wish to draw image onto nearest-media-card with image $img

    # Stream to network (30fps, repeated for stills)
    Wish "presentation" streams image $img
}
```

### Streaming Behavior

| Card Type | Image Updates | Stream Behavior |
|-----------|---------------|-----------------|
| **Camera** | 60fps | Send every other frame (30fps) |
| **Still** | Once | Repeat same frame (30fps) |
| **Video** | 30fps (future) | Send each frame once (30fps) |

## Audio Support (Future)

### Current: Video Only

```
Camera → Folk → Named Pipe → ffmpeg → UDP (video only)
```

### Future: Video + Audio

```
Camera Video → Folk → Video Pipe → \
                                    → ffmpeg → Mux → UDP
Microphone → Folk → Audio Pipe → /
```

Implementation:
```tcl
# Create two pipes
Wish "presentation-video" streams camera frames
Wish "presentation-audio" streams microphone samples

# ffmpeg reads both
ffmpeg -f rawvideo -i /tmp/folk-stream-presentation-video.fifo \
       -f s16le -ar 48000 -ac 2 -i /tmp/folk-stream-presentation-audio.fifo \
       -f mpegts udp://localhost:8081
```

Challenges:
- Audio/video sync
- Sample rate conversion
- Buffer management
- Latency matching

## Performance Considerations

### Bandwidth

| Format | FPS | Resolution | Bandwidth |
|--------|-----|-----------|-----------|
| Raw RGB | 30 | 640×480 | ~28 MB/s |
| Raw RGB | 30 | 1280×720 | ~66 MB/s |
| Raw RGB | 60 | 640×480 | ~55 MB/s |

Named pipes are local, so this is fine. But for network streaming, ffmpeg compresses to ~1 Mbps.

### CPU Usage

- **Folk**: Minimal (just copying frames)
- **ffmpeg**: Moderate (encoding mpeg1video)
- **Total**: ~10-20% CPU on modern hardware

### Latency

- **Camera to pipe**: <16ms (one frame @ 60fps)
- **Pipe to ffmpeg**: <1ms (local)
- **ffmpeg encode**: ~5-10ms
- **Network**: ~5-50ms (depends on network)
- **Total**: ~50-100ms end-to-end

Good enough for live presentations!

## Example: Complete Presentation

```tcl
# Load system
source table-plane.folk
source presenter/video-streamer.folk
source presenter/media-card-camera.folk
source presenter/nearest-media-card.folk

# Set origin
Claim tag 0 defines plane coordinates

# Auto-start streaming
Wish "live-presentation" streams to "udp://localhost:8081" \
    with width 640 height 480 fps 30

# Present nearest card
When nearest-media-card is closest to card /card/ &\
     /card/ is a mediacard with image /img/ {

    # Stream it (handles camera, stills, video uniformly)
    Wish "live-presentation" streams image $img
}
```

Viewer:
```bash
ffplay -fflags nobuffer -flags low_delay udp://localhost:8081
```

## Best Practices

### ✅ DO:
- Stream at 30fps (good quality/performance balance)
- Repeat still frames continuously
- Pre-process videos to 30fps
- Use table-plane for spatial interactions
- Monitor stream status with observable claims

### ❌ DON'T:
- Stream at 60fps (wastes bandwidth)
- Stop sending frames when showing stills (breaks stream)
- Mix frame rates in same stream
- Use screen-pixel distance (use table-plane meters!)

## Troubleshooting

### Stream freezes on still images

**Problem:** Stopped sending frames

**Solution:** Use continuous send pattern:
```tcl
Wish "stream" streams image $stillImage
# This sends $stillImage repeatedly at 30fps
```

### Jittery playback

**Problem:** Variable frame rate

**Solution:** Ensure consistent 30fps throttling:
```tcl
set frameNum [expr {int($ts * 30)}]
# Only send when frameNum changes
```

### High CPU usage

**Problem:** Streaming too fast or too high resolution

**Solution:**
- Reduce to 30fps
- Lower resolution: 640×480 instead of 1280×720
- Use hardware encoding if available

### Out of sync

**Problem:** Audio/video drift (future)

**Solution:**
- Timestamp each frame/sample
- Buffer management in ffmpeg
- Resync periodically
