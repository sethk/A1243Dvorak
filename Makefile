CXXFLAGS+= -Wall -Wextra -g -fno-inline -O0 -std=c++17
M8CDIS = Vendor/m8cdis/m8cdis

.SUFFIXES:

all: HIDFirmwareUpdaterTool.s \
    HIDFirmwareUpdaterTool.hex.s \
    HIDFirmwareUpdaterTool.hacked \
    kbd_0x0069_0x0220.asm \
    kbd_0x0069_0x0220.keys \
    CheckSum \
    kbd_0x0069_0x0220.def.hex \
    kbd_0x0069_0x0220.def.keys \
    dvorak.hex

HIDFirmwareUpdaterTool.s: HIDFirmwareUpdaterTool
	otool -tV -arch i386 $< > $@ || rm -f $@

HIDFirmwareUpdaterTool.hacked.s: HIDFirmwareUpdaterTool.hacked
	otool -tV -arch i386 $< > $@ || rm -f $@

HIDFirmwareUpdaterTool.hex.s: HIDFirmwareUpdaterTool
	otool -tV -j $< > $@ || rm -f $@

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
	./Codec < $< > $@ || rm -f $@

kbd_0x0069_0x0220.asm: kbd_0x0069_0x0220.hex kbd_0x0069_0x0220.mp $(M8CDIS)
	$(M8CDIS) -i $< -o $@ -b -e -s -u -p cy7c63923 #|| rm -f $@

FindKeys: FindKeys.cc USBKeys.inl
	$(CXX) $(CXXFLAGS) -o $@ $<

kbd_0x0069_0x0220.keys: kbd_0x0069_0x0220.hex FindKeys
	./FindKeys < $< > $@ || rm -f $@

Patch: Patch.cc USBKeys.inl HexFile.inl kbd_0x0069_0x0220.hex
	$(CXX) $(CXXFLAGS) -o $@ $<
	./Patch /dev/null < kbd_0x0069_0x0220.hex | diff -q - kbd_0x0069_0x0220.hex

kbd_0x0069_0x0220.def.hex: kbd_0x0069_0x0220.hex Patch default.patch
	./Patch default.patch < kbd_0x0069_0x0220.hex > kbd_0x0069_0x0220.def.hex || rm -f kbd_0x0069_0x0220.def.hex
	diff -q kbd_0x0069_0x0220.def.hex kbd_0x0069_0x0220.hex

kbd_0x0069_0x0220.def.keys: kbd_0x0069_0x0220.def.hex FindKeys
	./FindKeys < $< > $@ || rm -f $@

dvorak.hex: Patch dvorak.patch kbd_0x0069_0x0220.hex
	./Patch dvorak.patch < kbd_0x0069_0x0220.hex > dvorak.hex || rm -f dvorak.hex

kbd_0x0069_0x0220.def.irrxfw: kbd_0x0069_0x0220.def.hex
	./Codec < kbd_0x0069_0x0220.def.hex > kbd_0x0069_0x0220.def.irrxfw
	cmp -b kbd_0x0069_0x0220.def.irrxfw kbd_0x0069_0x0220.irrxfw

dvorak.irrxfw: dvorak.hex
	./Codec < dvorak.hex > dvorak.irrxfw || rm -f dvorak.irrfx

load_dvorak:: dvorak.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/dvorak.irrxfw

load_default:: kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

load_default_24f:: kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

debug_load_default::
	lldb HIDFirmwareUpdaterTool.hacked -- -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

watch_log:
	syslog -w 0 -k Sender HIDFirmwareUpdaterTool.hacked
