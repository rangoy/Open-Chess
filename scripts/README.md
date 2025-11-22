# Build Scripts

## build-html-headers.js

Node.js build script that **minifies** HTML/CSS/JS and generates C++ header files for compile-time embedding.

### Features

- ✅ **Minification**: Reduces file sizes by 30-60% to save Arduino memory
- ✅ **Watch Mode**: Auto-rebuild on file changes during development
- ✅ **Modern Tooling**: Uses industry-standard minifiers (html-minifier-terser, csso, terser)
- ✅ **Error Handling**: Graceful fallback if minification fails

### Installation

```bash
yarn install
# or
npm install
```

### Usage

**Build once:**
```bash
yarn build
# or
npm run build
```

**Watch mode (auto-rebuild on changes):**
```bash
yarn watch
# or
npm run watch
```

### Minification Benefits

The build process automatically minifies all files:

- **CSS**: Removes whitespace, optimizes selectors, shortens color values
- **JavaScript**: Compresses code, removes unnecessary whitespace, optimizes variable names
- **HTML**: Collapses whitespace, removes comments

**Example size reduction:**
- Original CSS: ~800 bytes
- Minified CSS: ~500 bytes (37% reduction)

This significantly reduces the memory footprint on Arduino while maintaining full functionality.

### File Structure

```
html_source/          (edit these files)
  ├── common_styles.css
  ├── game_selection_styles.css
  ├── board_view_styles.css
  ├── game_selection.html
  ├── game_selection.js
  └── evaluation_bar.html

js_source/            (edit these files)
  ├── piece_symbols.js
  ├── evaluation_bar.js
  └── board_update.js

html/                 (auto-generated, don't edit)
  ├── common_styles.h
  ├── game_selection_styles.h
  ├── board_view_styles.h
  ├── game_selection.html.h
  ├── game_selection_script.h
  └── evaluation_bar.html.h

js/                   (auto-generated, don't edit)
  ├── piece_symbols.js.h
  ├── evaluation_bar.js.h
  └── board_update.js.h
```

### Integration with Arduino IDE

1. Edit files in `html_source/` or `js_source/`
2. Run `yarn build` or `npm run build`
3. Compile in Arduino IDE
4. The minified, embedded code will be included in your firmware

### Development Workflow

For active development:

```bash
# Terminal 1: Watch for changes
yarn watch

# Terminal 2: Edit your HTML/JS files
# Files will auto-rebuild when saved
```

### Dependencies

- `html-minifier-terser`: HTML minification
- `csso`: CSS minification and optimization
- `terser`: JavaScript minification
- `chokidar`: File watching for development mode

### Notes

- The generated header files are auto-generated - **do not edit them directly**
- All changes should be made to source files in `html_source/` and `js_source/`
- CSS files don't need `<style>` tags - they're added automatically by template functions
- If minification fails, the script falls back to unminified content with a warning
