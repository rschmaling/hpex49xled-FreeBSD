# hpex49xled-FreeBSD
HP EX49x Mediasmart Server Hard Drive LED Monitoring service

LED monitoring service for HP Mediasmart Server EX49x (I'm assuming EX48x) FreeBSD 12.3 and above. Should work on any version that supports CAM, devstat, etc. 

A few notes:

1. I can't thank the original programmers of the mediasmartserverd enough for all their efforts and code - I have ported the code for Acer Altos, H340 - H342 into this service. HOWEVER - I have not activated the code.
2. If you are using an H340 - H342 or Atmos as supported in the Linux mediasmartserverd - Please compile and run the camtest program I included (just type make camtest) and send me the results in the issues section here on github. I can use that information to ensure the path id, unit number, etc., align and are properly accounted for during initialization. 
3. HOT Swap Works - feel free to add/pull drives - the service will detect and adjust for these.
4. hpex49xled is threaded - one thread per disk. I am only looking at IDE devices, I am only looking for four devices, and I am only looking at the four devices in the      enclosure. If adding external eSATA or USB drives causes an issue - please report it to me with some 
   trace information (like what camtest is telling you the box sees) and I'll track down the issue and fix the code.
5. Running 'make install' as root - install expects that /usr/local/etc/rc.d exists. This is where the .rc file is installed to. If you don't want it to go there, change the rcprefix in the make file.
6. after running 'make install' as root - you will need to add the following to the bottom of your /etc/rc.conf file: hpex49xled_enable="YES" - just copy and paste as-is.
7. Update Monitoring: hpex49xled now monitors for freebsd-update updatesready. You must have "@daily root /usr/sbin/freebsd-update -t root cron" in cron or equivilent. Use the --update command line parameter. Add hpex49xled_args="--update" in /etc/rc.conf to enable at startup.
