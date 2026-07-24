# DEV_IN_DEPTH.md

Complete source walkthrough for contributors and AI agents. This document is the single source of truth for how gitm works internally.

## Table of Contents

- [Execution Flow](#execution-flow)
- [Data Structures](#data-structures)
- [Source File Walkthrough](#source-file-walkthrough)
- [Config System](#config-system)
- [Process Execution](#process-execution)
- [Table Formatter](#table-formatter)
- [Argparse Library](#argparse-library)
- [Adding New Features](#adding-new-features)
- [Edge Cases and Gotchas](#edge-cases-and-gotchas)

---

## Execution Flow

Every `gitm` invocation follows this path:

```
main()
  ├── argparse_init()
  ├── global_options_init()         // --log-level, --log-file, --edit-entry
  ├── log_init(NULL, LOG_LEVEL_WARN) // early init so commands can log during parse
  ├── cmd_register_all()            // registers all 17 subcommands (each may call cmd_register_table_flag)
  ├── argparse_parse()              // matches command, calls callback
  │   └── cmd_callback()            // e.g. cmd_status()
  │       ├── config_default_path()
  │       ├── config_load()
  │       ├── git_exec() per repo
  │       ├── table output (if --table)
  │       └── config_save() if mutated
  ├── log_init(user_file, user_level) // re-init with user's --log-level / --log-file
  └── argparse_free()
```

### Step by step

1. **`main.c`**: Creates an `ArgParser` via `argparse_new()`. Registers `-L`, `-F`, `-E` on the root command.

2. **`log_init(NULL, LOG_LEVEL_WARN)`**: Early init at WARN level so that `LOG_DEBUG`/`LOG_TRACE` messages during command registration are suppressed by default, but `LOG_ERROR`/`LOG_WARN` messages are visible. This is called _before_ `cmd_register_all()` so that `LOG_TRACE` in `cmd.c` works correctly.

3. **`cmd_register_all()`** in `src/commands/cmd.c`: Calls each `cmd_register_*` function. These functions create subcommands with `argparse_add_command()` and add options with `argparse_add_option()`. Commands that support `--table` call `cmd_register_table_flag(cmd)`.

4. **`argparse_parse()`**: Tokenizes argv, matches the subcommand, calls its callback. Returns before `main()` continues if the command was `--help`, `--version`, `--shell-completion`, or `--edit-entry`.

5. **Command callback**: Each command follows the pattern:
   - Extract string options from `ArgParseResult` via `argparse_result_get()`
   - Resolve config path via `config_default_path()` or user override
   - Load config via `config_load()`
   - Perform work (iterate repos, run git, etc.)
   - If `g_table_mode` is true, use the table formatter for output
   - If the config was mutated, call `config_save()`
   - Free config with `config_free()`

6. **`log_init(user_file, user_level)`**: Re-initializes the logger with user-supplied `--log-level` and `--log-file`. Default level is WARN (not INFO). This ensures parsing-phase messages use defaults while the actual command respects user choices.

7. **Cleanup**: `argparse_free()` frees all parser memory. No global state leaks.

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

### `Table` / `TableRow` (in `include/table.h`)

```c
typedef struct {
    char **cells;      // array of cell strings (may contain ANSI codes)
    int    count;      // number of cells in this row
} TableRow;

typedef struct {
    TableRow *rows;
    size_t    row_count;
    size_t    row_capacity;

    const char **headers;
    int          col_count;

    bool show_header;
    bool use_color;
} Table;
```

- Created via `table_create(col_count, headers)`.
- Rows added via `table_add_row(table, ...)` (plain text) or `table_add_row_raw(table, cells, count)` (pre-formatted/ANSI).
- Width calculation skips ANSI escape sequences via `visible_width()`.
- Auto-detects TTY for color via `isatty(fileno(stderr))`.

---

## Source File Walkthrough

### Entry Point

#### `src/main.c`

- Declares the global options: `g_edit_entry`, `g_log_level_str`, `g_log_file`
- `parse_log_level()` accepts all 7 levels: `off`, `fatal`, `error`, `warn`, `info`, `debug`, `trace` — defaults to `LOG_LEVEL_WARN`
- `--edit-entry` handler opens `$EDITOR` (falling back to `vi`) with `fork()`/`execlp()` — uses raw fork, not `process_exec`, because editors require TTY access
- Calls `log_init(NULL, LOG_LEVEL_WARN)` before parse, `log_init(user_file, user_level)` after

### Command System

#### `src/commands/cmd.c`

- Contains `cmd_register_all(ArgParser *parser)` which calls all 17 `cmd_register_*` functions
- Defines `bool g_table_mode` (shared global, set by argparse storage pointer)
- Implements `cmd_register_table_flag(ArgCommand *cmd)` which registers `--table`/`-T` flag bound to `g_table_mode`
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
    if (g_table_mode) {
        // Table output path
        const char *headers[] = { "Name", "Status" };
        Table *t = table_create(2, headers);
        table_set_color(t, log_use_color());

        for (size_t i = 0; i < cfg.count; i++) {
            RepoEntry *e = &cfg.entries[i];
            table_add_row(t, e->name, "ok");
        }

        table_print(t, stdout);
        table_free(t);
    } else {
        // Plain output path
        for (size_t i = 0; i < cfg.count; i++) {
            RepoEntry *e = &cfg.entries[i];
            fprintf(stdout, "%s\n", e->name);
        }
    }

    // 4. Save if mutated
    // config_save(&cfg);

    // 5. Cleanup
    config_free(&cfg);
    return 0;
}
```

#### Commands That Invoke Git

Eight commands call `git_exec()` for each repo. All support `--tag`/`--group` filtering.

| Command    | File         | What it runs                               | `--table` |
| ---------- | ------------ | ------------------------------------------ | --------- |
| `status`   | `status.c`   | `git status --porcelain --branch`          | Yes       |
| `branch`   | `branch.c`   | `git branch`                               | Yes       |
| `last`     | `last.c`     | `git log -1 --format=%h\|%an\|%ar\|%s`     | Yes       |
| `recent`   | `recent.c`   | `git log -1 --format=%ct` (for sorting)    | Yes       |
| `remote`   | `remote.c`   | `git remote -v`                            | Yes       |
| `list-tag` | `list_tag.c` | `git tag -l -n1`                           | Yes       |
| `summary`  | `summary.c`  | `git branch` (count), `du -sh` (size)      | —         |
| `doctor`   | `doctor.c`   | `git rev-parse --git-dir` (validity check) | Yes       |

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

- `config_default_path()` resolves the config file path in this order:
  1. `$XDG_DATA_HOME/gitm/registered_repos.txt` — if `$XDG_DATA_HOME` is set and non-empty
  2. `~/Library/Application Support/gitm/registered_repos.txt` — on macOS (`__APPLE__`)
  3. `~/.local/share/gitm/registered_repos.txt` — on Linux (fallback)
- `config_ensure_dir()` creates the parent directory (and any missing parents) at the resolved path

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
- LOG_ERROR for pipe/fork failures includes `strerror(errno)`
- LOG_TRACE logs each `execvp` call

**Buffer limitation:** stdout and stderr are each limited to 1024 bytes. Commands producing more output will be truncated silently. This is a known limitation.

#### `src/git/git.c`

- `git_exec(cwd, args, result)` — convenience wrapper that calls `process_exec("git", args, result)`
- `git_is_repo(path)` — runs `git -C path rev-parse --git-dir`, checks exit code (LOG_TRACE)
- `git_current_branch(path)` — runs `git -C path rev-parse --abbrev-ref HEAD`, strips newline

### Table Formatter

#### `src/util/table.c` / `include/table.h`

A standalone utility for rendering aligned, pipe-separated tabular output. Used by 8 commands via `--table`/`-T` flag.

**API:**

| Function                                 | Purpose                                                 |
| ---------------------------------------- | ------------------------------------------------------- |
| `table_create(col_count, headers)`       | Allocate table with column headers (NULL for no header) |
| `table_add_row(table, ...)`              | Add a row of plain text cells (varargs, `const char *`) |
| `table_add_row_raw(table, cells, count)` | Add a row with pre-formatted cells (may contain ANSI)   |
| `table_set_header(table, show)`          | Enable/disable header row                               |
| `table_set_color(table, use_color)`      | Enable/disable ANSI color in output                     |
| `table_print(table, FILE *out)`          | Render table to a FILE stream                           |
| `table_free(table)`                      | Free all allocated memory                               |

**Width calculation:**

- `visible_width(s)` counts visible characters, skipping ANSI escape sequences (`\x1b[...m`)
- Column width = max of header width and all row widths in that column
- Columns are padded with spaces, separated by `|`

**Color handling:**

- `table_set_color()` defaults to `isatty(fileno(stderr))` — auto-detects TTY
- Headers are rendered in bold when color is enabled
- Separator line uses `-` and `+` characters

**Shared flag:**

- `bool g_table_mode` (defined in `cmd.c`, declared in `cmd.h`)
- Registered per-command via `cmd_register_table_flag(ArgCommand *cmd)` — adds `--table`/`-T` flag bound to `g_table_mode`
- Commands check `if (g_table_mode)` to choose between table and plain output

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
#include "table.h"

int cmd_newcmd(const ArgParseResult *result) {
    const char *name = argparse_result_get(result, "repo");

    GitConfig cfg;
    if (config_load(config_default_path(), &cfg) != 0) {
        LOG_ERROR("failed to load config");
        return -1;
    }

    if (g_table_mode) {
        const char *headers[] = { "Name", "Status" };
        Table *t = table_create(2, headers);
        table_set_color(t, log_use_color());
        // ... populate and print table
        table_print(t, stdout);
        table_free(t);
    } else {
        // ... plain output
    }

    config_free(&cfg);
    return 0;
}

void cmd_register_newcmd(ArgParser *parser) {
    ArgCommand *cmd = argparse_add_command(parser, "newcmd",
                                           "Description of the new command",
                                           cmd_newcmd);
    // Add --table support
    cmd_register_table_flag(cmd);
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
    ArgCommand *cmd = argparse_add_command(parser, "example",
                                           "Example command", cmd_example);
    argparse_add_option(cmd, "tag", 't', ARG_TYPE_STRING, "TAG",
                        "Filter by tag", &filter_tag);
    argparse_add_flag(cmd, "verbose", 'v', ARG_TYPE_NONE, NULL,
                      "Show detailed output", &verbose_flag);
}
```

In the callback, retrieve with:

```c
const char *tag = argparse_result_get(result, "tag");
const char *verbose = argparse_result_get(result, "verbose");
// verbose is "true" or "false" as a string
```

### Adding a New Global Option

In `main.c`, inside the global options block:

```c
static const char *g_my_option = NULL;
argparse_add_option(root, "my-option", 'm', ARG_TYPE_STRING, "VALUE",
                    "Description", &g_my_option);
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

2. **Config path on missing HOME**: `config_default_path()` calls `getenv("HOME")`. If `HOME` is unset and `$XDG_DATA_HOME` is not set, this returns `NULL` and `config_load()` will fail. When `$XDG_DATA_HOME` is set, `HOME` is not required for path resolution.

3. **Tag/group matching is substring, not token-based**: `config_entry_has_tag(e, "c")` matches both `"c"` and `"c,makefile"`, but also `"rust,c++"`. This is by design for simplicity.

4. **`--edit-entry` and `--shell-completion` short-circuit**: These cause `main()` to return before `argparse_parse()` is called. Global options like `--log-level` are ignored in this case.

5. **Raw fork for editors**: `open.c` and the `--edit-entry` handler use raw `fork()`/`execlp()` instead of `process_exec()` because editors need TTY access (stdin for user input, stdout for display).

6. **`config_save()` overwrites the file**: The entire config is rewritten on save. There is no partial write or journaling. If the process is killed mid-write, the config may be corrupted.

7. **`clean` command confirmation**: `clean.c` shows orphaned repos and waits for user confirmation before removing. `--dry-run` prints what would be removed without making changes.

8. **`recent` command sorting**: Uses `git log -1 --format=%ct` (Unix timestamp of last commit) to sort repos. Falls back to repos without commits being listed first.

9. **Argparse completion mode**: When `--shell-completion` is used, the parser enters a special mode that prints completion candidates and exits. This is not a regular command.

10. **`ARGPARSE_MAX_COMMANDS`**: Currently set to 32. If you add more than 32 subcommands total, increase this constant in `argparse/include/argparse.h`.

11. **Default log level is WARN**: `log_init()` defaults to `LOG_LEVEL_WARN`, not `INFO`. `LOG_INFO` and below are suppressed unless `--log-level info` (or lower) is passed. The `parse_log_level()` function also defaults to WARN when given an unrecognized string.

12. **`log_init()` called before `cmd_register_all()`**: The logger is initialized at WARN level _before_ command registration. This means `LOG_TRACE` in `cmd.c` (e.g., `"registering all commands"`) is visible by default. After parsing, `log_init()` is called again with the user's chosen level.

13. **Table output writes to stdout**: Unlike plain output which may use stderr for coloured status, table output goes to stdout. This allows piping `gitm list --table` to other commands.

14. **Table ANSI width**: `table.c` uses `visible_width()` to skip ANSI escape sequences when calculating column widths. This ensures columns align correctly even when cells contain colour codes.

15. **`str_util.h` not `string.h`**: The custom string utility header is named `include/str_util.h` to avoid shadowing the system `<string.h>`. Do not rename it back.

---

_For build system details and coding guidelines, see [DEV.md](DEV.md)._
