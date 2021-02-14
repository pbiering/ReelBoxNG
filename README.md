# ReelBoxNG
ReelBox Next Generation Helper/Support Scripts/Tools/Code

Content of this repository will support creating ReelBoxNG based on self-made mainboard exchange

See also Wiki: https://github.com/pbiering/ReelBoxNG/wiki

## Features
- DONE: frontpanel display (setup, control, progress feature)
- DONE: frontpanel LEDs (setup, control, progress feature)
- DONE: ReelBox remote control unit via frontpanel
- DONE: netceiver (preparation)
- PARTIAL: frontpanel button (POWER-only so far)
- DONE: add support for eHD (see also https://github.com/pbiering/vdr-plugin-reelbox)

## History
Starting with easyVDR, switching to BM2LTS, back to easyVDR, now running on vanilla Fedora Linux distribution

## Notes
In case of taking use in easyVDR or BM2LTS or local, please feed back all required extensions in an unbreaking compatibility was via github Pull Request

## Issues
Please use github's issue tracker

## Coming next
- schematics for frontpanel connection (serial and towards mainboard)

## Content

### src-reelvdr

Contain source code for
- hdshm3
- bspshm
- reelfpctl
- reelbox-ctrld

By scripts expected target for binaries would be: /opt/reel/sbin (use e.g. softlink)

### bin-reelvdr

Contain binaries for
- eHD boot image (MIPS)
- hdplayer binary (MIPS)

### etc

Contains script / config files, can be either stored directly in related OS directories or use softlink to the cloned directory

### etc/vdr

- remote.conf.reelbox: remote control unit configuration file extension for ReelBox support
- keymacros.conf.reelbox: keymacro configuration file example for ReelBox Remote Control Unit

### etc/sysconfig

- etc/sysconfig/reel: config file related to ReelBox

- etc/sysconfig/vdr (not shown): suggested action options would be: --shutdown=/opt/reel/sbin/easyvdr-shutdownaction --record /opt/reel/sbin/easyvdr-recordingaction

### etc/sysconfig/vdr-plugins.d/

- etc/sysconfig/vdr-plugins.d/graphlcd.conf: example for "graphlcd" plugin configuration
- etc/sysconfig/vdr-plugins.d/mcli.conf: example for "mcli" plugin configuration
- etc/sysconfig/vdr-plugins.d/remote.conf: example for "remote" plugin configuration
- etc/sysconfig/vdr-plugins.d/softhddevice.conf: example for "softhddevice" plugin configuration
- etc/sysconfig/vdr-plugins.d/mpv.conf: example for "mpv" plugin configuration

### etc/X11

- etc/X11/xorg.conf.d/99-mode-1920x1080-50.conf: config example for displays to enforce proper video mode

### etc/systemd/reel

systemd (example) files for preparation and progress

### etc/systemd/system/vdr.service.d

- etc/systemd/system/vdr.service.d/override.conf: set CAP_NET_RAW required by vdr if mcli plugin is used (otherwise it cannot listen on the given interface using raw socket)

### sbin

- sbin/easyvdr-recordingaction: imported+extended from easyVDR
- sbin/easyvdr-shutdownaction: imported+extended from easyVDR
- sbin/easyvdr-lcd: script for controlling frontpanel LCD by options
- sbin/easyvdr-lcd: script for controlling frontpanel LED by options
- sbin/reelbox-control.sh: major merge of various original ReelBox scripts into one-and-only controllable by options

### share/images

start/stop/reboot/progress images (can be configured by GRAPHLCDIMAGEPREFIX, see /etc/sysconfig/reel)
- DONE: "ReelBoxNG"
- DONE: "BM2LTS"
- DONE: "easyVDR"

#### share/images/generator

- image source (XCF)
- image templates
- image generator script

### share/reel

very simple "ReelBoxNG" log

### share/vdr

- recording-hooks/20_easyvdr-led.custom: hook script to control the recording LED and (upcoming in VDR > 2.5.1) standby LED in case of editing/copying

## Further information

### Required plugins

## mcli

- Use 0.9.7 or newer from https://github.com/vdr-projects/vdr-plugin-mcli

## graphlcd

- ReelBox skin is integrated since release 1.0.3
- Select "st7565r-reel" as driver
