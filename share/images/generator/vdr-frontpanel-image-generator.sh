#!/bin/bash
#
# (P) & (C) 2017-2020 by Peter Bieringer <pb@bieringer.de>
#
# requires netpbm-progs

action="$1"
logo="$2"

file_prefix_brand="image-128x64"
file_prefix_action="image-128x14-action"
file_prefix_progress="image-128x10-progress"


file_brand_list="easyvdr reelboxng bm2lts"
file_action_final_list="reboot start shutdown stopping starting"
file_action_ongoing_list="starting stopping"


for brand in $file_brand_list; do
	echo "INFO  : generate images for brand: $brand"
	file_logo="${file_prefix_brand}-logo-${brand}.png"

	echo "DEBUG : file_logo=$file_logo"
	if [ ! -f "$file_logo" ]; then
		echo "ERROR : missing file: file_logo=$file_logo"
		exit 1
	fi
	pngtopnm "$file_logo" >"$file_logo.pnm" || exit 1

	echo "INFO  : generate progress images for brand: $brand"
	for i in $(seq 0 8); do
		file_progress="${file_prefix_progress}-$i.png"
		if [ ! -f "$file_progress" ]; then
			echo "ERROR : missing file: file_progress=$file_progress"
			exit 1
		fi
		pngtopnm "$file_progress" >"$file_progress.pnm" || exit 1

		for action in $file_action_ongoing_list; do
			file_action="${file_prefix_action}-${action}.png"
			file_output="${file_prefix_brand}-${brand}-${action}-progress-${i}.pnm"
			if [ ! -f "$file_action" ]; then
				echo "ERROR : missing file: file_action=$file_action"
				exit 1
			fi
			pngtopnm "$file_action" >"$file_action.pnm" || exit 1
			pnmcomp -xoff=0 -yoff=38 "$file_action.pnm" "$file_logo.pnm" | pnmcomp -xoff=0 -yoff=54 "$file_progress.pnm" - >"$file_output" || exit 1
			rm "$file_action.pnm"
		done
		rm "$file_progress.pnm"
	done

	echo "INFO  : generate final action images for brand: $brand"
	for action in $file_action_final_list; do
		file_action="${file_prefix_action}-${action}.png"
		file_output="${file_prefix_brand}-${brand}-${action}.pnm"
		if [ ! -f "$file_action" ]; then
			echo "ERROR : missing file: file_action=$file_action"
			exit 1
		fi

		pngtopnm "$file_action" >"$file_action.pnm" || exit 1
		echo "DEBUG : create final output file: $file_output"
		pnmcomp -xoff=0 -yoff=38 "$file_action.pnm" "$file_logo.pnm" >"$file_output" || exit 1
		rm "$file_action.pnm" || exit 1
	done

	rm "$file_logo.pnm" || exit 1
done
