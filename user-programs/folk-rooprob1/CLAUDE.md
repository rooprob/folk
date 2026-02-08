# folk-rooprob1 User Workspace

This directory contains experimental Folk projects demonstrating the expressive capabilities of the Folk reactive database system.

## Project Overview

### [vim-editor/](vim-editor/)
**Status:** In development
**Goal:** Create a vim-inspired editor experience within Folk's projection mapping environment

A Tcl implementation of vim editing modes and keybindings, designed to work with Folk's physical computing model. The editor appears above keyboard AprilTags and responds to physical keyboard input.

**Key features:**
- Modal editing (normal, insert, visual modes)
- Vim keybindings and commands
- Integration with Folk's physical tag system
- Canvas-based text rendering with Folk's display system
- Pattern-based reactive state management

**Architecture:**
- [vim-editor.folk](vim-editor/vim-editor.folk) - Main editor program with When patterns for keyboard interaction
- [vim-editor-utils.folk](vim-editor/vim-editor-utils.folk) - Library of editing utilities and text manipulation functions

**Development approach:** Uses Folk's reactive patterns (`When`, `Claim`, `Wish`) to coordinate between physical keyboards (AprilTags), editor state, and display rendering.

### [inktober/](inktober/)
**Status:** Planned
**Goal:** Month-long creative coding project demonstrating Folk's expressive language

A series of daily programs exploring Folk's capabilities for generative art, interactive installations, and physical computing experiments. Each day builds on Folk's reactive database primitives to create novel experiences.

**Planned themes:**
- Generative visual patterns with projection mapping
- Interactive physical-digital installations
- Multi-tag coordination and spatial computing
- Real-time data visualization
- Experimental UI paradigms

**Format:** Daily .folk programs (day01.folk through day31.folk) showcasing different aspects of the Folk system.

### [presenter/](presenter/)
**Status:** Planned
**Goal:** Build presentation software natively in Folk

A projection-based presentation system leveraging Folk's physical computing capabilities. Instead of traditional slide-based presentations, create dynamic, spatially-aware presentations that respond to physical objects and gestures.

**Planned features:**
- Slide management with AprilTag navigation
- Physical slide markers (tags as slide indices)
- Live code editing during presentations
- Spatial transitions between content
- Integration with Folk's calibration system for multi-projector setups

**Design philosophy:** Presentations as reactive Folk programs rather than static slides, enabling live demos, code exploration, and physical interaction.

## Development Guidelines

**File naming:** Use descriptive names: `project-name.folk` for main programs, `project-name-utils.folk` for libraries.

**Scope management:** Always scope claims with `$this` or project-specific identifiers to avoid conflicts with builtin programs.

**Physical integration:** Design with Folk's projector-camera system in mind - consider how programs will appear when projected onto physical surfaces.

**Reactive patterns:** Leverage Folk's pattern matching for state management rather than traditional imperative approaches.

## Related Builtin Programs

See main [CLAUDE.md](../../../CLAUDE.md) for core Folk documentation.

**Relevant builtin programs:**
- [builtin-programs/editor.folk](../../../builtin-programs/editor.folk) - Original Folk editor (reference implementation)
- [builtin-programs/keyboard.folk](../../../builtin-programs/keyboard.folk) - Keyboard input handling
- [builtin-programs/display/](../../../builtin-programs/display/) - Display system and rendering
- [builtin-programs/apriltags.folk](../../../builtin-programs/apriltags.folk) - AprilTag detection

## Getting Started

```bash
# From Folk root directory
make && ./folk

# Programs load automatically from user-programs/folk-rooprob1/
# Edit programs while Folk is running - they reload automatically
```

Each project is a self-contained Folk program that integrates with the core system through reactive database patterns.
