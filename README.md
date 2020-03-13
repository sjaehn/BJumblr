# B.Jumblr
Repository: BJumblr

Description: B.Jumblr is a pattern-controlled audio stream / sample re-sequencer LV2 plugin.

![screenshot](https://raw.githubusercontent.com/sjaehn/BJumblr/master/doc/screenshot.png "Screenshot from B.Jumblr")


## Installation

a) Install the bjumblr package for your system
* [Arch user repository](https://aur.archlinux.org/packages/bjumblr.lv2-git) by Milk Brewster
* Check https://repology.org/project/bjumblr/versions for other systems

b) Build your own binaries in the following three steps.

Step 1: Clone or download this repository.

Step 2: Install pkg-config and the development packages for sndfile, x11, cairo, and lv2 if not
done yet. On Debian-based systems you may run:
```
sudo apt-get install pkg-config libsndfile-dev libx11-dev libcairo2-dev lv2-dev
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

## Running

After the installation Carla, Ardour and any other LV2 host should automatically detect B.Jumblr.

If jalv is installed, you can also call it
```
jalv.gtk https://www.jahnichen.de/plugins/lv2/BJumblr
```
to run it stand-alone and connect it to the JACK system.

**Jack transport is required to get information about beat and bar position (not required for seconds mode)**


## Usage

B.Jumblr is neither a sample slicer nor a step sequencer. From the technical POV B.Jumblr is a
sequencer pattern-controlled audio delay effect.

The pattern defines at when (vertical) and which (horizontal) piece of the audio input stream is
sent to the audio output. Use the default diagonal line pattern for live playback. Moving a pad
one step to the right results in a one step delayed playback of the respective piece of the audio
input stream. Each pad (and thus each piece) can be levelled up or down by mouse wheel scrolling.

In addidion, the user can control the playback progession using the playback buttons or the speed
dial. This is facilitated via the progression delay.

### Source
Select between (live) audio stream or a sample file as source.

### Pattern matrix

* **Left click**: Set (or delete) pad level
* **Right click**: Pick pad level
* **Scroll**: Increase or decrease pad value
* **Shift scroll**: Resize waveform

### Step sync

B.Jumblr time line automatically synchronizes with the host time/position. Use the button - and +
to manually shift the time line down or up, respectively. The button < sets the time
line to the start of the pattern. The home button can be used to re-synchronize with the host.

### Step edit mode

There are two edit modes. The **ADD** mode allows to place additional pads to a step (row) or to remove
them. So you can also set more than one pad per column to produce echo effects or make a canon.

In contrast, there is only (exactly) one pad per step allowed in the **REPLACE** mode. Clicking or
dragging will result in a replacement of the original pad. Note: Deletion of pads (cut) in the
REPLACE mode results in their replacement by default pads.

### Step size

Defines the duration of each step. You can select between 1/16 and 4 seconds or beats or bars.
Alternatively, you can enter any value between 0.01 and 4.0 in a host provided generic GUI.
Note: Jack transport is required in the beats mode and in the bars mode.

### Pattern size

Defines the total pattern size (= number of steps, = number of columns). You can choose between
predefined pattern sizes from 2 to 32 steps. Alternatively, you can enter any value between 2 and 32
in a host provided generic GUI.

### Playback and speed

Controls the progression of the playback. The white buttons reset, increase or decrease the position
of the white progression line. The speed dial controls the speed of progression. These kinds of
manipulation of the playback progression are facilitated via the progression delay. The total amount
of the progression delay is displayed in the panel above the buttons.

## What's new

* Pattern orientation changed (progression from bottom to top)
* Control playback progression
* Control playback progression speed

## TODO

* Jump to the start / end of the respective step if playback delay buttons pressed
* Optional playback delay control via CV port
* Optimize runSequencer code


## Links
* Preview video: https://www.youtube.com/watch?v=n3LrpOD8MrI
