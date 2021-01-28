# ReelBoxNG
ReelBox Next Generation Helper/Support Scripts/Tools/Code

Content of this repository will support creating ReelBoxNG based on self-made mainboard exchange

## Support
- DONE: frontpanel display
- PARTIAL: frontpanel button (POWER-only so far)
- DONE: ReelBox remote control unit via frontpanel
- DONE: frontpanel LEDs
- DONE: netceiver

## History
starting with easyVDR, switching to BM2LTS, back to easyVDR, now running on vanilla Fedora Linux distribution

## Notes
Note: in case of taking use in easyVDR or BM2LTS, please feed back all required extensions via Pull Requests

## Coming next
- schematics for frontpanel connection (serial and towards mainboard)

## Content

### src-reelvdr

Contain source code for
- reelfpctl
- reelbox-ctrld

By scripts expected target for binaries would be: /opt/reel/sbin (use e.g. softlink)

### etc

Contains script / config files, can be either stored directly in related OS directories or use softlink to the cloned directory

### etc/sysconfig

- etc/sysconfig/reel: config file related to ReelBox

- etc/sysconfig/vdr (not shown): suggested action options would be: --shutdown=/opt/reel/sbin/easyvdr-shutdownaction --record /opt/reel/sbin/easyvdr-recordingaction

### etc/X11

- etc/X11/xorg.conf.d/99-mode-1920x1080-50.conf: config example for displays to enforce proper video mode

### etc/systemd/reel

systemd (example) files for preparation and progress

### sbin

- sbin/easyvdr-recordingaction: imported+extended from easyVDR
- sbin/easyvdr-shutdownaction: imported+extended from easyVDR
- sbin/easyvdr-lcd: script for controlling frontpanel LCD by options
- sbin/easyvdr-lcd: script for controlling frontpanel LED by options
- sbin/reelbox-control.sh: major merge of various original ReelBox scripts into one-and-only controllable by options

### share/images

progress images for
- DONE: "ReelBoxNG"
- ON-REQUEST: "BM2LTS" (have to recreate them)
- ON-REQUEST: "easyVDR" (have to recreate them)

### share/reel

very simple "ReelBoxNG" log

### share/vdr

- recording-hooks/20_easyvdr-led.custom: hook script to control the recording LED and (upcoming in VDR > 2.5.1) standby LED in case of editing/copying
