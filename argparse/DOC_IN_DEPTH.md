# argparse — In-Depth Integration Guide

A complete guide to using this library in your own project.

---

## Table of Contents

1. [Setup](#setup)
2. [Initialization](#initialization)
3. [Adding Commands](#adding-commands)
4. [Options](#options)
5. [Positional Arguments](#positional-arguments)
6. [Global Options](#global-options)
7. [Nested Subcommands](#nested-subcommands)
8. [Command Aliases](#command-aliases)
9. [Option Features](#option-features)
10. [Built-in Options](#built-in-options)
11. [Parse Result](#parse-result)
12. [Help Output](#help-output)
13. [Shell Completion](#shell-completion)
14. [Full Example](#full-example)

---

## Setup

Copy the `argparse/` directory into your project:

```
your_project/
  argparse/
    include/
      argparse.h        ← the only header you include
    src/
      argparse.c
      lexer.c
      lexer.h           ← internal, don't include
      help.c
      error.c
      error.h           ← internal, don't include
```

### Build integration

**Makefile:**

```makefile
SRC += $(wildcard argparse/src/*.c)
CFLAGS += -Iargparse/include
```

**CMake:**

```cmake
add_library(argparse STATIC
    argparse/src/argparse.c
    argparse/src/lexer.c
    argparse/src/help.c
    argparse/src/error.c
)
target_include_directories(argparse PUBLIC argparse/include)
```

**Direct compilation:**

```sh
cc -std=c17 -Iargparse/include \
    main.c argparse/src/*.c -o myapp
```

### Requirements

- C17 or later (`-std=c17`)
- Standard C library only (`stdio.h`, `stdlib.h`, `string.h`, `stdbool.h`)
- `<unistd.h>` for colour detection (auto-detected TTY; safe to remove if not available)

---

## Initialization

The library is initialized through a config struct. All metadata about your program is set in one place:

```c
#include "argparse.h"

ArgParserConfig config = {
    .prog_name   = "myapp",              // required
    .version     = "1.0.0",              // required
    .description = "My awesome tool",    // shown in --help
    .bug_url     = "https://github.com/me/myapp/issues",
    .author      = "Me <me@example.com>",
};

ArgParser *parser = argparse_new(&config);
```

`prog_name` and `version` are required. Everything else is optional — pass `NULL` to skip.

When you're done, free the parser:

```c
argparse_free(parser);
```

### Alternative: setter functions

If you can't set everything at init time, use individual setters:

```c
ArgParserConfig config = { .prog_name = "myapp", .version = "1.0.0" };
ArgParser *parser = argparse_new(&config);

argparse_set_description(parser, "My tool");
argparse_set_bug_url(parser, "https://...");
argparse_set_author(parser, "Me");
```

---

## Adding Commands

Every CLI command needs a callback function. The callback receives an `ArgParseResult` pointer and returns an `int` (0 for success):

```c
static int cmd_list(const ArgParseResult *result)
{
    printf("Listing items...\n");
    return 0;
}
```

Register it:

```c
argparse_add_command(parser, "list", "List all items", cmd_list);
```

The three arguments are: command name, description (shown in `--help`), and callback. Pass `NULL` as callback for parent commands that only group subcommands.

You can add as many commands as you need (up to `ARGPARSE_MAX_COMMANDS`, default 32):

```c
argparse_add_command(parser, "list",   "List items",   cmd_list);
argparse_add_command(parser, "add",    "Add an item",  cmd_add);
argparse_add_command(parser, "remove", "Remove item",  cmd_remove);
```

After adding commands, parse and dispatch:

```c
int rc = argparse_parse(parser, argc, argv);
argparse_free(parser);
return rc;
```

The parser automatically matches the command name from `argv`, calls the right callback, and handles errors. You don't need any manual dispatch code.

---

## Options

Options are added to commands. Each option has:

| Field | Purpose |
|---|---|
| `long_name` | `"verbose"` → becomes `--verbose` |
| `short_name` | `'v'` → becomes `-v`. Use `'\0'` for no short form |
| `type` | What kind of value it takes (see below) |
| `metavar` | Placeholder in help text, e.g. `"FILE"` |
| `description` | Explains what the option does |
| `storage` | Pointer to a variable where the parsed value is stored |

### Option types

| Type | CLI form | Storage type | Behaviour |
|---|---|---|---|
| `ARG_TYPE_NONE` | `--verbose`, `-v` | `bool*` | Sets to `true` when present |
| `ARG_TYPE_STRING` | `--output=file`, `-o file` | `const char**` | Stores the string pointer |
| `ARG_TYPE_INT` | `--count=5`, `-c 5` | `int*` | Stores the integer |
| `ARG_TYPE_COUNT` | `-v -v -v` | `int*` | Increments by 1 each time |

### Adding options

```c
static bool   g_verbose = false;
static int    g_count   = 0;
static char  *g_output  = NULL;

static int cmd_build(const ArgParseResult *result)
{
    // g_verbose, g_count, g_output are already filled by the parser
    return 0;
}

ArgCommand *build = argparse_add_command(parser, "build", "Build project", cmd_build);

argparse_add_option(build, "verbose", 'v', ARG_TYPE_NONE,   NULL,  "Verbose output",    &g_verbose);
argparse_add_option(build, "count",   'c', ARG_TYPE_INT,    "N",   "Repeat count",      &g_count);
argparse_add_option(build, "output",  'o', ARG_TYPE_STRING, "FILE", "Output file",       &g_output);
```

### How values are parsed

The parser handles multiple CLI forms automatically:

```sh
myapp build --verbose              # boolean flag
myapp build --output=file.txt      # long option with = separator
myapp build --output file.txt      # long option with space separator
myapp build -o file.txt            # short option with space
myapp build -ofile.txt             # short option with attached value
myapp build -cv3                   # combined short flags + attached value
```

For combined short flags (`-cv3`), the parser knows `-c` takes a value, so it consumes `v3` as the value for `-c` (if `-c` is `ARG_TYPE_INT`, it reads `3`; if `ARG_TYPE_STRING`, it reads `"v3"`).

### Options that take no value

If `long_name` or `short_name` is `'\0'`, that form is not available:

```c
// Only --verbose, no -v
argparse_add_option(cmd, "verbose", '\0', ARG_TYPE_NONE, NULL, "Verbose", &g_verbose);

// Only -v, no --verbose
argparse_add_option(cmd, NULL, 'v', ARG_TYPE_NONE, NULL, "Verbose", &g_verbose);
```

---

## Positional Arguments

Positional arguments are the non-option arguments (like `git add file.txt` where `file.txt` is positional).

```c
argparse_add_positional(cmd, "SOURCE");
argparse_add_positional(cmd, "DEST");
```

These names are only used in help text — they don't affect parsing. Access the actual values in the callback:

```c
static int cmd_copy(const ArgParseResult *result)
{
    for (int i = 0; i < result->positional_count; i++)
        printf("arg[%d]: %s\n", i, result->positionals[i]);
    return 0;
}
```

The limit is `ARGPARSE_MAX_POSITIONAL` (default 8). Extra positional arguments beyond the limit are silently ignored.

---

## Global Options

Options added to `&parser->root` are available to all commands:

```c
static bool g_dry_run = false;
static int  g_verbose = 0;

// Global options on root
argparse_add_option(&parser->root, "dry-run",  'n', ARG_TYPE_NONE,  NULL, "Dry run", &g_dry_run);
argparse_add_option(&parser->root, "verbose",  'v', ARG_TYPE_COUNT, NULL, "Verbose", &g_verbose);
```

Now both `myapp --dry-run list` and `myapp list --dry-run` work. The parser scans past global options to find the command, and also falls back to root options when a command-specific option is not found.

This means you can mix global and command-specific options in any order:

```sh
myapp -v --dry-run list           # global before command
myapp list -v --dry-run           # global after command
myapp --dry-run list -v           # mixed
```

---

## Nested Subcommands

Commands can have subcommands, forming a tree:

```c
ArgCommand *remote = argparse_add_command(parser, "remote", "Manage remotes", NULL);

ArgCommand *remote_add = argparse_add_subcommand(remote, "add",    "Add a remote",    cmd_remote_add);
ArgCommand *remote_rm  = argparse_add_subcommand(remote, "remove", "Remove a remote", cmd_remote_rm);
ArgCommand *remote_ls  = argparse_add_subcommand(remote, "list",   "List remotes",    cmd_remote_ls);
```

Usage: `myapp remote add origin https://github.com/...`

Each level can have its own options:

```c
argparse_add_option(remote_add, "fetch", 'f', ARG_TYPE_NONE, NULL, "Fetch after add", &g_fetch);
```

The help output shows nested structure:

```
Commands:
  remote           Manage remotes
    add            Add a remote
    remove         Remove a remote
    list           List remotes
```

---

## Command Aliases

Aliases let users type shorter versions of command names:

```c
const char *status_aliases[] = { "st", "s" };
argparse_command_set_aliases(status_cmd, status_aliases, 2);
```

Now all of these work:

```sh
myapp status
myapp st
myapp s
```

You can set up to `ARGPARSE_MAX_ALIASES` (4) aliases per command.

Aliases are shown in the help output with **bold cyan** colour, making them visually distinct from the dim description text:

```
  status          (st, s) Show status of all registered repos
```

---

## Option Features

### Environment variable fallback

If an option is not provided on the CLI, the parser checks an environment variable:

```c
argparse_add_option_with_env(cmd, "editor", 'e', ARG_TYPE_STRING, "EDITOR",
                             "Text editor", &g_editor, "EDITOR");
```

Priority: CLI value > environment variable > default value.

### Default values

Set `default_val` on the option. The parser stores it if nothing else is provided:

```c
ArgOption *opt = &cmd->options[cmd->option_count - 1];  // last added option
opt->default_val = "4";
```

Help text will show `[default: 4]`.

### Required options

```c
opt->required = true;
```

The parser errors out with a message if the option is not provided and has no env var fallback.

### Mutually exclusive options

Options in the same group cannot be used together:

```c
static bool g_json   = false;
static bool gPretty  = false;

argparse_add_option_exclusive(cmd, "json",   'j', ARG_TYPE_NONE, NULL, "JSON output",   &g_json,   1);
argparse_add_option_exclusive(cmd, "pretty", 'p', ARG_TYPE_NONE, NULL, "Pretty output", &gPretty,  1);
```

If the user types `myapp list -j -p`, the parser errors: "options in group 1 are mutually exclusive".

Group IDs are arbitrary positive integers. Options with different group IDs are independent.

### Checking if an option was provided

After parsing, each option's `was_set` field tells you if it was explicitly provided:

```c
// After argparse_parse returns, find the option and check:
for (int i = 0; i < cmd->option_count; i++) {
    if (cmd->options[i].was_set)
        printf("--%s was provided\n", cmd->options[i].long_name);
}
```

---

## Built-in Options

These options are available on every parser automatically — you don't need to register them:

| Option | Description |
|---|---|
| `-h`, `--help` | Prints help to stderr and returns |
| `-v`, `--version` | Prints version to stderr and returns |
| `--shell-completion=SHELL` | Outputs a shell completion script to stdout |

### `--shell-completion`

Generates completion scripts for bash, zsh, or fish. Output goes to stdout so users can pipe it to a file:

```sh
myapp --shell-completion bash > /etc/bash_completion.d/myapp
myapp --shell-completion zsh  > ~/.zsh/completions/_myapp
myapp --shell-completion fish > ~/.config/fish/completions/myapp.fish
```

The generated scripts include:
- All registered commands and their descriptions
- All global options
- Command-specific options (with correct subcommand detection)
- Alias support (aliases are included in completion)

Unsupported shells print an error: `unsupported shell: powershell (use bash, zsh, or fish)`

---

## Parse Result

The callback receives an `ArgParseResult` with everything you need:

```c
struct ArgParseResult {
    ArgParser      *parser;
    ArgCommand     *command;        // which command was invoked
    int             argc;
    char          **argv;

    const char     *positionals[8]; // positional arguments
    int             positional_count;

    const char     *rest[8];        // args after --
    int             rest_count;
};
```

The `rest` array contains arguments after `--`:

```sh
myapp list -- file1 file2
# result->rest[0] = "file1"
# result->rest[1] = "file2"
```

### Return values

`argparse_parse()` returns:

- `0` on success (help/version/completion may have been printed)
- `-1` on error (message already printed to stderr)

Your callback's return value is passed through as the program's exit code.

---

## Help Output

Help is fully automatic. Users get it by typing `--help` or `-h`:

```
Usage: myapp [OPTIONS] COMMAND [ARGS]

My awesome tool

Commands:
  list             (ls)    List items
  add                      Add an item
  build                    Build project

Options:
  -n, --dry-run           Dry run
  -v, --verbose           Verbose output
  -o, --output=FILE       Output file     [$OUTPUT]
  -j, --json              JSON output    [group: 1]
  -h, --help              Show this help message
  -V, --version           Show version
      --shell-completion=SHELL  Output shell completion script (bash, zsh, fish)
```

### Annotations shown automatically

- `[$ENV_VAR]` — option has environment variable fallback
- `(required)` — option must be provided
- `[default: val]` — option has a default value
- `[group: N]` — option belongs to a mutually exclusive group
- `(alias1, alias2)` — command aliases (shown in bold cyan)

### Colour

Colour is auto-detected based on whether stderr is a terminal. No config needed. Output to files/pipes is plain text.

### Custom help

Call `argparse_help()` directly in your callback to show help programmatically:

```c
argparse_help(result->parser, result->command);
```

---

## Shell Completion

### Built-in (recommended)

Use `--shell-completion` — no extra code needed:

```sh
myapp --shell-completion bash
myapp --shell-completion zsh
myapp --shell-completion fish
```

### Manual API

For custom completion logic, use the low-level API:

```c
argparse_complete(parser, argc, argv);
```

This prints one candidate per line to stdout. The current word being completed is `argv[argc - 1]`.

### Bash integration

Add to your shell rc file:

```bash
_complete_myapp() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    COMPREPLY=($(myapp --shell-completion bash | ...))
}
complete -F _complete_myapp myapp
```

Or use the generated script directly:

```bash
eval "$(myapp --shell-completion bash)"
```

---

## Full Example

A complete program with nested subcommands, global options, env var fallback, and aliases:

```c
#include <stdio.h>
#include "argparse.h"

static bool   g_dry_run = false;
static int    g_verbose = 0;
static const char *g_editor = NULL;

static int cmd_list(const ArgParseResult *result)
{
    if (g_dry_run) { printf("(dry run) "); }
    printf("Listing...\n");
    if (g_verbose > 1) printf("verbose level %d\n", g_verbose);
    return 0;
}

static int cmd_add(const ArgParseResult *result)
{
    if (result->positional_count < 1) {
        fprintf(stderr, "add: missing name\n");
        return 1;
    }
    printf("Adding: %s\n", result->positionals[0]);
    return 0;
}

static int cmd_remote_add(const ArgParseResult *result)
{
    if (result->positional_count < 2) {
        fprintf(stderr, "usage: myapp remote add NAME URL\n");
        return 1;
    }
    printf("Adding remote %s -> %s\n", result->positionals[0], result->positionals[1]);
    return 0;
}

static int cmd_remote_list(const ArgParseResult *result)
{
    printf("Listing remotes...\n");
    return 0;
}

int main(int argc, char *argv[])
{
    ArgParserConfig config = {
        .prog_name   = "myapp",
        .version     = "1.0.0",
        .description = "Example application",
    };

    ArgParser *parser = argparse_new(&config);
    if (!parser) return 1;

    /* Global options */
    argparse_add_option(&parser->root, "dry-run", 'n', ARG_TYPE_NONE,  NULL,
                        "Dry run", &g_dry_run);
    argparse_add_option(&parser->root, "verbose", 'v', ARG_TYPE_COUNT, NULL,
                        "Verbose output", &g_verbose);

    /* Top-level commands */
    ArgCommand *list_cmd = argparse_add_command(parser, "list", "List items", cmd_list);
    ArgCommand *add_cmd  = argparse_add_command(parser, "add",  "Add an item", cmd_add);
    argparse_add_positional(add_cmd, "NAME");

    /* Aliases */
    const char *list_aliases[] = { "ls" };
    argparse_command_set_aliases(list_cmd, list_aliases, 1);

    /* Nested subcommands */
    ArgCommand *remote = argparse_add_command(parser, "remote", "Manage remotes", NULL);
    argparse_add_subcommand(remote, "add",  "Add remote",  cmd_remote_add);
    argparse_add_subcommand(remote, "list", "List remotes", cmd_remote_list);

    /* Parse and run */
    int rc = argparse_parse(parser, argc, argv);
    argparse_free(parser);
    return rc;
}
```

Usage:

```sh
myapp --help
myapp list
myapp ls
myapp -n add myitem
myapp remote add origin https://github.com/...
myapp -vvv list
myapp --shell-completion bash
```

---

## Limits

These can be changed by editing the `#define`s at the top of `argparse.h`:

| Constant | Default | Description |
|---|---|---|
| `ARGPARSE_MAX_OPTIONS` | 32 | Options per command |
| `ARGPARSE_MAX_COMMANDS` | 32 | Top-level commands |
| `ARGPARSE_MAX_POSITIONAL` | 8 | Positional args per command |
| `ARGPARSE_MAX_ALIASES` | 4 | Aliases per command |
| `ARGPARSE_MAX_EXCLUSIVE_GROUPS` | 8 | Mutually exclusive groups |

---

## Error Handling

The parser prints coloured error messages to stderr with suggestions:

```
myapp: unknown option: --log-leve
Did you mean: log-level
Try 'myapp --help' for usage information.
```

```
myapp: option '-o' requires a value
Try 'myapp --help' for usage information.
```

```
myapp: required option '--output' is missing
Try 'myapp --help' for usage information.
```

```
myapp: unsupported shell: powershell (use bash, zsh, or fish)
```

You don't need to write any error handling code — the parser takes care of it.
