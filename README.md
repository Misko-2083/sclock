# sclock
Simple clock
with options similar to yad

sclock.c requires libgtk3-dev

compiles with

```gcc sclock.c -o sclock $(pkg-config --cflags --libs gtk+-3.0)```

for options see help ```./sclock --help```


sclockx.c requires libcairo2-dev libx11-dev

compiles with

```gcc sclockx.c -o sclockx $(pkg-config --cflags --libs cairo-xlib-xrender x11)```
