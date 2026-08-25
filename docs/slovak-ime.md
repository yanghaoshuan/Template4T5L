# Slovak DGUS input method

The feature branch uses a T5L OS task to turn DGUS return-key events into
UTF-16BE Slovak text. The GUI project is configured for a 1024x600 display.

## DGUS interface

| VP | Words | Purpose |
| --- | ---: | --- |
| 0x0700 | 1 | Key event; cleared by the OS after consumption |
| 0x0710 | 1 | Destination VP and launch request |
| 0x0720 | 16 | Reserved for future composition data |
| 0x0730, 0x0740, 0x0750, 0x0760 | 16 each | Four candidates |
| 0x0770 | 16 | Caps/full/cursor status |
| 0x0780 | 64 | Editing buffer, 63 UTF-16 code units plus NUL |

Page 11 is the keyboard and page 12 is its pressed-state background. The demo
button on page 0 writes 0x2000 to VP 0x0710.

The editing preview inserts a visible `|` caret at the current cursor
position. The caret exists only in the preview VP and is never committed to
the destination text.

Existing ASCII keys preserve their packed upper/lower key words. Special keys
remain F0/F1/F2/F3/F4/F7/F8. Candidate keys use F101-F104. The 17 Slovak
extended keys use F200-F210 and are mapped by the OS to:

    á ä č ď é í ĺ ľ ň ó ô ŕ š ť ú ý ž

The private range avoids the collision between U+00F4 (ô) and the existing F4
Caps event.

## Suggestion dictionary

The first release contains 256 frequency-ranked words compiled into code
memory. Matching begins at two letters, is case-insensitive but
diacritic-sensitive, and returns at most four candidates. Selecting a
candidate replaces the word around the cursor and adds one trailing space only
when no delimiter already follows it.

The reproducible word list and source metadata are stored in
`slovak-ime-dictionary.tsv`.

## Integration

`SlovakImeInit()` clears the keyboard VP range. `SlovakImeTask()` runs every
20 ms as task ID 1. The feature is controlled by
`sysSLOVAK_IME_ENABLED`. This branch disables the R11 advertise/OTA modes
because the baseline R11 overlay uses the same 0x0700-0x0770 address range.
