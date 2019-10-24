A1243Dvorak
===========
Dvorak keyboard layout firmware for Apple Aluminum USB Keyboard (A1243)

What?
-----
I’ve always wanted a hardware Dvorak keyboard, but since 2007 or so my preferred keyboard has been the Apple aluminum keyboard with numeric keypad. I came to prefer the low-profile keys and how quiet they are, despite not being as ergonomic as the various curved keyboards that I liked before. I also find that the scissor mechanisms are more reliable when you aren't hitting the keys dead-on, unlike the more recent curved keyboards from Microsoft. Another issue is that I prefer a wired keyboard for security and reliability reasons. At this point, I think the only other keyboard I would consider switching to is the Kinesis Advantage 2.

Why?
----
There are a bunch of reasons to prefer a hardware keyboard layout over software:
1. If you want to interact with a machine before the OS loads, such as to access the BIOS or bootloader.
2. Lots of apps, especially videogames, have scancodes hard-coded into them because they interact with the keyboard at a much lower level than most programs.
3. When using a machine inside a VM or over a VNC connection, the remote end is often receiving raw USB events.
4. I found that I would inadvertently trigger the keyboard shortcut for switching back to QWERTY because it's very close to some of the IntelliSense bindings.
5. It just seems like a cool idea.

Whence?
-------
I ran across [this paper](http://www.hakim.ws/BHUSA09/speakers/Chen_Reversing_Apple_Firmware/BlackHat-USA-09-Chen-Reversing-Apple-Firmware-Update-wp.pdf) that talks about some of the steps towards reverse-engineering the firmware for the A1243 keyboard. Since having a hardware Dvorak version of this exact keyboard has been a fantasy of mine, the information in this paper put it within the realm of possibility for me.

How?
----
I used some of the information in the paper to un-obsfucate the firmware image (see Codec.cc), then was able to disassemble it using [this tool](https://github.com/jcwren/m8cdis). Gradually I added a bunch of chip-specific info to the disassembler, made some bug fixes, and managed to figure out where in the image the table of USB scancodes existed (see FindKeys.cc).

Then I made a tool (see Patch.cc) to replace the scancode table with a new layout and pad the data until the checksum matches what Apple’s firmware upgrade utility expects. I created two new layouts based on the Dvorak software layout shipped with macOS:

**Dvorak (dvorak.patch)**
![](https://raw.githubusercontent.com/sethk/A1243Dvorak/master/Images/dvorak.patch.png)
In this layout I’ve also swapped the left Ctrl key with Caps Lock. The fact that computer keyboards have dedicated such prime key real estate to a key designed to hold up a heavy type bar that computers don’t even have is somewhat maddening.

**Dvorak for Windows (dvorak-win.patch)**
![](https://raw.githubusercontent.com/sethk/A1243Dvorak/master/Images/dvorak-win.patch.png)
This layout is optimized for Windows, with the Alt and Meta (Win) keys where you would expect. In addition I've moved the Fn key down where Left Ctrl was, put Insert where Fn was, and brought back the SysRq, Scroll Lock, and Pause keys in place of F13-F15.
