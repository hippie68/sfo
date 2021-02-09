# sfo

You can print all SFO info:

    sfo param.sfo
    sfo your-game.pkg

You can also search for a specific key, for example:

    sfo param.sfo PUBTOOLINFO
    sfo your-game.pkg content_id

You can also overwrite existing data, for example:

    sfo -w param.sfo app_type 2
    sfo -w your-game.pkg title "New Title that looks better"
