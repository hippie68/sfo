# sfo

Prints SFO information of either a PS4 PKG or param.sfo file.                  
Providing a search string will output the value of that specific key only.     
Providing a replacement string will write it to the file (needs option -w). 

Usage:

    sfo [options] file [search] [replace]

Options:

    -h  Display help
    -w  Enable write mode
    -x  Print integer data in hexadecimal format

You can print all SFO info:

    sfo param.sfo
    sfo your-game.pkg

You can search for a specific key (case-insensitive), for example:

    sfo param.sfo PUBTOOLINFO
    sfo your-game.pkg content_id

You can also overwrite existing data, for example:

    sfo -w param.sfo app_type 2

To overwrite integer data, use regular decimal numbers (not 0x...).

Known issues:
- Overwriting PKG files directly may result in a broken PKG. Writing the original values will restore the PKG.
