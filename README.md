# B.???
Repository: BNoname

Description: A LV2 plugin. This plugin is in an early stage of development. No guarantees.

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
After the installation Carla, Ardour and any other LV2 host should automatically detect B.SEQuencer.

If jalv is installed, you can also call it
```
jalv.gtk https://www.jahnichen.de/plugins/lv2/BNoname
```
to run it stand-alone and connect it to the JACK system.

TODO
----
* Find a name
* Make a Description
* GUI artwork
* Debugging
