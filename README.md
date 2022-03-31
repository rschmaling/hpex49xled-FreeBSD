# hpex49xled-FreeBSD
HP EX49x Mediasmart Server Hard Drive LED Monitoring service

LED monitoring service for HP Mediasmart Server EX49x (I'm assuming EX48x) FreeBSD 12.3 and above. Should work on any version that supports CAM, devstat, etc. 

A few notes:

1. I can't thank the original programmers of the mediasmartserverd enough for all their efforts and code - I have ported the code for Acer Atmos, H340 - H342 into this service. HOWEVER - I have not activated the code.
2. If you are using an H340 - H342 or Atmos as supported in the Linux mediasmartserverd - Please compile and run the camtest program I included (just type make camtest) and send me the results at rob.schmaling@gmail.com or post them here. I can use that 
    information to ensure the path id, unit number, etc., align and are properly accounted for during initialization. 
3. HOT Swap Works with caveats:
  a. For whatever reason, the drive bay you extract a drive from - the bay light may illuminate. I don't know why this happens, and it won't turn off unless you re-start the box. Even restarting the service doesn't affect it - and the activity lights do not appear to work 
      after this happens but YMMV.
  b. devstat_getdevs() returns with an allocation error (randomly) after drive insertion/removal. I believe this is a thread issue where a thread hasn't terminated, free'd some variables, and devstat_getdevs is still trying to use them. I'm working on tracing this down 
      so, unless you plan to extract/insert/extract ad nauseam, it shouldn't and hasn't been an issue for day-to-day operations.
4. hpex49xled is threaded - one thread per disk. I am only looking at IDE devices, I am only looking for four devices, and I am only looking at the four devices in the enclosure. If adding external eSATA or USB drives causes an issue - please report it to me with some 
    trace information (like what camtest is telling you the box sees) and I'll track down the issue and fix the code.
