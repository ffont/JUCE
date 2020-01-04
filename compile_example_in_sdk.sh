#!/bin/bash

# Step 0) start docker samba (if not running already)

if docker ps | grep -q 'samba'; then 
    echo "- Docker samba container already running"; 
else
    echo "- Starting docker samba container (might ask for computer's password)"
    docker start samba && sudo ifconfig lo0 127.0.0.2 alias up
fi

if docker ps | grep -q 'samba'; then 
    echo "- Samba drive already mounted"; 
else
    echo "- Mounting samba drive (choose Guest when prompted)"
    open 'smb://127.0.0.2/workdir'
    echo "Now re-run this command once the samba drive is mounted!"
    exit 0
fi

# Step 1) copy JUCE_ffont folder to ELK SDK mounted volume (requires samba volume mounted)
# TODO: make it preserve build folder if already existing
echo "- Copying source files to samba drive locatio (preserving current build folder if existing)"
#rm -fr /Volumes/workdir/build_tmp
#cp -r /Volumes/workdir/JUCE_ffont/extras/MinimalExampleELKDrivers/Builds/LinuxMakefile/build /Volumes/workdir/build_tmp
rsync -av --progress /Users/ffont/Developer/JUCE_ffont/ /Volumes/workdir/JUCE_ffont --exclude .git > /dev/null
#mv /Volumes/workdir/build_tmp /Volumes/workdir/JUCE_ffont/extras/MinimalExampleELKDrivers/Builds/LinuxMakefile/build 

# Step 2) cross-compile
echo "- Cross-compiling project (using modified esdk-launch.py file)"
docker run --rm -it -v elkvolume:/workdir -v /Users/ffont/Developer/JUCE_ffont/custom-esdk-launch.py:/usr/bin/esdk-launch.py crops/extsdk-container

# Step 3) copy compiled executable to board
echo "- Copying executable to board (will ask for board's password, which is 'elk'. consider configuring ssh keys if asking password...)"
scp /Volumes/workdir/JUCE_ffont/extras/MinimalExampleELKDrivers/Builds/LinuxMakefile/build/MinimalExampleELKDrivers mind@elk-pi.local:

# All done!
echo ""
echo "Done! now you can run the command in the board..."
