## gitm

A fast C17 multi-repo Git registry manager. Maintains a list of Git repositories and provides commands to inspect, manage, and operate on them. No third-party libraries — uses a custom argparse library, system Git, and POSIX APIs.

### Features

- Zero third-party dependencies
- Custom argparse library (replaces argp)
- Fork/exec-based process execution with stdout/stderr capture
- Coloured help output (Rust clap-style)
- Configurable registry of Git repositories
- Health checks, batch operations, and per-repo exec

### Requirements

- **C17** compiler (gcc or clang)
- **POSIX** system (macOS, Linux)

---

## Build

```sh
make          # release build (-O3), outputs ./gitm
make debug -B O_DEBUG=1   # debug build (-g3, ASan, UBSan)
make clean
make install                        # install to /usr/local/bin
make install PREFIX="$HOME/.local"  # install to $HOME/.local
```

## Usage

```
gitm [OPTIONS] COMMAND [ARGS]
```

### Global Options

| Flag          | Short | Placeholder | Description                                      |
| ------------- | ----- | ----------- | ------------------------------------------------ |
| `--dry-run`   | `-n`  | —           | Show what would change without making changes    |
| `--log-level` | `-L`  | `LEVEL`     | Set log verbosity: `error`, `warn`, `info`, `debug` |
| `--log-file`  | `-F`  | `FILE`      | Set logging file                                 |
| `--edit-entry`| `-E`  | —           | Open registered_repos.txt in `$EDITOR`           |
| `--version`   | `-v`  | —           | Show version                                     |
| `--help`      | `-h`  | —           | Show help message                                |

### Commands

| Command    | Description                                             |
| ---------- | ------------------------------------------------------- |
| `list`     | List registered repositories                            |
| `add`      | Register a Git repository                               |
| `remove`   | Unregister a repository                                 |
| `rename`   | Rename a repository alias                               |
| `status`   | Show status of all registered repos                     |
| `info`     | Show repository metadata                                |
| `exec`     | Run a git command on a registered repo                  |
| `clone`    | Clone a repository and register it                      |
| `open`     | Open a repository in `$EDITOR`                          |
| `doctor`   | Health check all registered repositories                |

### Examples

```sh
# List all registered repos
gitm list

# Add a repo
gitm add /path/to/repo:my-alias

# Show status of all repos
gitm status

# Run git log on a specific repo
gitm exec my-alias log --oneline -10

# Health check
gitm doctor

# Open config file in editor
gitm --edit-entry
```

---

## Config File

Location: `~/.local/share/gitm/registered_repos.txt`

Format (one per line):
```
/Users/pritam/Developer/c_lang/gitm:gitm
/Users/pritam/Developer/c_lang/local_marks:local_marks
```

---

## Project Structure

```
gitm
├── argparse/                    # custom argument parser library (standalone)
│   ├── include/argparse.h
│   └── src/{argparse,lexer,help,error}.c
├── include/                     # all public headers
├── src/
│   ├── main.c                   # entry point: argparse init, dispatch
│   ├── commands/                # one file per subcommand + cmd.c
│   ├── config/config.c          # registry load/save/validate/add/remove
│   ├── git/
│   │   ├── process.c            # fork/exec wrapper with pipe capture
│   │   └── git.c                # high-level git helpers
│   └── util/log.c               # thread-safe logger
└── Makefile
```

---

## License

MIT — see [LICENSE](LICENSE).

---

## See Also

- [AGENTS.md](AGENTS.md) — Agent instructions for this repo
- [DEV.md](DEV.md) — Implementation plan
