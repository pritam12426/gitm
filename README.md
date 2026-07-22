## gitm

gitm is a lightweight command-line utility for inspecting, querying, and managing Git repositories. It is written in C and communicates directly with the Git executable using `fork()` and `exec()`, avoiding third-party libraries while remaining fully compatible with the user's installed Git version.

The project is designed around Git's plumbing commands and machine-readable output, making it fast, reliable, and suitable for automation, scripting, and working with one or many repositories.

### Features

- Zero third-party dependencies
- Uses the system Git executable
- Built with Git plumbing commands
- Fast and lightweight
- Script-friendly
- Supports multiple repositories
- Clean and portable C code
- Compatible with modern Git features such as worktrees, submodules, and sparse checkouts

### Philosophy

Rather than reimplementing Git internals, <PROJECT_NAME> treats Git as the source of truth and focuses on providing a clean, efficient interface for retrieving and organizing repository information.


## Requirements

- **C17** compiler (gcc or clang)
- **argp** — built into glibc on Linux; install via `brew install argp-standalone` on macOS

---

## Build

```sh
make help     # show available targets
make                                # optimised release build -O3
make debug -B O_DEBUG=1             # debug build with -g3 -DDEBUG
make install                        # install to /usr/local/bin (use PREFIX= to override)
make install PREFIX="$HOME/.local"  # install to $HOME/.local
make clean
```

## Usage

```
main-binary [OPTION...] [TARGET(s)...]
```

### Options

| Flag          | Short | Place shoulder | Description                                                   |
| ------------- | ----- | -------------- | ------------------------------------------------------------- |
| `--dry-run`   | `-n`  | —             | Show what would change without making any changes             |
| `--log-level` | `-L`  | `LEVEL`        | Set log verbosity: `error`, `warn`, `info` (default), `debug` |
| `--log-file`  | `-F`  | `FILE`         | Set logging file                                              |

### Examples

```sh
# See what would be synced without making changes
main-binary --dry-run

# Verbose debug output
main-binary --log-level debug
```

---

## Project structure

```
./main-binary
└── src/
│   ├── main.c            # CLI argument parsing, sync loop
│   ├── log.h             # LOG_ERROR / LOG_WARN / LOG_INFO / LOG_DEBUG macros
│   ├── log.c             # log_record() implementation
│   └── project_config.h  # Version, name, global rclone options
└── Makefile
```

---

## License

MIT — see [LICENSE](LICENSE).

---

## See Also

- [PROJECT_BRIEF.md](PROJECT_BRIEF.md) — Architecture, module guide, mental model
- [AGENTS.md](AGENTS.md) — Agent instructions for this repo
