# HexExplorer

Terminal-based binary file explorer with lua interface.

## Purpose

This tool primarily designed to explore .ttf and .otf files, and so features commands for navigating to parts of a file via in-file offset values.

## Usage

The program takes a single argument, which must be valid file, and opens it for reading.

Once opened, it presents the given argument along the top of the terminal,
an open space where the history and position stack will be listed, 
and a lua command bar and report box along the bottom.

The command bar accepts any valid lua.

### Commands

Navigation:
- `quit()` - exits the program.
- `seek(n)` - seeks to the given position of the file.
- `advance(n)` - advances the file position by `n` bytes, negative values accepted.

Stack:
- `push(*name)` - adds the current file position to the stack with the given name, defaults to `""`.
- `back()` - returns the file position to the last stack entry.
- `pop(*n)` - removes the last `n` stack entries and sets the last popped entry as the current position, defaults to `1`.

History:
- `note(str)` - adds the given string to the history.
- `hex(n)` - reads `n` bytes and displays them as a hex number in the history.
- `char(n)` - reads `n` bytes and displays them as characters in the history.
- `clear()` - clears the history display.
- `delete(*n)` - removes the last `n` entries from the history, defaults to `1`.

Scripting:
- `tell()` - returns the current file position.
- `int(n)` - reads `n` bytes and returns them as a int.


## Credits

Program written by Alexander J. Johnson.

Uses lua, http://www.lua.org/.

Colour scheme roughly based on https://github.com/morhetz/gruvbox.
