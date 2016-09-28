% Simple x264/x265 Launcher
% Created by LoRd_MuldeR &lt;<mulder2@gmx>&gt; &ndash; check <http://muldersoft.com/> for news and updates!


# Introduction #

This application is a lightweight GUI front-end for the x264 H.264/AVC as well as the x265 H.265/HEVC encoder, based on the Qt toolkit.

Some key features of the Simple x264/x265 Launcher software include:

* Support for creating H.264/AVC (x264) and H.265/HEVC (x265) files
* Fully self-contained, *no* additional Codecs or Plugin's are required
* 64-Bit as well as 32-Bit encoder binaries are fully supported
* Optionally the "high bit-depth" encoder variants can be selected
* Batch encoding (job control) support is implemented
* If desired, multiple encoding jobs can be executed concurrently
* Adding new jobs via command-line interface is supported
* Input from the Avisynth *and* VapurSynth frame servers is supported
* 32-Bit Avisynth/VapourSynth can be mixed with 64-Bit x264/x265
* Straightforward encoder setup thanks to the Preset and Tuning system
* Custom encoder parameters can be added, if desired
* Easily manage your encoder configurations with user-defined templates
* Consistent "look & feel" on all systems thanks to the Qt toolkit


# System Requirements #


Simple x264 Launcher is *100% standalone*, i.e. it does **not** require Mircrosoft.NET, Java Runtime Environment or other "external" dependencies.

The required Qt DLLs as well as the encoder binaries are *included* with the application. Frameservers, like *Avisynth* or *VapourSynth*, may need to be installed separately though.

The minimum system requirements to run Simple x264/x265 Launcher are as follows:

