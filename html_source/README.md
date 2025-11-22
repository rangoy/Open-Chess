# HTML/JS Source Files

This directory contains the source HTML, CSS, and JavaScript files for the OpenChess web interface.

## Structure

- **`html_source/`** - Source HTML and CSS files
  - `common_styles.css` - Common CSS styles for all pages
  - `game_selection_styles.css` - Styles for the game selection page
  - `board_view_styles.css` - Styles for the board view page
  - `game_selection.html` - Game selection page HTML content
  - `game_selection.js` - JavaScript for game selection
  - `evaluation_bar.html` - Evaluation bar HTML structure

- **`js_source/`** - Source JavaScript files
  - `piece_symbols.js` - Unicode chess piece symbol mapping
  - `evaluation_bar.js` - Evaluation bar update logic
  - `board_update.js` - Board state update via AJAX

## Editing Files

**Edit the files in `html_source/` and `js_source/` directories directly.** These are real HTML/CSS/JS files that you can edit with any text editor or IDE.

## Generating Header Files

After editing the source files, run the build script to minify and generate the header files:

```bash
# Using yarn (recommended)
yarn install  # First time only
yarn build

# Or using npm
npm install    # First time only
npm run build

# Watch mode (auto-rebuild on file changes)
yarn watch
# or
npm run watch
```

The build script:
1. Reads the source files from `html_source/` and `js_source/`
2. **Minifies** HTML, CSS, and JavaScript to reduce memory usage
3. Escapes them for C++ string literals
4. Generates header files in `html/` and `js/` directories
5. The generated headers are automatically included at compile time

### Minification

The build process automatically minifies all files:
- **CSS**: Removes whitespace, optimizes selectors
- **JavaScript**: Compresses code, removes unnecessary whitespace
- **HTML**: Collapses whitespace, removes comments

This significantly reduces the memory footprint on the Arduino while maintaining functionality.

## Workflow

1. Install dependencies (first time only):
   ```bash
   yarn install
   # or
   npm install
   ```

2. Edit files in `html_source/` or `js_source/`

3. Build the header files:
   ```bash
   yarn build
   # or
   npm run build
   ```

4. Compile your Arduino project - the changes will be embedded in the firmware

### Development Mode

For active development, use watch mode to automatically rebuild on file changes:

```bash
yarn watch
# or
npm run watch
```

Then edit your HTML/JS files and the headers will be regenerated automatically.

## Notes

- The generated header files in `html/` and `js/` are auto-generated - **do not edit them directly**
- All changes should be made to the source files in `html_source/` and `js_source/`
- The script handles proper escaping of quotes, newlines, and special characters
- CSS files don't need `<style>` tags - they're added automatically by the template functions

