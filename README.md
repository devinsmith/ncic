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
   | OpenBSD         | 4.2-5.9  | PowerPC  |
   | OpenBSD         | 4.2-4.5  | i386     |
   | FreeBSD         | 10.2     | amd64    |
   | MacOS X Server  | 10.4.10  | i386     |
   | Slackware Linux | 12.2     | i386     |
   | Ubuntu          | 16.04    | amd64    |
   | Debian          | Buster   | amd64    |

# Screenshot

![Main screen](img/ncic.png)


Building
========
Under Linux, ncurses and ssl development libraries are required. On a Debian
based distribution you can execute the following command:

```
sudo apt-get install ncurses-dev libssl-dev
```

Other versions of Linux/Unix typically have ncurses installed by default.


After, type:

```
mkdir build
cd build
cmake ..
make
sudo make install
```

You should then be able to simply run ncic by executing ncic. You can connect
to a server by typing

```
/connect yourname naken.cc
```

and pressing enter.
