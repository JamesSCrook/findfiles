# findfiles

See https://yosj.com.au/staff/c_programs/findfiles/findfiles.html for details.

But, very briefly, findfiles is a \*nix command line utility that finds file system objects[*] in a UNIX-like environment and lists them in time (of last modification and/or access) order. It's been tested on on Linux, AIX and Cygwin.

## But find already does that, right?

For the things that findfiles does, find's command line syntax is rather complex and unwieldy. That's when findfiles is useful.

Among other things, findfiles is useful for folks running:

  * \*nix desktops: for finding objects[*] when you can't remember where you put them.
  * \*nix servers: for scripts whenever you need an easy way to determine the age of last modification and/or access.



[*] findfiles divides file system objects into 3 categories: (regular) files, directories and "other" (symlinks, block specials, named pipes, etc.)
