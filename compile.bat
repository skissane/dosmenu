@echo off
cls
nasm -fbin -o launcher.com -l launcher.lst launcher.asm
tcc -mt -lt -M dosmenu.c
