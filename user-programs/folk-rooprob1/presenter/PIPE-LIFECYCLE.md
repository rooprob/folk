# Named Pipe Lifecycle in Folk Video Streaming

## The Problem

**Named pipes (FIFOs) block on open:**
- Opening for WRITE blocks until a READER opens
- Opening for READ blocks until a WRITER opens

This creates a chicken-and-egg problem for streaming!

## Solution: Non-Blocking Open with Retry

Folk uses `O_NONBLOCK` to avoid hanging, then retries until ffmpeg connects.

## Workflow Comparison

### Manual Mode (You Start ffmpeg)

```
Timeline:

1. Folk starts
   └─> Creates pipe: mkfifo /tmp/folk-stream-NAME.fifo
   └─> Tries to open: open(..., O_WRONLY | O_NONBLOCK)
   └─> Returns ENXIO (no reader yet)
   └─> Prints: "Stream pipe created, start ffmpeg listener"

2. You start ffmpeg in another terminal
   └─> ffmpeg opens pipe for reading
   └─> Pipe now has a reader!

3. Folk retries opening (next frame)
   └─> open() succeeds!
   └─> Switches to blocking mode
   └─> Streaming begins
```

**Commands:**
```bash
# Terminal 1: Start Folk
./folk
# See: "Stream pipe created for: my-stream"
# See: "/tmp/folk-stream-my-stream.fifo"

# Terminal 2: Start ffmpeg
ffmpeg -f rawvideo -pixel_format rgb24 -video_size 640x480 \
       -framerate 30 -i /tmp/folk-stream-my-stream.fifo \
       -f mpegts -codec:v mpeg1video -b:v 1000k \
       udp://localhost:8081

# Streaming begins!
```

### Auto Mode (Folk Starts ffmpeg)

```
Timeline:

1. Folk receives: Wish "my-stream" streams to "udp://localhost:8081"
   └─> Creates pipe: mkfifo /tmp/folk-stream-my-stream.fifo
   └─> Forks and execs ffmpeg
   └─> ffmpeg opens pipe for reading
   └─> Waits 100ms for ffmpeg to stabilize
   └─> Opens pipe: open(..., O_WRONLY | O_NONBLOCK)
   └─> Success! Streaming begins immediately
```

**Commands:**
```tcl
# In Folk program:
Wish "my-stream" streams to "udp://localhost:8081"

# Done! Folk handles everything
```

## Code Flow

### Pipe Creation (C code)

```c
// video-streamer.folk: createStreamPipe
snprintf(stream_path, sizeof(stream_path),
         "/tmp/folk-stream-%s.fifo", name);

unlink(stream_path);  // Remove old pipe

if (mkfifo(stream_path, 0666) != 0) {
    fprintf(stderr, "Failed to create pipe\n");
    return 0;
}

// Pipe created as special file on disk
// Anyone can now open it for reading or writing
```

### Non-Blocking Open (C code)

```c
// video-streamer.folk: openStreamPipe
stream_fd = open(stream_path, O_WRONLY | O_NONBLOCK);

if (stream_fd < 0) {
    if (errno != ENXIO) {  // ENXIO = no reader yet (OK!)
        fprintf(stderr, "Failed: %s\n", strerror(errno));
    }
    return 0;  // Will retry next frame
}

// Success! Switch to blocking for actual writes
int flags = fcntl(stream_fd, F_GETFL);
fcntl(stream_fd, F_SETFL, flags & ~O_NONBLOCK);
```

### Auto-Start ffmpeg (C code)

```c
// video-streamer.folk: startFfmpegReader
pid_t pid = fork();

if (pid == 0) {
    // Child process
    execlp("ffmpeg",
           "ffmpeg",
           "-f", "rawvideo",
           "-pixel_format", "rgb24",
           "-video_size", "640x480",
           "-framerate", "30",
           "-i", streamPath,           // The pipe!
           "-f", "mpegts",
           "-codec:v", "mpeg1video",
           "-b:v", "1000k",
           output,                     // udp://...
           NULL);
    exit(1);  // If exec fails
}

// Parent continues
ffmpeg_pid = pid;
usleep(100000);  // Give ffmpeg 100ms to open pipe
```

