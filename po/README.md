# Translations

The texts of FreeSerf in other languages, as gettext `.po` files read by
the game with [tinygettext](https://github.com/tinygettext/tinygettext).
The language is chosen in the advanced options ("Language"), the original
English when none is chosen.

| File | Description |
|---|---|
| `freeserf.pot` | The texts to translate, extracted from the sources. |
| `ru.po` | Russian. |

## Translating

Start a new language from the template, for example German:

```
msginit --locale=de --input=po/freeserf.pot --output-file=po/de.po
```

and translate it with any `.po` editor (Poedit, Lokalize, a web
platform). The file has to be UTF-8; the font of the game has the Latin,
Cyrillic and the European accented letters.

The boxes of the game are small, as in the original: a popup is 128 pixels
wide (about 16 letters), its texts are split into lines with `\n` by hand.
A translation keeps the lines short and may move the line breaks; the
lines are centred in the box where the English ones are. `%d` is a number.

## Updating

After the texts in the sources change, the build target `update-po`
extracts them again into `freeserf.pot` and merges them into the
translations (needs `xgettext` and `msgmerge` of GNU gettext):

```
cmake --build build --target update-po
```
