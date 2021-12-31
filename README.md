# sfo.c

sfo.c can be compiled into a command line program, which is faster than the old "sfo" Bash script (roughly by factor 30). It is still compatible with the "pkgrename" and "fw" scripts (https://github.com/hippie68/pkgrename, https://github.com/hippie68/fw), making their output faster.

    Usage: sfo [OPTIONS] FILE
    
    Reads a file to print or modify its SFO parameters.
    Supported file types:
      - PS4 param.sfo (print and modify)
      - PS4 disc param.sfo (print only)
      - PS4 PKG (print only)
    
    The modification options (-a/--add, -d/--delete, -e/--edit) can be used
    multiple times. They are automatically queued to run in appropriate order. If
    any modification fails, changes are discarded and no data is written.
    
    Options:
      -a, --add TYPE PARAMETER VALUE  Add a new parameter.
                                      TYPE must be either "int" or "str".
      -d, --delete PARAMETER          Delete specified parameter.
          --debug                     Print debug information.
          --decimal                   Display integer values as decimal numerals.
      -e, --edit PARAMETER VALUE      Change specified parameter's value.
      -f, --force                     Do not abort when modifications fail. Make
                                      option --add overwrite existing parameters.
                                      Make option --new-file overwrite existing
                                      files. Useful for automation.
      -h, --help                      Print usage information and quit.
          --new-file                  If FILE (see above) does not exist, create a
                                      new param.sfo file of the same name.
      -o, --output-file OUTPUT_FILE   Save the final data to a new file of type
                                      "param.sfo", overwriting existing files.
      -s, --search PARAMETER          If PARAMETER exists, print its value.
      -v, --verbose                   Increase verbosity.
          --version                   Print version information and quit.

### How to compile

    gcc sfo.c -O3 -s -o sfo

For Windows:

    x86_64-w64-mingw32-gcc-win32 sfo.c -O3 -s -o sfo.exe

Windows binaries are available in the Release section: https://github.com/hippie68/sfo/releases.

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



