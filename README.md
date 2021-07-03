# sfo

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
- Overwriting PKG files directly may result in a broken PKG. Writing the original values will restore the PKG.

# sfo.c

sfo.c can be compiled into a command line program, which is faster than the "sfo" Bash script (roughly by factor 30). It is still compatible with the "pkgrename" and "fw" scripts (https://github.com/hippie68/pkgrename, https://github.com/hippie68/fw), making their output faster. To compile, just enter

    make sfo

The command line options have changed a little:

    -d  Display integer values as decimal numerals
    -h  Display this help info
    -v  Increase verbosity
    -w  Enable write access

Windows binaries are available in the Release section: https://github.com/hippie68/sfo/releases.
