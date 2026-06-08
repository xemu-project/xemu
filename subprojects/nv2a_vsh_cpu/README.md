# nv2a_vsh_cpu

This project provides a CPU-based implementation of the nv2a vertex shader
operations used by the original Microsoft Xbox.


# Development

## git hooks

This project uses [git hooks](https://git-scm.com/book/en/v2/Customizing-Git-Git-Hooks)
to automate some aspects of keeping the code base healthy, in particular `clang-format`
invocation.

Please copy the files from the `githooks` subdirectory into `.git/hooks` to
enable them.
