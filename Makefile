CXXFLAGS+= -Wall -Wextra -g -fno-inline -O0 -std=c++17
M8CDIS = Vendor/m8cdis/m8cdis

.SUFFIXES:

all: HIDFirmwareUpdaterTool.s \
    HIDFirmwareUpdaterTool.hex.s \
    HIDFirmwareUpdaterTool.hacked \
    kbd_0x0069_0x0220.asm \
    kbd_0x0069_0x0220.keys \
    CheckSum \
    kbd_0x0069_0x0220.def.irrxfw \
    kbd_0x0069_0x0220.def.keys \
    dvorak.irrxfw \
    dvorak.keys \
    dvorak-win.irrxfw \
    dvorak-win.keys

HIDFirmwareUpdaterTool.s: HIDFirmwareUpdaterTool
	otool -tV -arch i386 $< > $@ || (rm -f $@; false)

HIDFirmwareUpdaterTool.hacked.s: HIDFirmwareUpdaterTool.hacked
	otool -tV -arch i386 $< > $@ || (rm -f $@; false)

HIDFirmwareUpdaterTool.hex.s: HIDFirmwareUpdaterTool
	otool -tV -j $< > $@ || (rm -f $@; false)

HIDFirmwareUpdaterTool.hacked: HIDFirmwareUpdaterTool.hacked.xxd
	xxd -r $< $@
	chmod +x $@

cmp_tool:: HIDFirmwareUpdaterTool HIDFirmwareUpdaterTool.hacked HIDFirmwareUpdaterTool.s HIDFirmwareUpdaterTool.hacked.s
	-cmp -l HIDFirmwareUpdaterTool HIDFirmwareUpdaterTool.hacked
	-diff -u HIDFirmwareUpdaterTool.s HIDFirmwareUpdaterTool.hacked.s

Codec: Codec.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

CheckSum: CheckSum.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

kbd_0x0069_0x0220.hex: kbd_0x0069_0x0220.irrxfw Codec
	./Codec < $< > $@ || (rm -f $@; false)

kbd_0x0069_0x0220.asm: kbd_0x0069_0x0220.hex kbd_0x0069_0x0220.mp $(M8CDIS)
	-$(M8CDIS) -i $< -o $@ -b -e -s -u -p cy7c63923 #|| (rm -f $@; false)

FindKeys: FindKeys.cc USBKeys.inl
	$(CXX) $(CXXFLAGS) -o $@ $<

%.keys: %.hex FindKeys
	./FindKeys < $< > $@ || (rm -f $@; false)

Patch: Patch.cc USBKeys.inl HexFile.inl kbd_0x0069_0x0220.hex
	$(CXX) $(CXXFLAGS) -o $@ $<
	./Patch /dev/null < kbd_0x0069_0x0220.hex | diff - kbd_0x0069_0x0220.hex || (rm -f $@; false)

kbd_0x0069_0x0220.def.hex: kbd_0x0069_0x0220.hex Patch default.patch
	./Patch default.patch < kbd_0x0069_0x0220.hex > kbd_0x0069_0x0220.def.hex || (rm -f $@; false)
	diff kbd_0x0069_0x0220.def.hex kbd_0x0069_0x0220.hex || (rm -f $@; false)

dvorak.hex: Patch dvorak.patch kbd_0x0069_0x0220.hex
	./Patch dvorak.patch < kbd_0x0069_0x0220.hex > $@ || (rm -f $@; false)

dvorak-win.hex: Patch dvorak-win.patch kbd_0x0069_0x0220.hex
	./Patch dvorak-win.patch < kbd_0x0069_0x0220.hex > $@ || (rm -f $@; false)

kbd_0x0069_0x0220.def.irrxfw: kbd_0x0069_0x0220.def.hex
	./Codec < kbd_0x0069_0x0220.def.hex > $@ || (rm -f $@; false)
	cmp -b $@ kbd_0x0069_0x0220.irrxfw || (rm -f $@; false)

dvorak.irrxfw: dvorak.hex
	./Codec < dvorak.hex > $@ || (rm -f $@; false)

dvorak-win.irrxfw: dvorak-win.hex
	./Codec < dvorak-win.hex > $@ || (rm -f $@; false)

load_dvorak:: dvorak.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/dvorak.irrxfw

load_dvorak_win:: dvorak-win.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/dvorak-win.irrxfw

load_default:: kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

load_default_24f:: kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

debug_load_default::
	lldb HIDFirmwareUpdaterTool.hacked -- -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

watch_log:
	syslog -w 0 -k Sender HIDFirmwareUpdaterTool.hacked

clean::
	rm -f dvorak.irrxfw dvorak.keys dvorak.hex
	rm -f dvorak-win.irrxfw dvorak-win.keys dvorak-win.hex
	rm -f kbd_0x0069_0x0220.def.irrxfw kbd_0x0069_0x0220.def.hex kbd_0x0069_0x0220.def.keys
	rm -f kbd_0x0069_0x0220.keys kbd_0x0069_0x0220.asm kbd_0x0069_0x0220.hex
	rm -f Patch FindKeys CheckSum Codec
	#rm -f HIDFirmwareUpdaterTool.hacked HIDFirmwareUpdaterTool.hex.s
