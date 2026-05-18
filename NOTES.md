- I noticed around part 3 of the video series that David is using row vectors
  rather than column vectors. There's nothing wrong with this, but it did
  confuse me until I picked up on it, because all of his transforms were
  transposed compared to the literature and the textbooks I have at hand, and
  I had been assuming column vectors.
  
- Vector/world space and screen space have notions of 'up', which is to say,
  in the usual Cartesian system positive y is up, while in our console's
  screen space, positive y is down. That is, in a character grid addressed by
  rows and columns, higher numbered rows are further down on the screen, not
  up, and the origin is in the top left, not top right. This can be corrected
  by apply a translation-reflection-translation transform to each primative
  before rasterizing it.
  
- Dealing with 256+ colors on the terminal is a pain. Many modern terminal
  emulators support more than the basic 16 ANSI colors. 256 is common. Some
  even allow programs to redefine the emulator's internal color pallete, so
  that, for instance, one could redefine every color as a different, subtle
  shade of gray, and get 256-shade grayscale renderings. Sounds good. However,
  as ever, there are problems.
  
  1. Some terminal emulators, on some systems, will incorrectly report that
     they support color redefinition when they actually do not. Attempting to
     change the pallete results in silent failure and much confusion and
     frustation. This bug is because TERM and COLORTERM env variables are set
     incorrectly. Konsole on Fedora is where I discovered this. It sets
     TERM=xterm-256color & COLORTERM=truecolor, even though terminfo has valid
     konsole entries (see /usr/share/terminfo). Whether this bug is on Fedora,
     or KDE, or is deliberatly chosen incorrectness for the sake of backwards
     compatibility, I can't say.
	 
  2. So redefining the pallete is unreliable. Maybe we can figure out what
     pallete the terminal is using at start up and pick the shades we want to
     use. ncurses provides a color\_content() function that hints at this
     possibility. Except no, that doesn't work either. There are subtleties
     hinted at in the man page description: "color\_content permits extraction
     of the red, green, and blue components of an **initialized** color"
     (emph. added). A color is not considered initialized by ncurses until a
     call to init_color() is done to set set its color by the program. So
     there is no way to synchronize a terminal's pallete and ncurses, as far
     as I can tell. Note that when ncurses starts up, it populates an internal
     color pallete but it just uses default values and it doesn't query the
     terminal emulatior about *its* pallete to syncchronize the two. I'm not
     even sure that's possible. But it means that the values color\_content
     returns won't match the terminals.
	 
   The upshot of all this is that we can't reliable redefine the pallete, and
   we also can't figure out what the pallete is before using it. My solution
   is to use the [xterm color mappings](https://ss64.com/bash/syntax-colors.html) and YOLO it. Using color numbers in
   ncurses before initializng them means ncurses will just pass that color
   number directly to the terminal, and the terminal will use its own internal
   pallete to pick rgb values for it. Anyway, the xterm colors have become a
   de facto standard so it seems reliable enough, and there's not much else I
   can do about it anyway.
   
   Another possibility is to use direct color, aka truecolor, and issue exact
   rgb values to the terminal per pixel/character. Some terminal emulators
   support this and ncurses has had the capability since version 6.1
   (c.2018). But that will be an adventure for another day.
