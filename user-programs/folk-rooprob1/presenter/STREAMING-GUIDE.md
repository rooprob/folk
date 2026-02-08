# Folk Video Streaming Guide

## Overview

The presenter system includes video streaming support for broadcasting Folk content to external viewers.

## Architecture

```
Camera → Folk → Named Pipe → ffmpeg → Network Stream → Viewer
         ↓
    Media Cards
         ↓
    Nearest Card Display
```

## Components

### 1. video-streamer.folk

Core streaming infrastructure:
- Creates named pipes (FIFOs) for frame data
- Handles JPEG encoding (or raw RGB)
- Manages pipe I/O and error handling
- Provides observable streaming state

**Key Claims:**
- `stream NAME has pipe created`
- `stream NAME is ready`
- `stream NAME last sent frame N`

### 2. media-card-camera.folk

Camera capture as media card:
- Displays camera preview on card
- Claims to be a mediacard with current frame
- Supports streaming on demand

**Key Claims:**
- `CARD is a mediacard with image IMG at timestamp TS`
- `CARD is streaming as NAME at timestamp TS`

### 3. nearest-media-card.folk

Presenter display:
- Uses table-plane for spatial awareness
- Finds physically closest media card
- Displays that card's content
- Can stream the selected card

## Usage

### Basic Setup

1. **Load the system:**
```tcl
source user-programs/folk-rooprob1/table-plane.folk
source user-programs/folk-rooprob1/presenter/video-streamer.folk
source user-programs/folk-rooprob1/presenter/media-card-camera.folk
source user-programs/folk-rooprob1/presenter/nearest-media-card.folk
```

2. **Create a camera media card** (print this tag):
```tcl
# Tag will automatically capture camera frames
```

3. **Create a presenter display** (print this tag):
```tcl
# Will show the nearest media card
```

### Streaming

#### Option 1: Stream Camera Directly

```tcl
# In any program:
Wish "my-presentation" streams camera frames
```

Start ffmpeg listener:
```bash
ffmpeg -f rawvideo -pixel_format rgb24 -video_size 640x480 \
       -framerate 30 -i /tmp/folk-stream-my-presentation.fifo \
       -f mpegts -codec:v mpeg1video -s 640x480 -b:v 1000k -bf 0 \
       udp://localhost:8081
```

#### Option 2: Stream Selected Media Card

```tcl
# When nearest-media-card selects a card, stream it:
When nearest-media-card is closest to card /card/ {
    Wish "presentation" streams image from $card
}
```

#### Option 3: Conditional Streaming

```tcl
# Only stream when presenter is near a card
When nearest-media-card has distance /dist/ to /card/ &\
     /dist/ < 0.2 &\
     /card/ is a mediacard with image /img/ {
    Wish "presentation" streams image $img
}
```

### Viewing the Stream

#### Option 1: JSMpeg Web Viewer

1. Install jsmpeg:
```bash
npm install -g jsmpeg
```

2. Start relay server:
```bash
node relay-server.js 8081 8082
```

3. Open browser:
```
http://localhost:8082
```

#### Option 2: VLC

```bash
vlc udp://@:8081
```

#### Option 3: ffplay

```bash
ffplay -fflags nobuffer -flags low_delay -framedrop \
       -i udp://localhost:8081
```

## Advanced Usage

### Multiple Streams

```tcl
# Stream different content on different channels
Wish "camera-1" streams camera frames
Wish "presentation" streams selected media card
Wish "debug-view" streams overlay graphics
```

### Observable Streaming State

```tcl
# React to streaming status
When stream "presentation" is ready {
    puts "Stream is live!"
    Wish presenter-card is outlined green
}

When stream "presentation" last sent frame /n/ {
    Wish status-display shows "Frame: $n"
}
```

### Stream Quality Control

Edit video-streamer.folk to adjust:
- Frame rate throttling (currently ~30 FPS)
- Resolution
- Encoding format (raw RGB vs JPEG)
- Buffer sizes

## Troubleshooting

### Pipe not opening

**Problem:** `Failed to open stream pipe: No such device or address`

**Solution:** Start ffmpeg BEFORE Folk creates the pipe, or use non-blocking open (already implemented).

### Frame lag

**Problem:** Frames arrive delayed

**Solutions:**
- Reduce ffmpeg buffer: `-fflags nobuffer -flags low_delay`
- Increase Folk frame rate
- Use UDP instead of TCP
- Reduce resolution/quality

### No frames received

**Check:**
1. Is pipe created? `ls -l /tmp/folk-stream-*.fifo`
2. Is ffmpeg reading? `lsof /tmp/folk-stream-*.fifo`
3. Are frames being sent? Check Folk output
4. Is network path open? `netstat -an | grep 8081`

## Example: Complete Presentation Setup

```tcl
# presenter-demo.folk

# Load dependencies
source table-plane.folk
source presenter/video-streamer.folk
source presenter/media-card-camera.folk
source presenter/nearest-media-card.folk

# Set table origin
Claim tag 0 defines plane coordinates

# Stream the nearest card
When nearest-media-card is closest to card /card/ at distance /dist/ &\
     /card/ is a mediacard with image /img/ {

    # Only stream when card is close (within 30cm)
    if {$dist < 0.3} {
        Wish "live-presentation" streams image $img

        # Visual feedback
        Wish nearest-media-card is outlined green
        Wish $card is outlined cyan
    }
}

# Show streaming status
When stream "live-presentation" is ready {
    Claim presentation is live
}

When presentation is live {
    Wish to draw text onto display "LIVE" \
        with position {10 10} color red size 24
}
```

Start streaming:
```bash
# Terminal 1: Start ffmpeg
ffmpeg -f rawvideo -pixel_format rgb24 -video_size 640x480 \
       -framerate 30 -i /tmp/folk-stream-live-presentation.fifo \
       -f mpegts -codec:v mpeg1video -s 640x480 -b:v 1000k -bf 0 \
       udp://localhost:8081

# Terminal 2: Start Folk
./folk

# Terminal 3: View stream
ffplay udp://localhost:8081
```

## Future Enhancements

- [ ] JPEG encoding (currently raw RGB)
- [ ] Resolution auto-detection
- [ ] WebRTC support
- [ ] Stream recording
- [ ] Multi-camera support
- [ ] Audio capture/streaming
- [ ] Stream overlays (graphics, text)
