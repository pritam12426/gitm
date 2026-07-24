# DEV_IN_DEPTH.md

Complete source walkthrough for contributors and AI agents. This document is the single source of truth for how gitm works internally.

## Table of Contents

- [Execution Flow](#execution-flow)
- [Data Structures](#data-structures)
- [Source File Walkthrough](#source-file-walkthrough)
- [Config System](#config-system)
- [Process Execution](#process-execution)
- [Argparse Library](#argparse-library)
- [Adding New Features](#adding-new-features)
- [Edge Cases and Gotchas](#edge-cases-and-gotchas)

---

## Execution Flow

Every `gitm` invocation follows this path:

```
main()
  ├── argparse_init()
  ├── argparse_set_help_colored(1)
  ├── global_options_init()         // --log-level, --log-file, --edit-entry, -S
  ├── cmd_register_all()            // registers all 17 subcommands
  ├── log_init()                    // first init with defaults
  ├── argparse_parse()              // matches command, calls callback
  │   └── cmd_callback()            // e.g. cmd_status()
  │       ├── config_default_path()
  │       ├── config_load()
  │       ├── git_exec() per repo
  │       └── config_save() if mutated
  ├── log_reinit()                  // re-init with user's --log-level / --log-file
  └── argparse_free()
```

### Step by step

1. **`main.c`**: Creates an `ArgParser` via `argparse_init()`. Sets colored help to enabled.

2. **Global options**: Four options are registered directly on the parser (not nested):
   - `-L/--log-level=LEVEL` — log verbosity
   - `-F/--log-file=FILE` — redirect log output to file
   - `-E/--edit-entry` — open config file in `$EDITOR`
   - `-S/--shell-completion=SHELL` — output shell completion script

3. **`cmd_register_all()`** in `src/commands/cmd.c`: Calls each `cmd_register_*` function. These functions create subcommands with `argparse_subcommand()` and optionally add options with `argparse_add_option()` or `argparse_add_flag()`.

4. **`log_init()`**: Initializes the logger before parsing so that `--log-level error` suppresses info/debug messages during parse itself.

5. **`argparse_parse()`**: Tokenizes argv, matches the subcommand, calls its callback. Returns before `main()` continues if the command was `--help`, `--version`, `--shell-completion`, or `--edit-entry`.

6. **Command callback**: Each command follows the pattern:
   - Extract string options from `ArgParseResult` via `argparse_result_get()`
   - Resolve config path via `config_default_path()` or user override
   - Load config via `config_load()`
   - Perform work (iterate repos, run git, etc.)
   - If the config was mutated, call `config_save()`
   - Free config with `config_free()`

7. **`log_reinit()`**: Re-initializes the logger with user-supplied `--log-level` and `--log-file`. This ensures parsing-phase messages still use defaults while the actual command respects user choices.

8. **Cleanup**: `argparse_free()` frees all parser memory. No global state leaks.

---

## Data Structures

### `RepoEntry` (in `include/config.h`)

```c
typedef struct {
    char *path;    // absolute path to repo, e.g. "/Users/dev/project"
    char *name;    // user-assigned alias, e.g. "my-project"
    char *tags;    // comma-separated tags or NULL, e.g. "c,makefile"
    char *groups;  // comma-separated groups or NULL, e.g. "work,backend"
} RepoEntry;
```

- All fields are heap-allocated duplicates (`strdup`).
- `tags` and `groups` may be `NULL` (meaning unset).
- `tags` and `groups` are stored as raw strings; helper functions do the matching.

### `GitConfig` (in `include/config.h`)

```c
typedef struct {
    RepoEntry *entries;  // dynamic array
    size_t count;        // current number of entries
    size_t capacity;     // allocated slots
    char *path;          // filesystem path to the config file
} GitConfig;
```

- Grows by doubling capacity when `count == capacity`.

### `ArgParseResult` (in `argparse/include/argparse.h`)

```c
typedef struct {
    char *command;                    // matched subcommand name
    ArgParseStringMap *string_options; // key-value string options
    ArgParseBoolMap *bool_options;     // boolean flags
    ArgParseStringMap *positional;     // positional arguments
    ArgParseBoolMap *bool_positional;  // boolean positional args
    ArgParseStringList *unrecognized;  // unrecognized args (fallback)
} ArgParseResult;
```

- Use `argparse_result_get(result, "key")` to retrieve any value.
- Bool options return `"true"` / `"false"` as strings.

### `ArgParseConfig` (in `argparse/include/argparse.h`)

```c
typedef struct {
    const char *program_name;
    const char *program_description;
    const char *program_version;
    const char *program_epilog;
    const char *program_usage_override;
    char **example_usage_lines;
    size_t example_usage_count;
    ArgParseStringOption *string_options;
    size_t string_options_count;
    ArgParseBoolOption *bool_options;
    size_t bool_options_count;
    ArgParsePositionalOption *positional_options;
    size_t positional_options_count;
    ArgParseSubcommand *subcommands;
    size_t subcommands_count;
    // ... env, exclusive, completion function fields
} ArgParseConfig;
```

### `ProcessResult` (in `include/process.h`)

```c
typedef struct {
    char *stdout;    // captured stdout (heap-allocated, may be NULL)
    char *stderr;    // captured stderr (heap-allocated, may be NULL)
    int exit_code;   // child process exit status
} ProcessResult;
```

---

## Source File Walkthrough

### Entry Point

#### `src/main.c`

- Declares the global `Logger` instance: `Logger g_logger = { ... }`
- Calls `global_options_init()` to register `-L`, `-F`, `-E`, `-S` on the parser
- The `--edit-entry` handler opens `$EDITOR` (falling back to `vi`) with `fork()`/`execlp()` — uses raw fork, not `process_exec`, because editors require TTY access
- Calls `log_init(0, LOG_LEVEL_INFO, NULL)` before parse, `log_reinit()` after

### Command System

#### `src/commands/cmd.c`

- Contains `cmd_register_all(ArgParser *parser)` which calls all 17 `cmd_register_*` functions
- This is the only file that needs modification when adding a new command

#### Command Callback Pattern

Every command callback follows this skeleton:

```c
int cmd_example(const ArgParseResult *result) {
    // 1. Extract options
    const char *name = argparse_result_get(result, "name");
    const char *tag  = argparse_result_get(result, "tag");

    // 2. Load config
    GitConfig cfg;
    config_load(&cfg, config_default_path());

    // 3. Perform work
    for (size_t i = 0; i < cfg.count; i++) {
        RepoEntry *e = &cfg.entries[i];
        if (tag && !config_entry_has_tag(e, tag)) continue;
        // ... do something with e
    }

    // 4. Save if mutated
    // config_save(&cfg);

    // 5. Cleanup
    config_free(&cfg);
    return 0;
}
```

#### Commands That Invoke Git

Eight commands call `git_exec()` for each repo. All support `--tag`/`--group` filtering:

| Command    | File         | What it runs                               |
| ---------- | ------------ | ------------------------------------------ |
| `status`   | `status.c`   | `git status --short`                       |
| `branch`   | `branch.c`   | `git branch`                               |
| `last`     | `last.c`     | `git log --oneline -1`                     |
| `recent`   | `recent.c`   | `git log -1 --format=%ct` (for sorting)    |
| `remote`   | `remote.c`   | `git remote -v`                            |
| `list-tag` | `list_tag.c` | `git tag -l -n1`                           |
| `summary`  | `summary.c`  | `git branch` (count), `du -sh` (size)      |
| `doctor`   | `doctor.c`   | `git rev-parse --git-dir` (validity check) |

#### Commands That Modify Config

| Command  | File       | What it does                              |
| -------- | ---------- | ----------------------------------------- |
| `add`    | `add.c`    | Validates path, appends to config         |
| `remove` | `remove.c` | Finds by name, removes from config        |
| `rename` | `rename.c` | Finds by name, updates `name` field       |
| `clean`  | `clean.c`  | Finds orphans, removes after confirmation |

#### Commands That Don't Touch Config

| Command  | File       | What it does                                                |
| -------- | ---------- | ----------------------------------------------------------- |
| `info`   | `info.c`   | Reads config, prints metadata for one or all repos          |
| `exec`   | `exec.c`   | Finds repo, runs arbitrary git command via `process_exec()` |
| `open`   | `open.c`   | Finds repo, opens `$EDITOR` via raw `fork()`/`execlp()`     |
| `search` | `search.c` | Reads config, filters by substring match on name/path       |

### Config System

#### `src/config/config.c`

This is the largest source file (~600 lines). It handles everything related to the registry.

**Config path resolution:**

- `config_default_path()` returns `~/.local/share/gitm/registered_repos.txt`
- Resolved via `getenv("HOME")` — no XDG support

**File format:**

```
# Lines starting with # are comments
/Users/dev/proj1:name1:tag1,tag2:group1
/Users/dev/proj2:name2
```

- Fields: `path:name[:tags[:groups]]`
- `name` is required; `tags` and `groups` are optional
- Empty lines are skipped
- Malformed lines are logged as errors and skipped

**Loading (`config_load`):**

1. Opens file; returns 0 for missing file (empty config)
2. Reads line by line, strips newline
3. Splits by `:` — expects 2-4 fields
4. `strdup`s all fields into `RepoEntry`
5. Grows `entries[]` array by doubling capacity

**Saving (`config_save`):**

1. Opens file for writing
2. Writes each entry as `path:name:tags:groups\n`
3. Skips NULL tags/groups fields

**Tag/group helper functions:**

| Function                                             | Purpose                                             |
| ---------------------------------------------------- | --------------------------------------------------- |
| `config_entry_has_tag(entry, tag)`                   | Returns 1 if `entry->tags` contains `tag`           |
| `config_entry_has_group(entry, group)`               | Returns 1 if `entry->groups` contains `group`       |
| `config_find_by_tag(cfg, tag, &indices, &count)`     | Finds all entries matching a tag                    |
| `config_find_by_group(cfg, group, &indices, &count)` | Finds all entries matching a group                  |
| `config_find_orphans(cfg, &indices, &count)`         | Finds entries whose paths don't exist on disk       |
| `config_remove_at_indices(cfg, indices, count)`      | Removes entries at given indices (shifts remaining) |

Tag/group matching: checks if the comma-separated string _contains_ the substring (not exact token match). `config_entry_has_tag(e, "c")` matches `"c,makefile"`.

**Other operations:**

- `config_add(cfg, path, name, tags, groups)` — validates path exists and is a git repo, checks for duplicate names, appends
- `config_remove(cfg, name)` — finds by name, removes
- `config_rename(cfg, old_name, new_name)` — finds by name, updates
- `config_find(cfg, name)` — linear search by name, returns `RepoEntry*`
- `config_find_prefix(cfg, prefix)` — finds by prefix match

### Process Execution

#### `src/git/process.c`

- `process_exec(cmd, args, result)` — fork/exec with stdout+stderr capture
- Creates two pipes (stdout, stderr)
- Forks child process
- Parent reads from both pipes into fixed-size buffers (1024 bytes each — **gotcha**: output is truncated at 1024 bytes)
- Child closes unused pipe ends, redirects stdout/stderr, then `execvp(cmd, args)`
- Parent waits for child with `waitpid()`
- Result is heap-allocated and must be freed with `process_result_free()`

**Buffer limitation:** stdout and stderr are each limited to 1024 bytes. Commands that produce more output will be truncated silently. This is a known limitation.

#### `src/git/git.c`

- `git_exec(cwd, args, result)` — convenience wrapper that calls `process_exec("git", args, result)`
- `git_is_repo(path)` — runs `git -C path rev-parse --git-dir`, checks exit code
- `git_current_branch(path)` — runs `git -C path rev-parse --abbrev-ref HEAD`, strips newline

### Argparse Library

The `argparse/` directory is a standalone, reusable C library with zero dependencies.

#### `argparse/src/argparse.c`

Core parser. Key internals:

- **`ArgParser`** struct: holds all registered options, subcommands, exclusive groups, and completion function
- **`ArgParseResult`** struct: holds parsed output (command name, string/bool option maps, positional args, unrecognized args)
- **`argparse_subcommand()`**: registers a nested subcommand with its own options and callback
- **`argparse_add_option()`**: registers a `--key=VALUE` string option with short alias
- **`argparse_add_flag()`**: registers a `--name` boolean flag with short alias
- **`argparse_parse()`**: tokenizes argv, matches subcommands recursively, collects options
- **`argparse_result_get(result, key)`**: retrieves a value from any of the result maps

#### `argparse/src/lexer.c`

- Tokenizes argv into `ArgParseToken` array
- Handles `--key=value`, `--key value`, `-k value`, `-kvalue`, `--` (stop options), `--no-name` (negated bools)

#### `argparse/src/help.c`

- Generates coloured help output (rust clap style)
- Aliases shown in bold cyan
- Usage line built from registered options and positional args
- `FORCE_COLOR` env var support for testing

#### `argparse/src/error.c`

- Error messages with Levenshtein distance suggestions for unrecognized options
- Minimum distance threshold of 3 to avoid bad suggestions

---

## Adding New Features

### Adding a New Command

1. Create `src/commands/newcmd.c`:

```c
#include <stdio.h>
#include <stdlib.h>
#include "argparse.h"
#include "cmd.h"
#include "config.h"
#include "git.h"
#include "log.h"

int cmd_newcmd(const ArgParseResult *result) {
    const char *name = argparse_result_get(result, "repo");

    GitConfig cfg;
    if (config_load(&cfg, config_default_path()) != 0) {
        LOG_ERROR("failed to load config");
        return -1;
    }

    // Your logic here

    config_free(&cfg);
    return 0;
}

void cmd_register_newcmd(ArgParser *parser) {
    ArgParserConfig cmd_config = argparse_config_create();
    cmd_config.program_name = "newcmd";
    cmd_config.program_description = "Description of the new command";

    argparse_add_positional(&cmd_config, "repo", "Repository name or alias", 0);

    argparse_subcommand(parser, &cmd_config, cmd_newcmd);
}
```

2. In `src/commands/cmd.c`, add the registration call:

```c
extern void cmd_register_newcmd(ArgParser *parser);

void cmd_register_all(ArgParser *parser) {
    cmd_register_add(parser);
    cmd_register_newcmd(parser);
    // ...
}
```

3. Verify `ARGPARSE_MAX_COMMANDS` in `argparse/include/argparse.h` is large enough (currently 32).

### Adding a New Option to an Existing Command

In the command's `cmd_register_*` function, add an option after creating the subcommand config:

```c
void cmd_register_example(ArgParser *parser) {
    ArgParserConfig cmd_config = argparse_config_create();
    cmd_config.program_name = "example";
    cmd_config.program_description = "Example command";

    argparse_add_option(&cmd_config, "tag", "Filter by tag", 0, NULL);
    argparse_add_flag(&cmd_config, "verbose", "Show detailed output", 'v');

    argparse_subcommand(parser, &cmd_config, cmd_example);
}
```

In the callback, retrieve with:

```c
const char *tag = argparse_result_get(result, "tag");
const char *verbose = argparse_result_get(result, "verbose");
// verbose is "true" or "false" as a string
```

### Adding a New Global Option

In `main.c`, inside `global_options_init()`:

```c
static void global_options_init(ArgParser *parser) {
    argparse_add_option(parser, "my-option", "Description", 'm', NULL);
    argparse_add_flag(parser, "my-flag", "Description", 'M');
}
```

Retrieve in `main()` after `argparse_parse()`.

### Adding a New Helper Function to Config

In `include/config.h`, declare the function. In `src/config/config.c`, implement it. Follow the pattern:

```c
// Declaration in config.h:
int config_entry_has_tag(const RepoEntry *entry, const char *tag);

// Implementation in config.c:
int config_entry_has_tag(const RepoEntry *entry, const char *tag) {
    if (!entry->tags || !tag) return 0;
    return strstr(entry->tags, tag) != NULL;
}
```

---

## Edge Cases and Gotchas

1. **stdout/stderr truncation**: `process_exec()` captures only 1024 bytes per stream. Commands producing more output (e.g., `git log` on a large repo) will be truncated silently.

2. **Config path on missing HOME**: `config_default_path()` calls `getenv("HOME")`. If `HOME` is unset, this returns `NULL` and `config_load()` will fail. No XDG fallback.

3. **Tag/group matching is substring, not token-based**: `config_entry_has_tag(e, "c")` matches both `"c"` and `"c,makefile"`, but also `"rust,c++"`. This is by design for simplicity.

4. **`--edit-entry` and `--shell-completion` short-circuit**: These cause `main()` to return before `argparse_parse()` is called. Global options like `--log-level` are ignored in this case.

5. **Raw fork for editors**: `open.c` and the `--edit-entry` handler use raw `fork()`/`execlp()` instead of `process_exec()` because editors need TTY access (stdin for user input, stdout for display).

6. **`config_save()` overwrites the file**: The entire config is rewritten on save. There is no partial write or journaling. If the process is killed mid-write, the config may be corrupted.

7. **`clean` command confirmation**: `clean.c` shows orphaned repos and waits for user confirmation before removing. `--dry-run` prints what would be removed without making changes.

8. **`recent` command sorting**: Uses `git log -1 --format=%ct` (Unix timestamp of last commit) to sort repos. Falls back to repos without commits being listed first.

9. **Argparse completion mode**: When `--shell-completion` is used, the parser enters a special mode that prints completion candidates and exits. This is not a regular command.

10. **`ARGPARSE_MAX_COMMANDS`**: Currently set to 32. If you add more than 32 subcommands total, increase this constant in `argparse/include/argparse.h`.

---

_For build system details and coding guidelines, see [DEV.md](DEV.md)._
