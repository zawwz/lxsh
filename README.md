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

### Output generated code

Output the generated shell code to stdout with either:
- `-o` option
- shebang other than lxsh

> Redirect stdout to a file to create a script file. <br>
> The resulting script is not dependent on lxsh

### Live execution

Directly execute an extended shell script with either
- `-e` option
- shebang is lxsh

> Direct execution introduces direct dependency on lxsh and overhead,
> therefore it should be avoided outside of development use

### Minimize code

Reduce code size to minimum without changing functionality with the `-m` option.

# Work in progress

lxsh should currently fully support POSIX syntax. <br>
A POSIX shell script should give a working output.

Some specific features are missing:
- link commands in subshells inside arithmetics are not resolved
- arithmetics cannot be minimized
- link commands placed on the same line as keywords `if`, `then`, `elif`, `else`, `for`, `while`, `do` or `done` are not resolved
