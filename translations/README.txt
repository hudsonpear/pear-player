Pear Player - adding a language
===============================

A language is one UTF-8 JSON file in this folder. No tools to install, no
compiling: a text editor is enough. Anything you add shows up in
Settings > Interface > Language the next time the player starts.


1. Copy template.json to a new file named after the language, for example
   pt-BR.json, es.json or de.json. The file name is what the player stores as
   your choice, so keep it short and without spaces.

2. Set "language" to the name you want to see in the Settings list, written in
   that language:

       "language": "Portugues (Brasil)"

3. Fill in the values. The key is the English text exactly as the app shows it;
   the value is your translation:

       "Open...": "Abrir...",
       "&File": "&Arquivo"

4. Save the file here and restart Pear Player.


Notes
-----

* You do not have to translate everything. Any string you leave empty, or leave
  out of the file entirely, is shown in English. A file with ten lines in it
  works fine.

* Keep the special characters that appear in a key:

      &     marks the keyboard letter in a menu ("&File" -> Alt+F).
            Put it before whichever letter suits your language.
      %1    a value the app fills in ("Speed: %1x"). Keep every %1, %2 you
            find; you may move them around the sentence.
      \n    a line break inside a tooltip.

* JSON needs a comma between entries but not after the last one, and any " or
  \ inside a value must be written \" and \\. If the player ignores your file,
  that is almost always the reason -- paste it into any JSON validator to find
  the spot.

* template.json is regenerated from the source by
  tools/make-translation-template.ps1, so after an update it may contain new
  strings. Your own file is never touched; new strings simply appear in English
  until you add them.
