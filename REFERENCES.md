# References

The argparse library implementation is based on the following standards, patterns, and reference material.

## Standards

- **POSIX `--` end-of-options** — IEEE Std 1003.1, Utility Conventions
  https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap12.html

- **CLICOLOR / CLICOLOR_FORCE / FORCE_COLOR** — Network Working Group for terminal colour
  https://bixense.com/clicolors/

- **GNU argp** — glibc argument parser (design inspiration)
  https://www.gnu.org/software/libc/manual/html_node/Argp.html

- **GNU getopt** — POSIX option parsing
  https://www.gnu.org/software/libc/manual/html_node/Getopt.html

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

- **ANSI C0 control codes**
  https://en.wikipedia.org/wiki/C0_and_C1_control_codes

- **No colour when piped** — isatty() detection pattern
  https://man7.org/linux/man-pages/man3/isatty.3.html

## Similar Projects (design inspiration)

- **clap** (Rust) — colour scheme and help formatting
  https://docs.rs/clap/latest/clap/

- **argparse** (Python) — subcommand design pattern
  https://docs.python.org/3/library/argparse.html

- **subcommand** (Go) — nested command pattern
  https://pkg.go.dev/github.com/posener/cmd#section-readme

## C Reference

- **C17 standard (ISO/IEC 9899:2018)**
  https://en.cppreference.com/w/c

- **fork/exec pattern**
  https://man7.org/linux/man-pages/man2/fork.2.html
  https://man7.org/linux/man-pages/man3/exec.3.html

## SPDX License

- **SPDX License List**
  https://spdx.org/licenses/
