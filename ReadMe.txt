Simple x264 Launcher - use 64-Bit x264 with 32-Bit Avisynth
Copyright (C) 2004-2012 LoRd_MuldeR <MuldeR2@GMX.de>


1. Introduction
---------------

This program is a simple GUI front-end for the x264 H.264/AVC encoder.
Thanks to Avs2YUV, this program can use the 64-Bit version of x264 with
the 32-Bit version of Avisynth. This way you can keep on using your
favorite 32-Bit Avisynth plug-ins (e.g. DGDecodeNV) and still benefit
from the speed improvements of 64-Bit x264. Of course you can also use
64-Bit Avisynth, which is still considered experimental, if desired.
And of course you can use x264's built-in LAVF/FFMS input instead of
Avisynth just as well. Moreover this program provides full batch
encoding support. This means that you can run several encoding jobs in
parallel. Or you can let them run in sequence - one by one.


2. System Requirements
----------------------

This program runs on Windows XP with Service Pack 2 and later.
64-Bit Windows is highly recommended, but 32-Bit Windows works as well.
The CPU should support at least the MMX and SSE1 instruction sets.
Avisynth 2.5.x must be installed in order to use Avisynth (.avs) input.


3. Anti-Virus Warning
---------------------

Occasionally your Antivirus program may mistakenly detect "malware"
(virus, trojan, worm, etc.) in some of the files here. This is called a
"False Positive" and the files are actually innocent/clean. It´s an
error in your specific Antivirus software.

In case you encounter such problems, go to http://www.virustotal.com/
and check the file again with multiple Antivirus engines! And take care
with results like "suspicious" , "generic" or "packed". Those are *not*
real hits, they are just wild speculation.

Apparently Antivirus programs tend to suspect installers/uninstaller
created with NSIS. Furthermore some Antivirus programs blindly suspect
all UPX´d (packed) executables of being malware. Obviously this is a
stupid generalization, so you can safely ignore those warnings!

Last but not least: Always keep in mind that this is OpenSource
software! If you don´t trust the people providing the pre-compiled
binaries, download the source codes and compile them yourself.

DON´T SUBMIT ANY VIRUS/TROJAN REPORTS, UNLESS YOU HAVE VERIFIED THE
INFECTION WITH MULTIPLE ANTIVIRUS ENGNINES. THANKS!


4. License
----------

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

http://www.gnu.org/licenses/gpl-2.0.txt


5. Portable Mode
----------------

This application can be run in "portable" mode. Just rename the EXE
file to 'x264_launcher_portable.exe' in order to trigger portable mode.
In that mode all configuration files will be saved in the same folder
where the EXE file resides. This may be helpful, if you want to run the
application directly from your USB stick on different computers. Note,
however, that in portable mode the install folder must be writable!


6. Updating Your x264 Binaries
------------------------------

This application works best with the x264 binaries that are included in
the distribution package. That's because these binaries have been
tested to work properly with the GUI. Nonetheless in some cases you may
want to replace the included binaries with a newer version of x264 or
with an alternative build of the same version. Generally newer versions
of x264 should work with the GUI too, though there is NO guarantee. In
rare cases the CLI syntax (or console output) of x264 may change in a
way that breaks the compatibility and therefore will require an update
of the GUI itself. Furthermore this application does NOT provide any
support for unofficial x264 patches. Usually x264 builds that contain
unofficial patches will work anyway, but again there is NO guarantee.
Using old/outdated x264 binaries with this application is NOT supported
or intended. Report bugs rather than reverting to an old version!


7. Timeout Warning
------------------

This application provides "deadlock" prevention. This means that if an
encoder process (x264 or Avisynth) stopps responding, it will be
terminated. This is done in order to ensure that the main program as
well as the other encoder processes can continue properly. More
specifically, a warning will be raised if the process does not respond
for one minute. If the process still didn't respond after five minutes,
it will be terminated and consequently the encoding job is aborted. In
some rare cases, your Avisynth script may take a very long time to
initialize and thus the process will be aborted before the encoding can
start. For example, this can happen if FFMS2/FFVideoSource takes a very
long time to index the source file. In that case, we recommend to index
the source file beforehand, e.g. by using the 'ffmsindex' tool.


8. Command-line Syntax
----------------------

The following command-line switches are available:

--add <file> [<file>] ... Create new job(s) from files(s)
--console ............... Show the "debug" console
--no-console ............ Don't show the "debug" console
--no-style .............. Don't use the Qt "Plastique" style
--force-cpu-no-64bit .... Forcefully disable 64-Bit support


9. Help & Support
-----------------

For help and support, please join the discussion at:
http://forum.doom9.org/showthread.php?t=144140

Please do NOT send me e-mail with support requests. Thanks!


e.o.f.
