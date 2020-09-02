# lxsh

Extended shell linker for linking and generating shell code

# Features

## Command extensions

lxsh implements special linking commands that are resolved at linking.
These commands can be placed anywhere within the script like regular commands.

- `%include` : allows to insert file contents
- `%resolve` : allows to execute a shell command and insert its output

> See `lxsh --help-commands` for more details

## Other features

### Live execution

Execute an extended shell script directly with the `-e` option.

### Minimize code

Reduce code size to minimum without changing functionality with the `-m` option.

# Work in progress

lxsh should currently fully support POSIX syntax. <br>
A POSIX shell script should give a working output.

Some specific features are missing:
- link commands inside arithmetics (`$(())`) are not resolved
- link commands placed on the same line as keywords `if`, `then`, `elif`, `else`, `for`, `while`, `do` or `done` are not resolved
