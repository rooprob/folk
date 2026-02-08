# Folk Live Booklet Kit - Quick Start

## Creating Your Physical Booklet

### Step 1: Generate AprilTags

**Important:** AprilTags are printed with numeric IDs (e.g., 100, 101, 102...), not names. We use a **mapping file** to associate physical tag numbers with logical page names.

#### Generate Tags

Generate sequential AprilTags at: https://april.eecs.umich.edu/software/apriltag.html
- Use tag family: 36h11
- For Folk February (28 days = 56 pages), print tags 100-155
- Include tag 99 for the cover

#### Generate Mapping File

Use the helper script:
```bash
cd folk-live-booklet-kit
tclsh generate-tag-mappings.tcl folk-february 28 100 tag-mappings.folk
```

This creates a mapping file like:
```tcl
Claim tag 100 is page folk-february-01-L
Claim tag 101 is page folk-february-01-R
Claim tag 102 is page folk-february-02-L
...
```

**This makes booklets portable!** Different Folk installations can use different tag ranges by simply editing the mapping file.

### Step 2: Create Page Layout (A4: 210mm × 297mm)

```
┌────────────────────────────────┐
│   [AprilTag]                   │  ← Top center, ~30mm from top
│   Tag ID: folk-february-01-L   │
│                                │
│   ╔══════════════════════════╗ │
│   ║  CONTENT AREA            ║ │  ← Main content region
│   ║  (margins: 20mm)         ║ │     Program code on left
│   ║                          ║ │     Live output area on right
│   ║                          ║ │
│   ╚══════════════════════════╝ │
│                                │
│   Page 1               ← →     │  ← Bottom: page # & navigation
└────────────────────────────────┘
```

### Step 3: Layout Pages

**Left Page Template:**
- AprilTag at top
- Page title: "Day XX: PROMPT"
- Step-by-step code breakdown
- Key concepts callout box
- Page number at bottom

**Right Page Template:**
- AprilTag at top
- Page title: "Day XX: PROMPT (Live)"
- Instructions for interaction
- Live output area (the actual program runs here!)
- Observable state visualization
- Page number at bottom

### Step 4: Print and Bind

1. Print pages on A4 paper
2. Bind into booklet (spiral binding works well)
3. Ensure tags are flat and clearly visible
4. Test under projector before finalizing

## Using the Booklet

### Setup

1. Load the booklet programs:
   ```bash
   # Core system
   source folk-live-booklet-kit/booklet-core.folk
   source folk-live-booklet-kit/booklet-renderer.folk

   # Folk February booklet
   source folk-live-booklet-kit/booklets/folk-february/booklet.folk
   source folk-live-booklet-kit/booklets/folk-february/day01-left.folk
   source folk-live-booklet-kit/booklets/folk-february/day01-right.folk
   # ... etc
   ```

2. Place booklet under projector

3. Open to any page spread

4. The pages detect themselves and run their programs!

### Interaction

- **Move the book** - Pages track themselves
- **Place other tags** - Programs respond to them (especially day01-trace, day02-gravity)
- **Cover pages** - Temporarily hide/show to restart programs
- **Touch pages** - Some programs may respond to proximity (future feature)

### Observability

Query the booklet's state:
```tcl
# What's currently visible?
Query! the current booklet is /name/
Query! the current page number is /num/

# What pages have been read?
Query! page /p/ was first read at /time/

# Progress tracking
Query! booklet "folk-february" has read /r/ of /t/ pages
Query! booklet "folk-february" has progress /p/ percent
```

## Creating New Pages

### Template for Left Page

```tcl
When page my-booklet-XX-L is visible &\
     page my-booklet-XX-L has quad /page/ {

    Wish $page draws title "Day XX: PROMPT"

    Wish $page shows step 1 at 0.22 with \
        code { ... } \
        title "Step Title" \
        explanation "What this does"

    # Add more steps...

    Wish $page draws page number XX
}
```

### Template for Right Page

```tcl
When page my-booklet-XX-R is visible &\
     page my-booklet-XX-R has quad /page/ {

    Wish $page draws title "Day XX: PROMPT (Live)"

    # Your actual program here
    # Use tags, display, whatever you need

    Wish $page draws page number XX
}
```

## Tips

1. **Test incrementally** - Build one spread at a time
2. **Use high-quality prints** - Tags must be sharp
3. **Good lighting** - Camera needs to see tags clearly
4. **Flat pages** - Curled pages = missed detection
5. **Tag size** - Larger tags (50-100mm) work better from distance
6. **Contrast** - White paper, black tags, good contrast

## Advanced Features

### Cross-page interactions

```tcl
# Page 5 reacts to page 3 being visible
When page my-booklet-03-R is visible &\
     page my-booklet-05-R is visible {
    # Something special happens!
}
```

### Progress-based unlocks

```tcl
# Page 10 only activates after reading pages 1-9
When booklet "my-booklet" has read /r/ of /t/ pages &\
     /r/ >= 9 &\
     page my-booklet-10-R is visible {
    # Congratulations! Secret unlocked!
}
```

### The book observes itself

```tcl
# Track reading patterns
When page /p/ is visible {
    set duration [measureReadingTime $p]
    Claim page $p was read for $duration seconds
}

# Most-read page?
Query! page /p/ was read for /longest/ seconds where {
    # No page was read longer
}
```

## Next Steps

- Create Day 03-28 for Folk February
- Add a cover page with table of contents
- Add index/glossary pages
- Create other booklets (tutorials, reference guides, etc.)
- Experiment with interactive features (touching pages, etc.)

The book is alive! 📖✨
