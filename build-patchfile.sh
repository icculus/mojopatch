#!/bin/sh

# Just die if anything fails.
set -e

# revert everything to a sane state.
make
strip -S ./bin/mojopatch
mv ./bin/mojopatch ./MojoPatch.app/Contents/MacOS/
rm -rf ./MojoPatch.app/default.mojopatch
rm -rf "/Users/icculus/Desktop/Shadows of Undrentide Installer.app"
rm -rf "/Users/icculus/Desktop/Hordes of the Underdark Installer.app"

# Just a blank dir, so everything in the patch dir is added in the patch...
mkdir -p tmp

# Put the 1.62 patch together...
./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights" \
--identifier "nwmain.icns" \
--version "0.0.1d1" --newversion "1.62" --replace --append --quietonsuccess \
--titlebar "Neverwinter Nights 1.62 patch" \
"./MojoPatch.app/default.mojopatch" \
"./tmp" \
"/Users/icculus/Desktop/nwn162"

# Copy initial patchfile to seperate expansion pack installers...
echo "copying initial patch to SoU branch..."
cp -R MojoPatch.app "/Users/icculus/Desktop/Shadows of Undrentide Installer.app"
rm -f "/Users/icculus/Desktop/Shadows of Undrentide Installer.app/Contents/MacOS/xdelta"

echo "copying initial patch to HotU branch..."
cp -R MojoPatch.app "/Users/icculus/Desktop/Hordes of the Underdark Installer.app"
rm -f "/Users/icculus/Desktop/Hordes of the Underdark Installer.app/Contents/MacOS/xdelta"

# Add XP1 data to patchfile...
./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights" \
--identifier "SoU" \
--version "a" --newversion "b" --replace --append --quietonsuccess \
--titlebar "NWN Shadows of Undrentide Expansion Pack" \
"/Users/icculus/Desktop/Shadows of Undrentide Installer.app/default.mojopatch" \
"./tmp" \
"/Users/icculus/Desktop/XP1/XP1-1.62.8047e"

# Add XP2 data to the other patchfile...
./MojoPatch.app/Contents/MacOS/mojopatch  --create --alwaysadd --zliblevel 1 \
--product "Neverwinter Nights" \
--identifier "HotU" \
--version "a" --newversion "b" --replace --append \
--titlebar "NWN Hordes of the Underdark Expansion Pack" \
"/Users/icculus/Desktop/Hordes of the Underdark Installer.app/default.mojopatch" \
"./tmp" \
"/Users/icculus/Desktop/XP2/XP2-1.62.8047e"

rmdir tmp

exit 0;

