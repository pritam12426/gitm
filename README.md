# gitm

A fast C17 multi-repo Git registry manager. Maintains a list of Git repositories and provides commands to inspect, manage, and operate on them.

Zero third-party dependencies. Uses a custom argument parser, system Git, and POSIX APIs.

## Features

- **Registry of Git repositories** — register repos with aliases, tags, and groups
- **Batch operations** — run status, branch, log, and remote queries across all repos
- **Tag and group filtering** — filter any command by tag or group
- **Tabular output** — `--table` flag for aligned, pipe-separated columns
- **Orphan cleanup** — find and remove repos that no longer exist on disk
- **Health checks** — verify all registered repos are valid Git repositories
- **Per-repo exec** — run arbitrary Git commands on a specific repo
- **Shell completion** — built-in bash, zsh, and fish completion generation
- **Coloured output** — Rust clap-style coloured help and status output
- **Structured logging** — 7 severity levels (off, fatal, error, warn, info, debug, trace)

## Requirements

- **C17** compiler (gcc or clang)
- **POSIX** system (macOS, Linux)
- **Git** installed and available in `$PATH`

## Build

```sh
make            # release build (-O3), outputs ./gitm
make debug -B   # debug build (-g3, ASan, UBSan)
make clean      # remove build artifacts
make install    # install to /usr/local/bin
make install PREFIX="$HOME/.local"  # custom prefix
```

## Quick Start

```sh
# Register a repository
gitm add /path/to/repo my-alias

# Register with tags and groups
gitm add --tag c,project --group learning /path/to/repo my-alias

# List all registered repos
gitm list

# List repos in tabular format
gitm list --table

# Filter by tag or group
gitm list --tag c
gitm list --group learning

# Show status of all repos
gitm status
gitm status --table

# Run a git command on a specific repo
gitm exec my-alias log --oneline -10

# Health check
gitm doctor --table

# Open config file in $EDITOR
gitm --edit-entry
```

## Commands

| Command    | Aliases      | Description                               | `--table` |
| ---------- | ------------ | ----------------------------------------- | --------- |
| `list`     | `ls`         | List registered repositories              | Yes       |
| `add`      | —            | Register a Git repository                 | —         |
| `remove`   | `rm`         | Unregister a repository                   | —         |
| `rename`   | —            | Rename a repository alias                 | —         |
| `status`   | `st`, `s`    | Show status of all registered repos       | Yes       |
| `info`     | `i`          | Show repository metadata                  | —         |
| `exec`     | `x`          | Run a git command on a registered repo    | —         |
| `open`     | `o`          | Open a repository in `$EDITOR`            | —         |
| `doctor`   | `doc`, `d`   | Health check all registered repositories  | Yes       |
| `recent`   | `r`          | List repos sorted by last commit date     | Yes       |
| `summary`  | `sum`        | Total repos, branches, and size           | —         |
| `search`   | `find`, `f`  | Search repos by name or path pattern      | —         |
| `list-tag` | `tags`, `lt` | Show git tags with messages               | Yes       |
| `remote`   | `rem`        | Show remote settings                      | Yes       |
| `last`     | `log`, `l`   | Show last commit log for each repo        | Yes       |
| `branch`   | `br`, `b`    | Show local branches                       | Yes       |
| `clean`    | `prune`      | Remove repos that no longer exist on disk | —         |

All commands that iterate repositories support `--tag` and `--group` filters.

## Global Options

| Flag                       | Short | Description                                                                  |
| -------------------------- | ----- | ---------------------------------------------------------------------------- |
| `--log-level=LEVEL`        | `-L`  | Set log verbosity: `off`, `fatal`, `error`, `warn`, `info`, `debug`, `trace` |
| `--log-file=FILE`          | `-F`  | Set logging file                                                             |
| `--edit-entry`             | `-E`  | Open registered_repos.txt in `$EDITOR`                                       |
| `--help`                   | `-h`  | Show help message                                                            |
| `--version`                | `-v`  | Show version                                                                 |
| `--shell-completion=SHELL` | `-S`  | Output completion script (bash, zsh, fish)                                   |

## Configuration

Config file: `~/.local/share/gitm/registered_repos.txt`

Format (one entry per line):

```
/path:name:tag1,tag2:group1,group2
```

Fields after `name` are optional. Example:

```
/Users/dev/project-a:project-a:c,makefile:work,backend
/Users/dev/project-b:project-b:rust:learning
```

Lines starting with `#` are comments. Empty lines are ignored.

## Platform Support

| Platform | Status                              |
| -------- | ----------------------------------- |
| macOS    | Supported                           |
| Linux    | Supported                           |
| Windows  | Not supported (requires POSIX APIs) |

## License

MIT — see [LICENSE](LICENSE).

## Documentation

- [DEV.md](DEV.md) — architecture, build system, and developer guide
- [DEV_IN_DEPTH.md](DEV_IN_DEPTH.md) — complete source walkthrough and internals
