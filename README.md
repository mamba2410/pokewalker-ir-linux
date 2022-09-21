# pw-ir-linux

Pokewalker IR interactions for Linux.

## About

Linux testing grounds for IR code for the `picowalker` since it's faster to develop natively.
It's not meant as a program for end-users, but feel free to play around with it.

Right now, I have a bare-bones send and recv system which dumps the internal rom to a file on disk.


## Usage

```
make run
```

Then point your Pokewalker at your IrDA serial device and select "Connect" from the main menu.

If all goes well, it should stop at address `0xbf80` and you should have a 48kiB file called "rom.bin".

## Dependencies

You need to have an IR serial port attached to your linux machine.
For testing, I have an _IrDA 3click_ board wired to an _ftdi2232h_, plugged in via USB.

## Credits

Dmitry GR for doing the initial rom dump of the Pokewalker and writing his [docs](http://dmitry.gr/?r=05.Projects&proj=28.%20pokewalker#_TOC_34cd8beec5c4813f4d151130632174e7).
This would not be possible without it.

I also ~~stole~~ used their shellcode for the rom dump and checksum verification code.


