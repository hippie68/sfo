# sfo
Reads SFO information from a PS4 PKG or param.sfo file.
Overwrites SFO fields in a PS4 PKG or param.sfo file, one at a.

You can print all SFO info:

    sfo param.sfo
    sfo your-game.pkg

You can also search for a specific key, for example:

    sfo param.sfo PUBTOOLINFO
    sfo your-game.pkg content_ide

You can also overwrite existing data, for example:

    sfo param.sfo app_type 2
    sfo your-game.pkg title "New Title that looks better"
    
Be careful, as providing 2 arguments enables overwriting.
