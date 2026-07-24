# argparse References

Links related to the argparse library design and implementation.

## Standards

- **GNU argp** — glibc argument parser (design inspiration)
  https://www.gnu.org/software/libc/manual/html_node/Argp.html

- **GNU getopt** — POSIX option parsing
  https://www.gnu.org/software/libc/manual/html_node/Getopt.html

- **POSIX `--` end-of-options** — IEEE Std 1003.1, Utility Conventions
  https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html

## Similar Projects (design inspiration)

- **clap** (Rust) — colour scheme and help formatting
  https://docs.rs/clap/latest/clap/

- **argparse** (Python) — subcommand design pattern
  https://docs.python.org/3/library/argparse.html

- **subcommand** (Go) — nested command pattern
  https://pkg.go.dev/github.com/posener/cmd#section-readme

## Shell Completion

- **Bash programmable completion**
  https://www.gnu.org/software/bash/manual/html_node/Programmable-Completion.html

- **Zsh completion system**
  https://zsh.sourceforge.io/Doc/Release/Completion-System.html

- **Fish complete builtin**
  https://fishshell.com/docs/current/cmds/complete.html

## Terminal & Colour

- **ANSI escape codes**
  https://en.wikipedia.org/wiki/ANSI_escape_code

- **CLICOLOR / CLICOLOR_FORCE / FORCE_COLOR** — Network Working Group for terminal colour
  https://bixense.com/clicolors/

- **isatty() detection pattern**
  https://man7.org/linux/man-pages/man3/isatty.3.html
