# lxsh

Extended shell linker for linking, processing and minifying shell code

# Installing

## linux-amd64

### zpkg

Available from the `zpkg` repository:
```shell
wget -qO- https://zpkg.zawz.net/install.sh | sh
zpkg install lxsh
```

### Binary

Download the `lxsh.tar.gz` archive, extract it,
and move the `lxsh` binary in a PATH folder (`/usr/local/bin` is the recommended).

```shell
wget https://github.com/zawwz/lxsh/releases/download/v1.1.0/lxsh.tar.gz
tar -xvf lxsh.tar.gz
sudo mv lxsh /usr/local/bin
```

## Other

See [Build](#build).

# Features

## Linking

lxsh implements special linking commands that are resolved at build time.
These commands can be placed anywhere within the script like regular commands.

- `%include` : allows to insert file contents
- `%resolve` : allows to execute a shell command and insert its output

> See `lxsh --help-commands` for more details

## Minify code

Reduce code size to a minimum without changing functionality with the `-m` option.

### Further minifying

The script can be further minified by altering code elements.
This can cause some change in execution behavior if you are not careful.

Variable names can be minified with `--minify-var`,
use `--exclude-var` to exclude variables from being minified (for example environment config).

Function names can be minified with `--minify-fct`,
use `--exclude-fct` to exclude functions from being minified.

Unnecessary quotes can be removed with `--minify-quotes`.

Unused functions and variables can be removed with `--remove-unused`.

Use `-M` to enable all of these minifying features (you still have to specify `--exclude` options when needed)

## Debashify

Some bash specific features can be translated into POSIX shell code.

The following bash features can be debashified:
- `<()` and `>()` process substitutions
- `<<<` herestring
- `>&`, `&>` and `&>>` output redirects
- `[[ ]]` conditions
- indexed arrays and associative arrays (+ their `declare` and `typeset` definitions)
- `$RANDOM`

### Advantages

- Removes dependency on bash and makes a script more portable.
- In some cases it can also provide improved performance given that some more minimalist shells like `dash` have better performance.
  * this doesn't always apply for all situations, make sure to verify through testing

### Limitations

#### $RANDOM

Debashifying of $RANDOM assumes /dev/urandom exists and provides proper randomness. <br>
The debashified $RANDOM generates numbers in range 0:65535 instead of 0:32767.

Debashified calls of $RANDOM have major performance loss

#### Process substitution

The debashifying of process substitution assumes that /dev/urandom will exist and will provide proper randomness. <br>
Temporary files with random names are used to create named pipes for the commands in the substitution.

There may be some slight performance loss on the creation of said process subtitution.

#### Indexed/Associative Arrays

Indexed arrays and associative arrays are detected on parse instead of runtime.
By default if an array operator is found, it is assumed to be an indexed array,
and associative arrays are detected through the use of `declare` (or `typeset`). <br>
In cases where there is ambiguity, the result upon execution might be undesired.

> To avoid such ambiguities, put the `declare` statement of a variable first of all,
> and don't mix and match different types on the same variable name

Getting the value of an array without index will give the full value instead of the first value.

> To avoid such situation, always get values from an index in your array

Arrays are stored as strings. Indexed arrays are delimited by tabs and associative arrays by newlines,
This means inserting values containing these characters will have undesired behavior.

Debashified arrays have substantially reduced performance.

Where bash would present proper errors upon incorrectly accessing arrays,
these features will continue working with undesired behavior.

> To avoid this, make sure to never access incorrect values

Array argument with `[@]` does not expand into the desired multiple arguments.

## String processors

You can use prefixes in singlequote strings to apply processing to the string contents. <br>
To use string processors, prefix the string content with a line in the form of `#<PROCESSOR>`.
Example:
```shell
sh -c '#LXSH_PARSE_MINIFY
printf "%s\n" "Hello world!"'
```

As of now only the processor `LXSH_PARSE_MINIFY` is implemented, but more may come later

## Other features

### Output generated code

By default lxsh outputs generated shell code to stdout.
You can use the `-o` option to output to a file and make this file directly executable.

> The resulting script is not dependent on lxsh

### Live execution

Directly execute an extended lxsh script with either
- `-e` option
- shebang is lxsh

> Direct execution introduces direct dependency on lxsh and code parsing overhead,
> therefore it should be avoided in production environments.

> stdin is known to not work properly on direct execution as of now

### Variable/Function/command listing

You can list all calls of variables, functions or commands with `--list-*` options

# Build <a name="build"></a>

## Dependencies

Depends on [ztd](https://github.com/zawwz/ztd)

## Building

Use `make -j` to build.<br>
You can use environment variables to alter some aspects:
- DEBUG: when set to `true` will generate a debug binary with profiling
- RELEASE: when set to `true`, the version string will be generated for release format
- STATIC: when set to `true` will statically link libraries

# Work in progress

The full POSIX syntax is supported and should produce a functioning result. <br>
Most bash syntax is also supported, but not all.

## Known bash issues

- Extended globs (`*()`) are not supported
- `(())` is parsed as subshells
- Unsetting functions can have undesired effects
