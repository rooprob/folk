# CLAUDE.md

Claude Code guidance for this repository.

## Architecture Overview

Folk is a physical computing system combining a reactive database, programming environment, and projection mapping. It implements a natural-language Datalog reactive database in Tcl and C.

**Core runtime:** [folk.c](folk.c) manages threads, work queues, database operations
**Database:** [db.c](db.c)/[db.h](db.h) with custom trie indexing ([trie.c](trie.c)/[trie.h](trie.h))
**Tcl runtime:** [prelude.tcl](prelude.tcl) handles unknown commands, lexical environments
**Boot system:** [boot.folk](boot.folk) initializes and loads programs

### Directory Structure

- [lib/](lib/) - Core Tcl/C libraries (c.tcl for C FFI, terminal.tcl, ws.tcl)
- [builtin-programs/](builtin-programs/) - System .folk programs (apriltags, display, editor, keyboard)
- [user-programs/](user-programs/) - User programs organized by hostname
- [vendor/](vendor/) - Third-party dependencies (jimtcl, apriltag, tracy)
- [test/](test/) - Test programs

## Building and Testing

See [Makefile](Makefile) for all targets. Common commands:

```bash
make deps          # Install dependencies (run once)
make && ./folk     # Build and run locally
make test          # Run all tests
make test/filename # Run specific test
make debug         # Debug with gdb
make remote FOLK_REMOTE_NODE=hostname  # Build and run remotely
```

## Folk Language

Folk extends Tcl with reactive database constructs:

**Claim** - Assert facts: `Claim $this has value 3`
**When** - Reactive pattern matching: `When /x/ has value /v/ { ... }`
**Wish** - Express desired outcomes: `Wish $region is outlined`
**Hold!** - Persistent state: `Hold! -on $this Claim $x is $y`

**Pattern syntax:**
- `/variable/` - Bind pattern variable (like Datalog)
- `/someone/, /something/` - Non-capturing wildcards
- `/nobody/, /nothing/` - Negation patterns
- `&` - Join patterns: `When /x/ is /v/ & /x/ has color /c/ { ... }`

## Code Style

**Tcl conventions:**
- Use `fn` for lexically captured commands, not `proc`
- Prefer `try`/`on error` over `catch`
- Use `apply` not `subst` for lambda construction
- Complete sentences: `Claim $this has value 3` not `Claim $this value 3`
- Scope with `$this` to prevent global collisions
- Multi-line patterns: `When /x/ is /y/ &\` with backslash continuation

**C conventions:**
- Thread-local storage: `__thread` for `interp`, `cache`, `self`
- Work queues for cross-thread communication
- Epoch-based memory management

## System Architecture

**Threading:** Main database thread distributes work via lock-free mpmc queues to worker threads with thread-local Tcl interpreters.

**Program loading:** [boot.folk](boot.folk) loads .folk files from `builtin-programs/` and `user-programs/[hostname]/`. Programs assert code into database; Assert! handler evaluates them.

**C FFI:** [lib/c.tcl](lib/c.tcl) allows inline C compilation and dynamic loading at runtime.

**Pattern indexing:** [trie.c](trie.c) indexes statements word-by-word for efficient pattern matching with wildcards at any position.

## Key Implementation Details

- Builtin programs are primary extension mechanism - avoid adding .tcl files to Git
- Statement indexing supports wildcards at any position
- System designed for projector-camera calibration and physical computing
- Web server provides program editing at `/programs`, calibration at `/calibrate`