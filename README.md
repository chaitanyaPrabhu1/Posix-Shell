# Shell Assignment

## Overview
A custom interactive POSIX shell implemented in C++17. It supports the features required by AOS Assignment 2.

## Files

| File | Description |
|------|-------------|
| `main.cpp` | Entry point: initialises globals, sets up signal handlers, runs the REPL |
| `shell.h` | Shared declarations, includes, and extern globals |
| `globals.cpp` | Definition of global variables (home dir, cwd, fg pid, etc.) |
| `utils.cpp` | Tokeniser, command parser, prompt display, cwd helpers |
| `builtins.cpp` | Built-in commands: `ls` (-a/-l), `cd` (., .., -, ~), `pwd`, `echo` |
| `pinfo_search.cpp` | `pinfo` (reads `/proc`) and `search` (recursive file search) |
| `history.cpp` | History stored in `~/.myshell_history`: load, save, add, display |
| `signals.cpp` | Signal handlers: SIGCHLD (reap bg), SIGTSTP (Ctrl-Z), SIGINT (Ctrl-C) |
| `execute.cpp` | Command execution, I/O redirection, pipeline (any number of pipes), semicolon-separated commands |
| `input.cpp` | Raw terminal input: TAB autocomplete (commands + files), UP/DOWN arrow history, Ctrl-D exit |
| `makefile` | Build system |

## Building

```
make
```

## Running

```
./shell
```

## Features Implemented

1. **Prompt** — `<username@hostname:dir>` with `~` substitution for home
2. **cd / pwd / echo** — built-in, no `execvp`
3. **ls** — built-in with `-a`, `-l`, `-la/-al`, multi-path support, no `execvp`
4. **Background/Foreground** — `&` suffix; PID printed for background jobs
5. **pinfo** — reads `/proc/<pid>/stat` and `/proc/<pid>/exe`; shows status, virtual memory, exe path
6. **search** — recursive DFS under current directory
7. **I/O Redirection** — `<`, `>`, `>>` with correct permissions (0644)
8. **Pipelines** — unlimited pipe depth via `|`
9. **Redirection + Pipeline** — combined, e.g. `ls | grep txt > out.txt`
10. **Signals** — Ctrl-Z (SIGTSTP → bg), Ctrl-C (SIGINT → fg), Ctrl-D (exit shell)
11. **Autocomplete** — TAB completes commands (from PATH + builtins) and files/dirs in cwd
12. **History** — persisted across sessions in `~/.myshell_history`, max 20 stored, `history [n]`, UP/DOWN arrow navigation
