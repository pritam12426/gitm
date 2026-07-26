# Changelog

All notable changes to gitm will be documented in this file.

## [1.0.0] - 2026-07-26

First stable release.

### Features

- 21 subcommands: `list`, `add`, `remove`, `rename`, `status`, `info`, `exec`, `open`, `doctor`, `recent`, `summary`, `search`, `list-tag`, `remote`, `last`, `branch`, `clean`, `stats`, `stale`, `stash`, `clone`
- Parallel per-repo data collection via pthread thread pool (10 commands)
- Thread count configurable via `GITM_THREADS` environment variable
- XDG-aware config path resolution (`$XDG_DATA_HOME`, macOS `~/Library/Application Support`, Linux `~/.local/share`)
- Tag and group filtering on any command (`--tag`, `--group`)
- Tabular output mode (`--table`) with ANSI-aware width calculation
- Coloured help output (rust clap style) with Levenshtein suggestions
- Shell completion generation (bash, zsh, fish)
- Structured logging with 7 severity levels (off–trace)
- Man page (`gitm.1`) with `make install` / `make uninstall` support
- Relative date formatting in `recent` command
- `stats` command for tag and group frequency summary
- `stale` command for repos with no recent commits
- `stash` command for stashing dirty working trees across repos
- `clone` command to clone and register repositories

### Bug Fixes

- Fixed process_exec pipe deadlock — rewritten with `poll()` for concurrent stdout/stderr reads
- Fixed 53 runtime safety issues across 23 files (realpath overflow, config_add MAX_REPOS cap, open.c raw fork+exec, SIGPIPE save/restore, double-free in clone.c, uninitialized waitpid status, OOM checks on all strdup, strncpy null-termination, config_load resource leak, config_rename OOM safety)
- Fixed config_add to enforce MAX_REPOS=50 limit
- Fixed FORCE_COLOR and CLICOLOR_FORCE env vars in child processes (respects NO_COLOR)
- Fixed config_save to write groups without tags
- Fixed process_exec_colored to set FORCE_COLOR=1/CLICOLOR_FORCE=1
- Fixed open.c to use raw fork+exec with waitpid EINTR retry
- Fixed ANSI colour leak when piping stdout

### Refactoring

- Split config.c into 5 focused modules (config, path, crud, tags, validate)
- Centralized shared utilities in share.h/share.c (constants, ANSI helpers, date parsing, cmd_cleanup, cmd_save_config)
- Centralized duplicated command boilerplate into cmd_util.h/cmd_util.c
- Reduced heap usage via stack allocation (MAX_REPOS bound enables safe stack arrays)
- Standardized short flags: `--tag=-T`, `--group=-G`, `--table=-t`
- Refactored argparse into build.c, completion.c, error.c, help.c, lexer.c

### Documentation

- Restructured docs into three audience-specific files (README.md, DEV.md, DEV_IN_DEPTH.md)
- Added man page in mdoc(7) format
- Synced all documentation with current codebase (struct definitions, API signatures, architecture)