* Windows XP with Service Pack 2 or any later Windows system &ndash; note that Windows XP is **not** recommended!
* 64-Bit editions of Windows are highly recommended, though 32-Bit editions will work as well
* The CPU must support at least the MMX and SSE instruction sets
* Avisynth input only available with [Avisynth](http://avisynth.nl/index.php/Main_Page#Official_builds) **2.6** installed &ndash; Avisynth **2.5.x** is *not* recommended!
* VapourSynth input only available with [VapourSynth](http://www.vapoursynth.com/) **r24+** installed &ndash; [Python](https://www.python.org/downloads/windows/) is a prerequisite for VapourSynth!
* YV16/YV24 color spaces require Avisynth 2.6 (see section 10)


# Anti-Virus Warning #

Occasionally your Antivirus program may mistakenly detect "malware" (virus, trojan, worm, etc.) in some of the files here. This is called a "False Positive" and the files are actually innocent/clean. It's an error in your specific Antivirus software. In case you encounter such problems, go to http://www.virustotal.com/ and check the file again with multiple Antivirus engines! And take care with results like "suspicious" , "generic" or "packed". Those are **not** real hits, they are just wild speculation. Apparently Antivirus programs tend to suspect installers/uninstaller created with NSIS. Furthermore some Antivirus programs blindly suspect all UPX'd (packed) executables of being malware. Obviously this is a stupid generalization, so you can safely ignore those warnings! Last but not least: Always keep in mind that this is OpenSource software! If you don't trust the people providing the pre-compiled binaries, download the source codes and compile them yourself.

**DON'T SUBMIT ANY VIRUS/TROJAN REPORTS, UNLESS YOU HAVE VERIFIED THE INFECTION WITH MULTIPLE ANTIVIRUS ENGNINES. THANKS!**


# License #

Simple x264/x265 Launcher is Copyright (C) 2004-2016 LoRd_MuldeR.

```
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
```

http://www.gnu.org/licenses/gpl-2.0.txt


# Third-Party Software #

The following third-party software is used by this application:

* The Qt GUI Toolkit is Copyright (C) 2012 Digia Plc.
  http://qt.digia.com/
  Qt is a free software, released under the terms of GNU GPL, version 3.

* The x264 encoder software is Copyright (C) 2003-2016 x264 project.
  http://www.videolan.org/developers/x264.html
  x264 is a free software and is released under the terms of the GNU GPL.

* The x265 encoder software is Copyright (C) 2013-2016 x265 project.
  http://www.videolan.org/developers/x265.html
  x265 is a free software and is released under the terms of the GNU GPL.

* AviSynth is Copyright (C) 2002-2006 Ben Rudiak-Gould et al.
  http://avisynth.nl/index.php/Main_Page
  AviSynth is a free software, released under the terms of the GNU GPL.

* VapourSynth is Copyright (c) 2012-2016 Fredrik Mellbin.
  http://www.vapoursynth.com/
  VapourSynth is free software, released under the terms of the GNU LGPL.

All third-party binaries included in this distribution package are redistributed in full accordance with the GNU General Public License, version 2. For further information see the respective web sites!


# Portable Mode #

Simple x264/x265 Launcher can be run in a "portable" mode. This may be helpful, for example, if you want to run the application directly from your USB stick, and on different machines. There is **no** separate *portable* version though, since *all* binaries of Simple x264/x265 Launcher support the *portable* mode. In order to trigger the *portable* mode, simply rename the EXE file to <tt>x264_launcher<span style="color:darkred">_portable</span>.exe</tt>. Once the *portable* mode has been enabled, *all* configuration files will be saved to (and loaded from) the same folder where the EXE file resides. Otherwise, they are stored in the user's <tt>%LOCALAPPDATA%</tt> folder. Note, however, that in *portable* mode, the program folder (i.e. the folder where the EXE file resides) must always be writeable. If the program folder happens to *not* be writeable, your settings can *not* and will *not* be saved, in *portable* mode!

***Note:*** Regardless of whether you enable the *portable* mode or not, the Simple x264/x265 Launcher application is **always** "portable", in the sense that the program is fully self-contained. It does *not* require any "external" libraries or frameworks to be installed on the computer. The only exceptions here are Avisynth and VapourSynth, which have to be installed *separately*, if they are needed. The *sole* purpose of the *portable* mode is to control where the configuration files will be stored. There is **no** such thing as a separate "portable" version of Simple x264/x265 Launcher.

## VapourSynth

There now is a "portable" edition **VapourSynth** available, which *Simple x264/x265 Launcher* can use. For this purpose, download the "portable" edition VapourSynth an *extract* it into the sub-directory `extra\VapourSynth` inside of the Simple x264/x265 Launcher "main" directory (i.e. where the `x264_launcher.exe` file is located). More specifically, the *32-Bit* version of "portable" VapourSynth needs to go to the `extra\VapourSynth\core32` sub-directory and the *64-Bit* version needs to go to the `extra\VapourSynth\core64` sub-directory. Be aware, however, that "portable" VapourSynth requires **embeddable Python** to work! So, you *must* download Python as "embeddable zip file" from the Python web-site and extract that into the same sub-directory as the VapourSynth "portable" binaries. In the end, it should look like this:
```
InstallPath
├─ x264_launcher_portable.exe
└─ extra\
   └─ VapourSynth\
      ├─ core32\
      │  ├─ python35.dll     <32-Bit>
      │  ├─ VapourSynth.dll  <32-Bit>
      │  ├─ VSPipe.exe       <32-Bit>
      │  └─ etc…
      └─ core64\
         ├─ python35.dll     <64-Bit>
         ├─ VapourSynth.dll  <64-Bit>
         ├─ VSPipe.exe       <64-Bit>
         └─ etc…
```

# Updating Your Encoder Binaries #

This application works best with the encoder binaries that are included in the distribution package. That's because these binaries have been tested to work properly with the GUI. Nonetheless, in some cases, you may wish to replace the included binaries with a newer encoder version or with an alternative build of the same version. Generally, newer versions of x264/x265 should work properly, though there is **no** guarantee! In rare cases, the CLI syntax (or console output) may have changed in a way that breaks compatibility with the GUI program. Furthermore, this application does **not** provide any support for "unofficial" patches. Usually custom builds that contain such patches will work anyway, but again there is **no** guarantee. Also, using *outdated* binaries with this application is **not** supported or intended. Please report bugs rather than reverting to an old version!


# Timeout Warning #

This application provides "deadlock" prevention. This means that if an encoder process (x264 or Avisynth) stops responding, it will be terminated. This is done in order to ensure that the main program as well as the other encoder processes can continue properly. More specifically, a warning will be raised if the process does not respond for one minute. If the process still didn't respond after five minutes, it will be terminated and consequently the encoding job is aborted. In some rare cases, your Avisynth script may take a very long time to initialize and thus the process will be aborted before the encoding can start. For example, this can happen if FFMS2/FFVideoSource takes a very long time to index the source file. In that case, we recommend to index the source file beforehand, e.g. by using the 'ffmsindex' tool.


# Custom Parameters #

This application provides a "Custom Parameters" edit box. All command-line parameters that you enter there will be passed to x264 or x265 unmodified. You can send arbitrary parameters - even such ones that are only available in patched builds. See the x264/x265 manuals or the Help Screen for a list of available parameters. However be aware that the GUI will not check your parameters at all! Thus using an unknown or unsupported parameter will cause your encode to fail. Using an existing parameter in the wrong way will cause your encode to fail too. Last but not least, some parameters are forbidden by the GUI. If some parameter is forbidden, that's because the GUI will set that parameter for you (if required) or because that parameter is NOT compatible with the GUI.

HINT: Occasionally your custom parameters string may become very long, especially when working with zones. In that case you can right-click on the "Custom Parameters" edit box and choose "Open the Text-Editor". This will open a multi-line text editor for easier parameter handling!


# Color Spaces / Chroma Subsampling #

Avs2YUV converts the output of your Avisynth script to the YV12 format, i.e. YUV data with 4:2:0 chroma subsampling and 8-Bit precision. Usually this is exactly what you want/need. If, however, your Avisynth script outputs image data with a higher chroma resolution, e.g. YUY2 (4:2:2), then the conversion to YV12 (4:2:0) will discard some of the information. In that case, and if you want/need to keep the full chroma resolution of your Avisynth script's output, you will have to pass the <tt>-csp</tt> switch to Avs2YUV as a custom parameter! Use <tt>-csp I422</tt> for YUV 4:2:2 (YV16) and use <tt>-csp I444</tt> for YUV 4:4:4 (YV24). Please note that Avs2YUV can NOT pass through the "packed" YUY2 format. Thus it has to be converted to the "planar" YV16 format. As both, YUY2 and YV16, are YUV 4:2:2 formats, converting from YUY2 to YV16 is a lossless operation. Note, however, that Avisynth 2.5 did NOT support YV16/YV24, so you need to use Avisynth 2.6; otherwise Avs2YUV will fail to do the conversion! Also be aware that the x264 encoder itself will convert any YV16 or YV24 input back to the YV12 format, if you don't pass the suitable <tt>--output-csp i422/i444</tt> switch to x264 as a custom parameter! In short, to encode YUY2 from Avisynth, you have to pass <tt>-csp I422</tt> to Avs2YUV and <tt>--output-csp i422</tt> to x264 to avoid 4:2:0 downsampling.


# Audio Processing/Encoding #

This application is a front-end to the x264 encoder. And, as x264 does NOT support audio processing/encoding yet, there is NO explicit support for audio encoding in this application. Thus, if you want to create a video file *with* audio, you will have to add the audio stream to the encoded video file afterwards. This process is called 'multiplexing' or just 'muxing'. In case you are dealing with Matroska (MKV) files, then "MKVMerge GUI" from the "MKVToolNix" package is the right tool for this task. If, instead, you are dealing with MP4 files, then you may use "MP4Box" or the "YAMB" front-end for muxing the audio stream.

Having said all that, there now is an unofficial "Audio" branch of x264 available. If you are using one of the modified x264 builds with "audio support" patch (e.g. those provided by JEEB), then you can process the audio with x264 and skip the additional muxing step. Basically the audio patch adds a new <tt>--acodec</tt> switch, which you can pass to x264 as a custom parameter. For example, you can pass <tt>--acodec aac</tt> for encoding the audio to the AAC format (recommended for MP4 files). Or you can pass <tt>--acodec vorbis</tt> for encoding the audio to the Ogg/Vorbis format (recommended for MKV files). Please be aware that audio encoding will work only, if your input file contains an audio stream! When using the built-in LAVF/FFMS input of x264, the audio can be encoded straight from the input file. This does NOT work with Avisynth input! Instead, if you want to encode audio from an Avisynth script, you must pass the <tt>--audiofile <path_to_avs_file></tt> switch to x264 as a custom parameter. For convenience, the string <tt>--audiofile $(INPUT)</tt> may be used.


# OpenCL Support #

Newer builds of x264 now support OpenCL Lookahead, i.e. GPU accelerated encoding. This can be enabled with the <tt>--opencl</tt> custom parameter. But OpenCL Lookahead will only work if you have an OpenCL-capable graphics card *and* if you have an up-to-date video driver installed!

Note that x264 will now *only* try to load the OpenCL.DLL if you really use the <tt>--opencl</tt> option. Therefore, the "dummy" OpenCL.DLL included in older versions of the Simple x264 Launcher is **not** needed any more!!


# Command-line Syntax #

**PLEASE NOTE:** These are parameters you can pass to Simple x264 Launcher (`x264_launcher.exe`), they can **NOT** be passed to x264 or x265 as "custom" parameters !!!

The following command-line switches are available:
```
--add-file="<file>" .............. Create a new job via GUI dialog
--add-job="<src>|<dest>|<tpl>" ... Create a new job directly from CLI
--[no-]force-start ............... Start the next job immediately
--[no-]force-enqueue ............. Append the next job to the queue
--skip-avisynth-check ............ Skip Avisynth detection
--skip-vapoursynth-check ......... Skip VapourSynth detection
--skip-version-checks    ......... Skip x264/x265 version checks, NOT recommended!
--force-cpu-no-64bit ............. Forcefully disable 64-Bit support
--no-deadlock-detection .......... Do not abort a sub-process on possible deadlock
--[no-]console ................... Do [not] show the Debug console
--no-style ....................... Don't use the Qt "Plastique" style
```

Some details on the "--add-job" command-line switch:
```
<src> .... Specifies the source media file or AVS/VPY script file
<dest> ... Specifies the output H.264/HEVC/MKV/MP4 file to be written
<tpl> .... Specifies the template to be used ("-" uses defaults)
```

Use `--[no-]force-start` or `--[no-]force-enqueue` to tweak startup behavior. If neither is used, the default startup behavior applies.


# Downloads & Updates #

Please download the latest version of Simple x264/x265 Launcher from one of the official download mirrors:

* https://github.com/lordmulder/Simple-x264-Launcher/releases/latest
* https://bitbucket.org/lord_mulder/simple-x264-launcher/downloads
* https://x264launcher.codeplex.com/releases/
* http://sourceforge.net/projects/muldersoft/files/Simple%20x264%20Launcher/
* https://www.assembla.com/spaces/simple-x264-x265-launcher/documents


# Help & Support #

For help and support, please join the discussion at:
* http://forum.doom9.org/showthread.php?t=144140

Please *avoid* sending me e-mail with support requests. I get a lot of mail every day and cannot answer everything. Thank you!


# Source Codes #

Simple x264/x265 Launcher is written in C++ and currently developed with Microsoft Visual Studio 2013. It is based on the Qt toolkit.

The source codes can be obtained from the official Git repository:

* https://github.com/lordmulder/Simple-x264-Launcher
* https://gitlab.com/simple-x264-launcher/simple-x264-launcher/
* https://bitbucket.org/lord_mulder/simple-x264-launcher/src
* https://www.assembla.com/code/simple-x264-x265-launcher/git/nodes
* https://x264launcher.codeplex.com/SourceControl/latest

Download Visual Studio Express 2013 for Windows *Desktop* here:
* http://www.visualstudio.com/en-us/downloads/download-visual-studio-vs

Download the latest Qt toolkit (4.8.x) from the Qt Project web-site:
* http://download.qt-project.org/official_releases/qt/4.8/
* http://sourceforge.net/projects/lamexp/files/Miscellaneous/Qt%20Libraries/

&nbsp;  
&nbsp;  
**e.o.f.**
