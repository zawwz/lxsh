# lxsh

Extended shell linker for linking, generating and minimizing shell code

# Installing

## linux-amd64

Download the `lxsh-linux-amd64.tar.gz` archive and move `lxsh` binary in a PATH folder,
`/usr/local/bin` is the recommended.

## Other

See [Build](#build).

# Features

## Command extensions

lxsh implements special linking commands that are resolved at linking.
These commands can be placed anywhere within the script like regular commands.

- `%include` : allows to insert file contents
- `%resolve` : allows to execute a shell command and insert its output

> See `lxsh --help-commands` for more details

## Minimize code

Reduce code size to a minimum without changing functionality with the `-m` option.

### Further minimizing

The script can be further minimized by altering code elements.
This can cause some change in execution behavior if you are not careful.

Variable names can be minimized with `--minimize-var`,
use `--exclude-var` to exclude variables from being minimized (for example environment config).

Function names can be minimized with `--minimize-fct`,
use `--exclude-fct` to exclude functions from being minimized.

Unused functions can be removed with `--remove-unused`.
Removal of unused variables is a work in progress.

## Other features

### Output generated code

By default lxsh outputs generated shell code to stdout.
You can use the `-o` option to output to a file and make this file directly executable.

> The resulting script is not dependent on lxsh

### Live execution

Directly execute an extended lxsh script with either
- `-e` option
- shebang is lxsh

> Direct execution introduces direct dependency on lxsh and code generation overhead,
> therefore it should be avoided outside of development use

> There may be some issues with direct execution as of now

### Variable/Function/command listing

You can list all calls of variables, functions or commands with `--list-*` options

# Build <a name="build"></a>

## Dependencies

Depends on [ztd](https://github.com/zawwz/ztd)

## Building

Use `make -j8` to build.<br>
You can use environment variables to alter some aspects:
- DEBUG: when set to `true` will generate a debug binary with profiling
- RELEASE: when set to `true`, the version string will be generated for release format
- STATIC: when set to `true` will statically link libraries

# Work in progress

The full POSIX syntax is supported and should produce a functioning result. <br>
However not all bash syntax is supported yet.

## incomplete POSIX features

- `$(())` arithmetics are not minimized

## Known bash issues

- `&>` and `>&` are not supported
- `|&` is not supported
- `<<<` is not supported
- `<()` is not supported
- `${!VAR}` does not register as a variable
- Extended globs (`*()`) are not supported
- `(())` creates two subshells instead of one object

## Debashifying

Work to debashify scripts is planned
