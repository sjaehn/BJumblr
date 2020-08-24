# B.Jumblr
Repository: BJumblr

Description: B.Jumblr is a pattern-controlled audio stream / sample re-sequencer LV2 plugin.

![screenshot](https://raw.githubusercontent.com/sjaehn/BJumblr/master/doc/screenshot.png "Screenshot from B.Jumblr")


## Installation

a) Install the bjumblr package for your system
* [Arch user repository](https://aur.archlinux.org/packages/bjumblr.lv2-git) by Milkii Brewster
* [FreeBSD](https://www.freshports.org/audio/bjumblr-lv2) by yurivict
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

**The host must provide information about beat and bar position to use B.Jumblr in these modes.**
**Pure audio tracks (Ardour) may lack these information. Try to add a MIDI input!**


## Usage

B.Jumblr is neither a sample slicer nor a step sequencer. From the technical POV B.Jumblr is a
sequencer pattern-controlled audio delay effect.

The pattern defines at when (vertical) and which (horizontal) piece of the audio input stream is
sent to the audio output. Use the default diagonal line (zero delay line) pattern for live
playback. Moving a pad one step to the right results in a one step delayed playback of the
respective piece of the audio input stream. Each pad (and thus each piece) can be leveled up or
down by mouse wheel scrolling.

In addition, the user can control the playback progression using the playback buttons or the speed
dial. This is facilitated via the progression delay.


### Source

Select between (live) audio stream or a sample file as source.


### Pattern page tabs

Define multiple patterns. Click on a tab to highlight the tab and to edit the respective pattern.
Press ▸ on the tab to switch playback to the respective pattern. Press the piano keys symbol to
enable / disable MIDI-controlled playback of the respective pattern.

| Symbol | Description |
| --- | --- |
| + | Add a pattern page after the respective page. |
| - | Remove this pattern page. |
| < | Move pattern page backward. |
| > | Move pattern page forward. |

### MIDI control page

This menu appears upon clicking on the piano keys symbol in the respective tab. Enable / disable
MIDI-controlled playback of the respective pattern by selection or deselection (= none) of a MIDI
status. You can manually set the parameters to which B.Jumblr shall respond or you can use MIDI
learning. Don't forget to confirm ("OK") or discard changes ("Cancel")!


### Pattern matrix

* **Left click**: Set (or delete) pad level
* **Right click**: Pick pad level
* **Scroll**: Increase or decrease pad value
* **Shift scroll**: Resize waveform

### Calibration

B.Jumblr time line automatically synchronizes with the host time/position. Use the button - and +
to manually shift the whole sample /stream. The button < sets the current sample to the start of
the pattern. The home button can be used to re-synchronize with the host.


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

Controls the progression of the pattern. These buttons do not shift the sample /stream (in contrast to
calibration). The white buttons modify the position (reset, increase, difference to start of the step,
or increase) of the pattern progression represented by the yellow markers left and right to the pattern.
The speed dial controls the speed of progression. These kinds of manipulation of the pattern progression
are facilitated via the progression delay. The total amount of the progression delay (= delay buttons +
speed-induced delay) is displayed in the panel above the buttons.


## What's new

* mp3 support


## Acknowledgments

* Milkii Brewster for ideas about principle and features
* unfa for ideas about multiple patterns and automation
* Rob van den Berg for the plugin name


## Links
* Tutorial video: https://www.youtube.com/watch?v=DFSi7TMqvMw
* Re-sampling by unfa: https://youtu.be/MmGLTOkNenc?t=4491
