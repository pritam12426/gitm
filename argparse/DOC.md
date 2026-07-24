# argparse

A lightweight, zero-dependency C17 argument parser with subcommand support.

No third-party libraries. No POSIX extensions. Just standard C.

## Features

- Nested subcommands (multi-level command trees)
- Global options inherited by subcommands
- Short and long options (`-v`, `--verbose`, `-j4`, `--output=file`)
- Mutually exclusive option groups
- Required option validation
- Default values wired to storage
- Environment variable fallback for any option
- Subcommand aliases (`st` for `status`) — shown in help with distinct colour
- Automatic `--help`, `--version`, and `--shell-completion`
- Built-in shell completion generation (bash, zsh, fish)
- Colour-coded help output
- POSIX-style `--` end-of-options handling
- "Did you mean?" suggestions for typos

## Quick Start

```c
#include "argparse.h"

static int cmd_list(const ArgParseResult *result)
{
    printf("Listing items...\n");
    return 0;
}

int main(int argc, char *argv[])
{
    ArgParserConfig config = {
        .prog_name   = "myapp",
        .version     = "1.0.0",
        .description = "My awesome tool",
    };

    ArgParser *parser = argparse_new(&config);

    ArgCommand *list = argparse_add_command(parser, "list", "List items", cmd_list);
    argparse_add_option(list, "verbose", 'v', ARG_TYPE_NONE, NULL, "Verbose output", NULL);

    int rc = argparse_parse(parser, argc, argv);
    argparse_free(parser);
    return rc;
}
```

## Usage Examples

### Basic command with options

```c
static bool g_verbose = false;
static const char *g_output = NULL;

static int cmd_build(const ArgParseResult *result)
{
    if (g_verbose)
        printf("Building...\n");
    if (g_output)
        printf("Output: %s\n", g_output);
    return 0;
}

// Register:
ArgCommand *build = argparse_add_command(parser, "build", "Build project", cmd_build);
argparse_add_option(build, "verbose", 'v', ARG_TYPE_NONE, NULL, "Verbose output", &g_verbose);
argparse_add_option(build, "output", 'o', ARG_TYPE_STRING, "FILE", "Output file", &g_output);
```

### Global options (available to all commands)

```c
static bool g_dry_run = false;

// Add to root command:
argparse_add_option(&parser->root, "dry-run", 'n', ARG_TYPE_NONE, NULL,
                    "Dry run", &g_dry_run);
```

### Positional arguments

```c
argparse_add_positional(cmd, "SOURCE");
argparse_add_positional(cmd, "DEST");

// Access in callback:
for (int i = 0; i < result->positional_count; i++)
    printf("arg[%d]: %s\n", i, result->positionals[i]);
```

### Environment variable fallback

```c
// Uses $EDITOR if --editor not provided on CLI
argparse_add_option_with_env(cmd, "editor", 'e', ARG_TYPE_STRING, "EDITOR",
                             "Text editor", &g_editor, "EDITOR");
```

### Mutually exclusive options

```c
static bool g_json = false;
static bool g_table = false;

argparse_add_option_exclusive(cmd, "json", 'j', ARG_TYPE_NONE, NULL,
                              "JSON output", &g_json, 1);
argparse_add_option_exclusive(cmd, "table", 't', ARG_TYPE_NONE, NULL,
                              "Table output", &g_table, 1);
// Parser errors if both -j and -t are given
```

### Required options

```c
ArgOption *opt = /* ... */;
opt->required = true;
// Parser errors if option is not provided
```

### Nested subcommands

```c
ArgCommand *remote = argparse_add_command(parser, "remote", "Manage remotes", NULL);
ArgCommand *remote_add = argparse_add_subcommand(remote, "add", "Add a remote", cmd_remote_add);
ArgCommand *remote_rm  = argparse_add_subcommand(remote, "remove", "Remove a remote", cmd_remote_rm);

// Usage: myapp remote add origin https://...
```

### Command aliases

```c
const char *aliases[] = { "st", "s" };
argparse_command_set_aliases(status_cmd, aliases, 2);
// "myapp st" and "myapp s" now work like "myapp status"
// Aliases are shown in help with distinct bold cyan colour
```

### Shell completion (built-in)

Users can generate completion scripts without any extra code:

```sh
myapp --shell-completion bash > /etc/bash_completion.d/myapp
myapp --shell-completion zsh  > ~/.zsh/completions/_myapp
myapp --shell-completion fish > ~/.config/fish/completions/myapp.fish
```

Supported shells: `bash`, `zsh`, `fish`.

### Check if option was provided

```c
static int cmd_example(const ArgParseResult *result)
{
    // After parsing, check the option's was_set field
    // (You need to keep a pointer to the option)
    return 0;
}
```

## Built-in Options

These are available on every parser automatically — no code needed:

| Option | Description |
|---|---|
| `-h`, `--help` | Show help message |
| `-v`, `--version` | Show version |
| `--shell-completion=SHELL` | Output completion script (bash, zsh, fish) |

## Return Values

| `argparse_parse()` | Meaning |
|---|---|
| `0` | Success (help/version/completion may have been printed) |
| `-1` | Error (message already printed to stderr) |
| Callback return | Your callback's return value is passed through |

## Help Output

Automatic. Users get coloured output with:

```
Usage: myapp [OPTIONS] COMMAND [ARGS]

My awesome tool

Commands:
  build           Build the project
  test            Run tests

Options:
  -n, --dry-run           Dry run
  -v, --verbose           Verbose output
  -o, --output=FILE       Output file     [$OUTPUT]
  -h, --help              Show this help message
  -v, --version           Show version
      --shell-completion=SHELL  Output shell completion script (bash, zsh, fish)
```

Command aliases show in bold cyan:

```
  status          (st, s) Show status of all registered repos
```

Options can display annotations:

- `[$ENV_VAR]` — env var fallback
- `(required)` — mandatory option
- `[default: val]` — default value
- `[group: N]` — mutually exclusive group

## License

Public domain. Use however you like.
