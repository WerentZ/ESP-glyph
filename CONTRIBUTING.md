# Contributing

Thanks for your interest in Glyph Matrix! Issues, ideas and pull requests are
welcome.

## Getting started

1. Fork the repository and clone your fork.
2. Install [PlatformIO](https://platformio.org/) (CLI or the VS Code extension).
3. Build and flash:

   ```bash
   pio run
   pio run -t upload
   pio device monitor
   ```

4. For the website, edit `web/index.html`. `src/web_assets.h` is generated
   automatically by `scripts/embed_web.py` before every build — never edit it
   by hand.
5. For the phone editor, edit `editor.html` and test it in a browser
   (mobile viewport is useful).

## Guidelines

- Keep the existing code style: 4 spaces, `camelCase` functions, `m_` prefix for
  private fields, comments in English.
- Do not change pin assignments, Wi-Fi credentials or the `.anim` format without
  a good reason; if you do, update both READMEs and the API table.
- Device UI strings are in `src/ui.cpp` and `src/main.cpp`; the built-in font
  supports Latin and Cyrillic. Web strings live in the `DICT` object inside
  `web/index.html` and `editor.html` (English and Russian).
- Keep pull requests small and focused. Describe what you changed and why, and
  mention the board you tested on.

## Commit messages

Use short imperative subjects, for example:

```
Add round brush to the editor
Fix button layout at 270 degrees
```

## License

By contributing you agree that your changes are published under the MIT license
(see [LICENSE](LICENSE)).
