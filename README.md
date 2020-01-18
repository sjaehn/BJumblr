# B.???
Repository: BNoname

Description: A LV2 plugin. B.Noname resequences an audio input stream. This plugin is in an early stage of development. No guarantees.

Installation
------------
Build your own binaries in the following three steps.

Step 1: Clone or download this repository.

Step 2: Install pkg-config and the development packages for x11, cairo, and lv2 if not done yet. On
Debian-based systems you may run:
```
sudo apt-get install pkg-config libx11-dev libcairo2-dev lv2-dev
```

Step 3: Building and installing into the default lv2 directory (/usr/lib/lv2/) is easy. Simply call:
```
make
sudo make install PREFIX=/usr
```
from the directory where you downloaded the repository files. For installation into an
alternative directory (e.g., /usr/local/lib/lv2/), change the variable `PREFIX` while installing:

```
sudo make install PREFIX=/usr/local
```

Running
-------
After the installation Carla, Ardour and any other LV2 host should automatically detect B.Noname.

If jalv is installed, you can also call it
```
jalv.gtk https://www.jahnichen.de/plugins/lv2/BNoname
```
to run it stand-alone and connect it to the JACK system.

Usage
-----
From the technical POV B.Noname is a sequencer pattern-controlled audio delay effect.

The pattern defines at when (horizontal) and which (vertical) piece of the audio input stream is
sent to the audio output. Use the default diagonal line pattern for live playback. Moving a pad
one step to the right results in a one step delayed playback of the respective piece of the audio
input stream. You can also set more then one pad per row to produce echo effects or make a canon.
Each pad (and thus each piece) can be levelled up or down by mouse wheel scrolling.

TODO
----
* Find a name
* Make a Description
* GUI artwork
* Soft fading between slices
* Debugging