## Observable State

Folk makes the streaming lifecycle observable:

```tcl
# Pipe created
When stream "my-stream" has pipe created {
    puts "Pipe ready: /tmp/folk-stream-my-stream.fifo"
}

# ffmpeg started (auto mode only)
When stream "my-stream" has ffmpeg running {
    puts "ffmpeg is reading the pipe"
}

# Pipe opened successfully
When stream "my-stream" is ready {
    puts "Streaming active!"
    Wish status-card is outlined green
}

# Frame sent
When stream "my-stream" last sent frame /n/ {
    Wish status-card shows "Frame: $n"
}
```

## Who Creates What?

| Component | Who Creates | When |
|-----------|-------------|------|
| **Named pipe** | Folk (`mkfifo`) | When `Wish X streams...` |
| **ffmpeg process** | You OR Folk | Manual mode OR auto mode |
| **Pipe reader** | ffmpeg | When ffmpeg starts |
| **Pipe writer** | Folk | After reader detected |
| **UDP socket** | ffmpeg | When it starts streaming |
| **Network stream** | ffmpeg | Continuous after connection |

## Error Handling

### Pipe exists but can't open

```c
if (stream_fd < 0) {
    if (errno == ENXIO) {
        // No reader yet - this is normal, will retry
        return 0;
    } else {
        // Real error (permissions, etc)
        fprintf(stderr, "Failed to open pipe: %s\n",
                strerror(errno));
        return 0;
    }
}
```

### ffmpeg dies

```tcl
When stream "my-stream" is ready &\
     camera /cam/ has color frame /frame/ {

    if {![$streamLib writeJpegFrame $frame]} {
        # Write failed - pipe closed
        Hold! -key stream:my-stream:ready {}
        puts "Stream disconnected, waiting for reader..."

        # Will retry opening on next frame
    }
}
```

### Cleanup on exit

```tcl
Subscribe: program $this will stop {
    Notify: cleanup streams
}

Subscribe: cleanup streams {
    $streamLib stopFfmpeg      # Kill ffmpeg (SIGTERM)
    $streamLib closeStream     # Close pipe
    # Named pipe file remains for next run
}
```

## Best Practices

### ✅ Recommended: Auto Mode

```tcl
Wish "presentation" streams to "udp://localhost:8081"
```

**Pros:**
- No coordination needed
- Folk manages lifecycle
- Clean startup/shutdown
- Observable state

**Cons:**
- Folk must have ffmpeg installed
- Less control over ffmpeg options

### ✅ Also Good: Manual Mode

```tcl
# Terminal 1
./folk
# Wait for: "Stream pipe created"

# Terminal 2
ffmpeg -i /tmp/folk-stream-presentation.fifo ...
```

**Pros:**
- Full control over ffmpeg
- Can use custom options
- Can restart ffmpeg without restarting Folk

**Cons:**
- Requires terminal coordination
- Manual startup sequence

### ❌ Don't: Start ffmpeg before Folk

```bash
# DON'T DO THIS:
ffmpeg -i /tmp/folk-stream-X.fifo ...  # Pipe doesn't exist!
# Error: No such file or directory
```

Folk must create the pipe first!

## Debugging

### Check pipe exists

```bash
ls -la /tmp/folk-stream-*.fifo
# Should show: prw-rw-r-- (p = pipe)
```

### Check who has pipe open

```bash
lsof /tmp/folk-stream-my-stream.fifo
# Shows both reader (ffmpeg) and writer (folk)
```

### Monitor ffmpeg

```bash
# Start ffmpeg with verbose output
ffmpeg -loglevel debug -i /tmp/folk-stream-X.fifo ...
```

### Check network stream

```bash
# UDP stream active?
netstat -an | grep 8081

# Test receive
ffplay udp://localhost:8081
```

## Summary

**The pipe lifecycle is:**
1. Folk creates pipe (mkfifo)
2. Folk tries to open (non-blocking, may fail initially)
3. ffmpeg opens pipe (manually or auto-started)
4. Folk's next open attempt succeeds
5. Streaming active
6. On exit: Folk closes pipe, kills ffmpeg

**Key insight:** Non-blocking open with retry avoids the coordination problem!
