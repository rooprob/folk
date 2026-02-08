# Folk Live Booklet Kit

A system for creating living, interactive booklets where the pages ARE the programs.

## Concept

Traditional books document programs. Folk Live Booklets **are** the programs - they execute when placed under the projector.

**Each double-page spread:**
- **Left page**: Annotated code with step-by-step breakdown
- **Right page**: Live output and interaction
- **Both pages**: Have AprilTags for detection

The book observes itself through Folk's reactive database. You can see which pages have been read, track progress, and even have pages that react to each other.

## Structure

```
folk-live-booklet-kit/
├── README.md              # This file
├── booklet-core.folk      # Core detection & page management
├── booklet-renderer.folk  # Rendering utilities (syntax highlighting, etc.)
├── templates/             # Templates for creating new booklets
│   ├── page-left.folk     # Template for left page (annotated code)
│   ├── page-right.folk    # Template for right page (live output)
│   └── booklet.folk       # Template for booklet metadata
└── booklets/              # Actual booklets
    └── folk-february/     # Example: Folk February booklet
        ├── booklet.folk   # Metadata (title, days, etc.)
        ├── day01-left.folk
        ├── day01-right.folk
        ├── day02-left.folk
        ├── day02-right.folk
        └── ...
```

## Creating a Booklet

### 1. Create booklet metadata

```tcl
# booklets/my-booklet/booklet.folk

Claim booklet "my-booklet" has title "My Amazing Booklet"
Claim booklet "my-booklet" has page count 10
Claim booklet "my-booklet" has format A4

# Define pages
for {set i 1} {$i <= 10} {incr i} {
    Claim booklet "my-booklet" has page $i
}
```

### 2. Create page programs

**Left page (annotated code):**
```tcl
# booklets/my-booklet/page01-left.folk

When page my-booklet-01-L is visible {
    Wish $page draws title "Day 01: First Program"

    # Step-by-step breakdown
    Wish $page shows step 1 with code {
        When tag /id/ has quad /quad/ {
            puts "Tag detected!"
        }
    } explanation "This detects AprilTags"
}
```

**Right page (live output):**
```tcl
# booklets/my-booklet/page01-right.folk

When page my-booklet-01-R is visible {
    # Run the actual program here
    # This page IS the canvas
}
```

### 3. Tag Mapping System

**Physical Reality:** AprilTags are printed with numeric IDs (42, 100, 123, etc.)

**Logical Names:** Booklet pages have semantic names (`folk-february-01-L`, etc.)

**Solution:** A mapping file bridges the gap!

```tcl
# tag-mappings.folk
Claim tag 100 is page folk-february-01-L
Claim tag 101 is page folk-february-01-R
Claim tag 102 is page folk-february-02-L
...
```

**This makes booklets portable across Folk installations:**
- Installation A uses tags 100-155
- Installation B uses tags 200-255
- Installation C uses tags 500-555
- Each just updates their local `tag-mappings.folk`

**Generate mappings:**
```bash
tclsh generate-tag-mappings.tcl folk-february 28 100 tag-mappings.folk
```

## Physical Layout

Each A4 page (210mm × 297mm) contains:
- AprilTag (positioned at top center)
- Content area (with margins)
- Page number
- Navigation hints

## Observable Features

The booklet observes itself:
```tcl
Query! page /name/ is visible
Query! page /name/ has been read at /time/
Query! spread /name/ is open
Query! the current booklet is /name/
```

This enables:
- Progress tracking
- Dynamic table of contents
- Cross-page interactions
- Booklet statistics

## Example: Folk February

See `booklets/folk-february/` for a complete example implementing the 28-day Folk February challenge.
