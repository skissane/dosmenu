# dosmenu
A very simple menuing system for MS-DOS compatible operating systems. I wrote this for my son to use in DOSBox.

It is written in C to compile with Borland Turbo C++ 3.0.

You can probably get it to work with other MS-DOS C compilers without too much trouble.

It also contains an assembly language component compiled with NASM (LAUNCHER.COM). This attempts to reduce memory consumption to maximise the amount of conventional memory available to programs run from the menu. Without LAUNCHER.COM, DOSMENU.COM will consume 66KB of conventional memory, whereas using LAUNCHER.COM the consumption is reduced to only 4KB. Note that LAUNCHER.COM is optional, and DOSMENU.COM will still work without it being present, just at the cost of more memory consumption.

![Screenshot](./screenshot.png?raw=true "Screenshot")
