## 3t is 3d in the terminal
3t is a software renderer that can display OBJs in the terminal using only text. I made 3t to explore the math powering 3d graphics and the basics of software rendering. It was inspired by [javidx9's YouTube](https://www.youtube.com/watch?v=ih20l3pJoeU) series on building a similar software renderer.

Currently it builds on Linux, specifically Fedora 42, and macOS Monterey 12.7.6. It would be nice to get it working on the BSDs as well.

### Technical Features
- ~Uses [Unicode Block Elements](https://en.wikipedia.org/wiki/Block_Elements) characters to increase the effective resolution by two times in each dimension. An 80 x 24 terminal can be addressed as 160 x 48 pixels.~

### Quirks
- 3t will run in the Linux console (ie, the kernel console that must used if X or Wayland is not installed). However, as the console only supports a limited number of colors by default, 3t will restrict itself to drawing wireframes. ~~However some character selections may seem strange. This is because it is able to understand, but not use, the block element characters that 3t emits because these charecters in higher unicode pages. The kernel maps these higher characters onto the sypmbols it can display.~~
- On the built in macOS terminal, block element characters do not fill the entire line vertically, at least in my testing with the base fixed-width fonts, leading to unsightly narrow gaps between lines. This can be corrected by forcing a shorter line height in the Inspector -> Profile -> Font menu.
- Some consoles might display strange colors when 3t is attempting to output grayscale. This is because they have TERM and COLORTERM set incorrectly, indicating that support color redefinition when they actually do not. Konsole on Fedora is one such culprit. This can be corrected by setting TERM & COLORTERM.

## Screenshot
![Cube](screenshot1.png?raw=true)
## How to Build
### Linux
On Linux, ensure that ncurses is installed using your distribution's package manager.
- **Fedora/RHEL/etc**: `sudo dnf install ncurses`
- **Debian/Ubuntu/etc**: `sudo apt install ncurses`

Then build 3t by running `make` in the 3t directory.

### BSD
*to be written*

### MacOS
The macOS Xcode dev tools ship with a version on ncurses which does not include wide character support. You will need to install a more complete version of the library using either MacPorts or Homebrew:
- **MacPorts**: `sudo macports install ncurses`
- **Homebrew**: `sudo brew install ncurses`

Open 3t/makefile and uncomment the appropriate lines.

Then build 3t by running `make` in the 3t directory.
