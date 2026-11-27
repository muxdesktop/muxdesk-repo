#!/bin/sh
# MuxOS test path — "New Folder" for the desktop right-click menu.
# Creates "Untitled Folder" (or "Untitled Folder 2", ...) in ~/Desktop.
d="$HOME/Desktop"
mkdir -p "$d"
n="Untitled Folder"
i=2
while [ -e "$d/$n" ]; do
  n="Untitled Folder $i"
  i=$((i+1))
done
mkdir "$d/$n"
