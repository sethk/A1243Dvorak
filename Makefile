CXXFLAGS+= -Wall -Wextra -g -fno-inline -O0 -std=c++17

.SUFFIXES:

main: HIDFirmwareUpdaterTool.hacked dvorak.irrxfw dvorak-win.irrxfw

all:	HIDFirmwareUpdaterTool.s \
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

/Volumes/Aluminum\ Keyboard\ Firmware\ Update/AlKybdFirmwareUpdate.pkg:
	@echo "*************************"
	@echo "Please download AlKybdFirmwareUpdate.dmg from https://support.apple.com/kb/DL997?viewlocale=en_US&locale=en_US and open it (mount the image)."
	@echo "*************************"
	@exit 1

AlKybdFirmwareUpdate.pkg/Payload: /Volumes/Aluminum\ Keyboard\ Firmware\ Update/AlKybdFirmwareUpdate.pkg
	xar -xf "$<" $@
	touch $@

HIDFirmwareUpdaterTool: AlKybdFirmwareUpdate.pkg/Payload
	cpio -i -d -I $< ./Library/Application\ Support/Apple/HIDFirmwareUpdater/HIDFirmwareUpdaterTool
	cp ./Library/Application\ Support/Apple/HIDFirmwareUpdater/HIDFirmwareUpdaterTool .

kbd_0x0069_0x0220.irrxfw: AlKybdFirmwareUpdate.pkg/Payload
	cpio -i -d -I $< ./Library/Application\ Support/Apple/HIDFirmwareUpdater/Firmware/kbd_0x0069_0x0220.irrxfw
	cp ./Library/Application\ Support/Apple/HIDFirmwareUpdater/Firmware/kbd_0x0069_0x0220.irrxfw .

%.s: %
	otool -tV -arch i386 $< > $@ || (rm -f $@; false)

%.hex.s: HIDFirmwareUpdaterTool
	otool -tV -j $< > $@ || (rm -f $@; false)

HIDFirmwareUpdaterTool.xxd: HIDFirmwareUpdaterTool
	xxd $< $@

hacked_xxd_diff: HIDFirmwareUpdaterTool.hacked.xxd HIDFirmwareUpdaterTool.xxd
	diff HIDFirmwareUpdaterTool.xxd HIDFirmwareUpdaterTool.hacked.xxd > $@

HIDFirmwareUpdaterTool.hacked.tmp.xxd: HIDFirmwareUpdaterTool.hacked.xxd.diff HIDFirmwareUpdaterTool.xxd
	patch -o $@ HIDFirmwareUpdaterTool.xxd < $< || (rm -f $@; false)

HIDFirmwareUpdaterTool.hacked: HIDFirmwareUpdaterTool.hacked.tmp.xxd
	xxd -r $< $@
	chmod +x $@

cmp_tool:: HIDFirmwareUpdaterTool HIDFirmwareUpdaterTool.hacked HIDFirmwareUpdaterTool.s HIDFirmwareUpdaterTool.hacked.s
	-cmp -l HIDFirmwareUpdaterTool HIDFirmwareUpdaterTool.hacked
	-diff -u HIDFirmwareUpdaterTool.s HIDFirmwareUpdaterTool.hacked.s

%: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

Codec: Codec.cc

%.hex: %.irrxfw Codec
	./Codec < $< > $@ || (rm -f $@; false)

M8CDIS_DIR = Vendor/m8cdis
M8CDIS = Vendor/m8cdis/m8cdis

$(M8CDIS)::
	$(MAKE) -C $(M8CDIS_DIR)

%.asm: %.hex %.mp $(M8CDIS)
	-$(M8CDIS) -i $< -o $@ -b -e -s -u -p cy7c63923 || (rm -f $@; false)

debug_m8cdis::
	lldb $(M8CDIS) -- -i kbd_0x0069_0x0220.hex -o kbd_0x0069_0x0220.asm -b -e -s -u -p cy7c63923

FindKeys: FindKeys.cc USBKeys.inl

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

TOOL_DIR = /Library/Application\ Support/Apple/HIDFirmwareUpdater
FW_DIR = $(TOOL_DIR)/Firmware

$(TOOL_DIR):
	sudo mkdir -v -m 755 "$@"

$(FW_DIR): $(TOOL_DIR)
	sudo mkdir -v -m 755 "$@"

load_dvorak:: $(FW_DIR) dvorak.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/dvorak.irrxfw

load_dvorak_win:: $(FW_DIR) dvorak-win.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/dvorak-win.irrxfw

load_default:: $(FW_DIR) kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x220 ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

load_default_24f:: $(FW_DIR) kbd_0x0069_0x0220.irrxfw HIDFirmwareUpdaterTool.hacked
	sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

debug_load_default::
	lldb HIDFirmwareUpdaterTool.hacked -- -progress -pid 0x24f ../../../../..$(PWD)/kbd_0x0069_0x0220.irrxfw

watch_log:
	syslog -w 0 -k Sender HIDFirmwareUpdaterTool.hacked

clean::
	rm -f dvorak.irrxfw dvorak.keys dvorak.hex
	rm -f dvorak-win.irrxfw dvorak-win.keys dvorak-win.hex
	rm -f kbd_0x0069_0x0220.def.irrxfw kbd_0x0069_0x0220.def.hex kbd_0x0069_0x0220.def.keys
	rm -f kbd_0x0069_0x0220.irrxfw kbd_0x0069_0x0220.keys kbd_0x0069_0x0220.asm kbd_0x0069_0x0220.hex
	rm -f Patch FindKeys CheckSum Codec
	rm -f HIDFirmwareUpdaterTool.hacked.tmp.xxd HIDFirmwareUpdaterTool.hacked HIDFirmwareUpdaterTool.hex.s
	rm -fr Library
	rm -fr AlKybdFirmwareUpdate.pkg/
	rm -f HIDFirmwareUpdaterTool.xxd HIDFirmwareUpdaterTool

sys_clean:: clean
	sudo rm -rf /Library/Application\ Support/Apple/HIDFirmwareUpdater
