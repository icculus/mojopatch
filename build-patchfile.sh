#!/bin/sh

# Just die if anything fails.
set -e

rm -f ~/Desktop/nwn.mojopatch

./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights Dedicated Server" --identifier "nwserver.icns" \
--version "0.0.1d1" --newversion "1.62" --replace --append --quietonsuccess \
--titlebar "Neverwinter Nights Dedicated Server 1.62 patch" \
"/Users/icculus/Desktop/nwn.mojopatch" \
"/Users/icculus/Desktop/Neverwinter Nights orig/Dedicated Server.app" \
"/Users/icculus/Desktop/nwn162/Dedicated Server.app"

./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights Main Game Icon" --identifier "nwmain.icns" \
--version "0.0.1d1" --newversion "1.62" --replace --append --quietonsuccess \
--titlebar "Neverwinter Nights 1.62 patch" \
"/Users/icculus/Desktop/nwn.mojopatch" \
"/Users/icculus/Desktop/Neverwinter Nights orig/Neverwinter Nights.app" \
"/Users/icculus/Desktop/nwn162/Neverwinter Nights.app"

# Just a blank dir, so everything in the patch dir is added in the patch...
mkdir -p tmp

./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights base folder" \
--version "Neverwinter Nights.app" --replace --append \
--titlebar "NWN Shadows of Undrentide Expansion Pack" \
"/Users/icculus/Desktop/nwn.mojopatch" \
"./tmp" \
"/Users/icculus/Desktop/XP1/XP1-1.62.8047e"

rmdir tmp

exit 0;

