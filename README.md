# sfo.c

sfo.c can be compiled into a command line program, which is faster than the old "sfo" Bash script (roughly by factor 30). It is still compatible with the "pkgrename" and "fw" scripts (https://github.com/hippie68/pkgrename, https://github.com/hippie68/fw), making their output faster. It can be used to query or modify param.sfo data or to build new param.sfo files from scratch.

    Usage: sfo [OPTIONS] FILE

    Reads a file to print or modify its SFO parameters.
    Supported file types:
      - PS4 param.sfo (print and modify)
      - PS4 disc param.sfo (print only)
      - PS4 PKG (print only)

    The modification options (-a/--add, -d/--delete, -e/--edit, -s/--set) can be
    used multiple times. Modifications are done in memory first, in the order in
    which they appear in the program's command line arguments.
    If any modification fails, all changes are discarded and no data is written:

      Modification  Fail condition
      --------------------------------------
      Add           Parameter already exists
      Delete        Parameter not found
      Edit          Parameter not found
      Set           None

    Options:
      -a, --add TYPE PARAMETER VALUE  Add a new parameter, not overwriting existing
                                      data. TYPE must be either "int" or "str".
      -d, --delete PARAMETER          Delete specified parameter.
          --debug                     Print debug information.
          --decimal                   Display integer values as decimal numerals.
      -e, --edit PARAMETER VALUE      Change specified parameter's value.
      -f, --force                     Do not abort when modifications fail. Make
                                      option --new-file overwrite existing files.
      -h, --help                      Print usage information and quit.
          --new-file                  If FILE (see above) does not exist, create a
                                      new param.sfo file of the same name.
      -o, --output-file OUTPUT_FILE   Save the final data to a new file of type
                                      "param.sfo", overwriting existing files.
      -q, --query PARAMETER           Print a parameter's value and quit.
                                      If the parameter exists, the exit code is 0.
      -s, --set TYPE PARAMETER VALUE  Set a parameter, whether it exists or not,
                                      overwriting existing data.
      -v, --verbose                   Increase verbosity.
          --version                   Print version information and quit.

### Examples

Viewing SFO parameters:

    sfo param.sfo
    sfo example.pkg --verbose --decimal

Modifying SFO parameters:

    sfo -e title "Super Mario Bros." -d title_00 -s int pubtool_ver 0x123 param.sfo

Modifying SFO parameters but saving to a different file:

    sfo -e title "Super Mario Bros." -d title_00 -s int pubtool_ver 0x123 param.sfo --output-file test.sfo

Extracting a param.sfo file from a PS4 PKG file:

    sfo example.pkg --output-file param.sfo

Creating a new param.sfo file from scratch:

    sfo --new-file -a str app_ver 01.00 -a str category gdk -a int attribute 12 param.sfo

Printing a single parameter:

    $ sfo -q title param.sfo
    Super Mario Bros.

Querying will return 0 if the parameter exists:

    $ sfo -q title param.sfo
    Super Mario Bros.
    $ echo $?
    0

    $ sfo -q asdf param.sfo
    $ echo $?
    1

Use querying to save parameters in your scripts/tools, for example (Bash):

    title=$(sfo -q title param.sfo)

Or parse them all (Bash):

    i=0
    while read -r line; do
      param[i]=${line%%=*}
      value[i]=${line#*=}
      ((i++))
    done < <(sfo param.sfo)

### How to compile

    gcc sfo.c -O3 -s -o sfo

For Windows:

    x86_64-w64-mingw32-gcc-win32 sfo.c -O3 -s -o sfo.exe

Windows binaries are available at https://github.com/hippie68/sfo/releases.

### Troubleshooting

Option --debug prints the exact param.sfo file layout, making it easy to spot errors quickly.

Please report bugs or request features at https://github.com/hippie68/sfo/issues.

# sfo (old Bash script)

Prints or modifies SFO parameters of a PS4 PKG or a param.sfo file.
Providing a search string will output the value of that specific key only.
Providing a replacement string will write it to the file (needs option -w).

Usage:

    sfo [options] file [search] [replace]

Options:

    -h  Display help
    -w  Enable write mode

You can print all SFO info:

    sfo param.sfo
    sfo your-game.pkg

You can search for a specific key (case-insensitive), for example:

    sfo param.sfo PUBTOOLINFO
    sfo your-game.pkg content_id

You can also overwrite existing data, for example:

    sfo -w param.sfo app_type 2

To overwrite integer data, you can use decimal or hexadecimal numbers.

Known issues:

- Overwriting PKG files directly ~~may~~ will result in a broken PKG. Writing the original values will restore the PKG.



