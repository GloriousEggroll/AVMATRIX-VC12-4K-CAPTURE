Preface:

I bought this card on Amazon for around $250 because it has the same capabilities as the Magewell Pro Capture HDMI 4k LT, and I wanted to see if it held up in comparison. The Magewell card is a $900 card, as opposed to this one. 

Once I got the card, I found the standard ./install -did not work- on Fedora, and the code was slightly outdated for kernel 6.1. On top of that the drivers were being hosted on OneDrive.. which is sketchy. 

To add to that, since I also own the Magewell card already, I was able to do a driver comparison, and I found that this company had done a copy/paste of the Magewell driver install bash scripts and just modified them with their own driver sources.  This made porting over the DKMS functionality (and required kernel module signing) much easier!

Therefore I updated their drivers for them, pointed out all the things wrong, and sent them an e-mail about it. In the meantime, here is the full driver source, which is has been updated and working on Fedora 37 as of Kernel 6.2. 

The license for this project is GPL, as both the AVMATRIX driver -AND- the Magewell driver use GPL:

AVMATRIX:
```
$ grep -rni . -e license
./src/hws_video.c:5794:MODULE_LICENSE("GPL");
```

Magewell:
```
$ grep -rni . -e license
./src/sources/avstream/capture.c:611:MODULE_LICENSE("GPL");
```
