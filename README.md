ncic
====

1. About NCIC

   NCIC is a chat client for Michael Kohn's Nakenchat (www.naken.cc).
   NCIC stands for either Naken Chat in Curses or Naken Chat in Color,
   you pick. :)

2. Who wrote NCIC?

   See AUTHORS

3. What is the license that NCIC is distributed under?

   The GPL. Also see COPYING

4. What systems does it work on?

   Probably any Unix or Unix derivative that supports nCurses.

   I have tested it on the following platforms.

   | OS              | Versions | Arch     |
   | --------------- | -------- | -------- |
   | OpenBSD         | 4.2-4.9  | PowerPC  |
   | OpenBSD         | 4.2-4.5  | i386     |
   | MacOS X Server  | 10.4.10  | i386     |
   | Slackware Linux | 12.2     | i386     |

Building
========
Under Ubuntu you will need to install ncurses-dev.
sudo apt-get install ncurses-dev

Other versions of Linux/Unix typically have ncurses installed by default.

After, type:

./configure
make
sudo make install
