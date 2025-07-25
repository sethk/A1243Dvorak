CXXFLAGS+= -Wall -Wextra -g -fno-inline -O0 -std=c++17

.SUFFIXES:

.PHONY: main
main: Firmware/dvorak.irrxfw Firmware/dvorak-win.irrxfw

ORIG_FW = kbd_0x0069_0x0220

all:	Disassembly/$(ORIG_FW).asm \
		Keymaps/$(ORIG_FW).keys \
		Tools/CheckSum \
		Firmware/$(ORIG_FW).def.irrxfw \
		Keymaps/$(ORIG_FW).def.keys \
		Firmware/dvorak.irrxfw \
		Keymaps/dvorak.keys \
		Firmware/dvorak-win.irrxfw \
		Keymaps/dvorak-win.keys

obsolete:	Obsolete/HIDFirmwareUpdaterTool.s \
			Obsolete/HIDFirmwareUpdaterTool.hex.s \
			Obsolete/HIDFirmwareUpdaterTool.hacked \

/Volumes/Aluminum\ Keyboard\ Firmware\ Update/AlKybdFirmwareUpdate.pkg:
	@echo "*************************"
	@echo "Please download AlKybdFirmwareUpdate.dmg from https://support.apple.com/kb/DL997?viewlocale=en_US&locale=en_US and open it (mount the image)."
	@echo "*************************"
	@exit 1

Packages/AlKybdFirmwareUpdate.pkg/Payload: /Volumes/Aluminum\ Keyboard\ Firmware\ Update/AlKybdFirmwareUpdate.pkg
	(cd Packages && xar -xf "$<" AlKybdFirmwareUpdate.pkg/Payload)
	touch $@

Obsolete/HIDFirmwareUpdaterTool: Packages/AlKybdFirmwareUpdate.pkg/Payload
	cpio -i -d -I $< ./Library/Application\ Support/Apple/HIDFirmwareUpdater/HIDFirmwareUpdaterTool
	cp ./Library/Application\ Support/Apple/HIDFirmwareUpdater/HIDFirmwareUpdaterTool Obsolete/

Firmware/$(ORIG_FW).irrxfw: Packages/AlKybdFirmwareUpdate.pkg/Payload
	cpio -i -d -I $< ./Library/Application\ Support/Apple/HIDFirmwareUpdater/Firmware/$(ORIG_FW).irrxfw
	cp ./Library/Application\ Support/Apple/HIDFirmwareUpdater/Firmware/$(ORIG_FW).irrxfw Firmware/

%.s: %
	otool -tV -d -r -I -G -function_offsets -arch i386 $< > $@ || (rm -f $@; false)

%.hex.s: Obsolete/HIDFirmwareUpdaterTool
	otool -tV -j $< > $@ || (rm -f $@; false)

Obsolete/HIDFirmwareUpdaterTool.xxd: Obsolete/HIDFirmwareUpdaterTool
	xxd $< $@

Obsolete/hacked_xxd_diff: Obsolete/HIDFirmwareUpdaterTool.hacked.xxd Obsolete/HIDFirmwareUpdaterTool.xxd
	diff Obsolete/HIDFirmwareUpdaterTool.xxd Obsolete/HIDFirmwareUpdaterTool.hacked.xxd > $@

Obsolete/HIDFirmwareUpdaterTool.hacked.tmp.xxd: Obsolete/HIDFirmwareUpdaterTool.hacked.xxd.diff Obsolete/HIDFirmwareUpdaterTool.xxd
	patch -o $@ Obsolete/HIDFirmwareUpdaterTool.xxd < $< || (rm -f $@; false)

Obsolete/HIDFirmwareUpdaterTool.hacked: Obsolete/HIDFirmwareUpdaterTool.hacked.tmp.xxd
	xxd -r $< $@
	chmod +x $@

cmp_tool:: Obsolete/HIDFirmwareUpdaterTool Obsolete/HIDFirmwareUpdaterTool.hacked Obsolete/HIDFirmwareUpdaterTool.s Obsolete/HIDFirmwareUpdaterTool.hacked.s
	-cmp -l Obsolete/HIDFirmwareUpdaterTool Obsolete/HIDFirmwareUpdaterTool.hacked
	-diff -u Obsolete/HIDFirmwareUpdaterTool.s Obsolete/HIDFirmwareUpdaterTool.hacked.s

Tools/%: Sources/%.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

%.hex: %.irrxfw Tools/Codec
	Tools/Codec < $< > $@ || (rm -f $@; false)

M8CDIS_DIR = Vendor/m8cdis
M8CDIS = Vendor/m8cdis/m8cdis
M8CDIS_FLAGS = -a -b -e -s -u -p cy7c63923

$(M8CDIS)::
	$(MAKE) -C $(M8CDIS_DIR)

Disassembly/%.asm: Firmware/%.hex Disassembly/%.mp Disassembly/%.hint $(M8CDIS)
	-$(M8CDIS) -i $< -o $@ $(M8CDIS_FLAGS) || (echo exit status $$?; rm -f $@; false)

debug_m8cdis::
	lldb $(M8CDIS) -- -i Firmware/$(ORIG_FW).hex -o Disassembly/$(ORIG_FW).asm $(M8CDIS_FLAGS)

redisasm:: clean_disasm Disassembly/$(ORIG_FW).asm

Tools/FindKeys: Sources/FindKeys.cc Sources/USBKeys.inl

Keymaps/%.keys: Firmware/%.hex Tools/FindKeys
	Tools/FindKeys < $< > $@ || (rm -f $@; false)

Tools/Patch: Sources/Patch.cc Sources/USBKeys.inl Sources/HexFile.inl Firmware/$(ORIG_FW).hex
	$(CXX) $(CXXFLAGS) -o $@ $<
	Tools/Patch /dev/null < Firmware/$(ORIG_FW).hex | diff - Firmware/$(ORIG_FW).hex || (rm -f $@; false)

Firmware/$(ORIG_FW).def.hex: Firmware/$(ORIG_FW).hex Tools/Patch Keymaps/default.patch
	Tools/Patch Keymaps/default.patch < Firmware/$(ORIG_FW).hex > Firmware/$(ORIG_FW).def.hex || (rm -f $@; false)
	diff Firmware/$(ORIG_FW).def.hex Firmware/$(ORIG_FW).hex || (rm -f $@; false)

Firmware/dvorak.hex: Tools/Patch Keymaps/dvorak.patch Firmware/$(ORIG_FW).hex
	Tools/Patch Keymaps/dvorak.patch < Firmware/$(ORIG_FW).hex > $@ || (rm -f $@; false)

Firmware/dvorak-win.hex: Tools/Patch Keymaps/dvorak-win.patch Firmware/$(ORIG_FW).hex
	Tools/Patch Keymaps/dvorak-win.patch < Firmware/$(ORIG_FW).hex > $@ || (rm -f $@; false)

Firmware/$(ORIG_FW).def.irrxfw: Tools/Codec Firmware/$(ORIG_FW).def.hex
	Tools/Codec < Firmware/$(ORIG_FW).def.hex > $@ || (rm -f $@; false)
	cmp -b $@ Firmware/$(ORIG_FW).irrxfw || (rm -f $@; false)

Firmware/dvorak.irrxfw: Tools/Codec Firmware/dvorak.hex
	Tools/Codec < Firmware/dvorak.hex > $@ || (rm -f $@; false)

Firmware/dvorak-win.irrxfw: Tools/Codec Firmware/dvorak-win.hex
	Tools/Codec < Firmware/dvorak-win.hex > $@ || (rm -f $@; false)

TOOL_DIR = /Library/Application\ Support/Apple/HIDFirmwareUpdater
FW_DIR = $(TOOL_DIR)/Firmware

$(TOOL_DIR):
	sudo mkdir -v -m 755 "$@"

$(FW_DIR): $(TOOL_DIR)
	sudo mkdir -v -m 755 "$@"

Tools/Upload: Sources/Upload.cc Sources/HexFile.inl Sources/Format.inl Sources/SyscallError.inl
	$(CXX) $(CXXFLAGS) $(shell pkg-config --cflags libusb-1.0) -o $@ $< $(shell pkg-config --libs libusb-1.0)

load_dvorak:: Tools/Upload Firmware/dvorak.hex
	Tools/Upload Firmware/dvorak.hex

#load_dvorak_24f:: $(FW_DIR) dvorak.irrxfw HIDFirmwareUpdaterTool.hacked
#sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x24f ../../../../..$(PWD)/dvorak.irrxfw

load_dvorak_win:: Tools/Upload Firmware/dvorak-win.hex
	Tools/Upload Firmware/dvorak-win.hex

load_default:: Tools/Upload Firmware/$(ORIG_FW).hex
	Tools/Upload Firmware/$(ORIG_FW).hex

#load_default_24f:: $(FW_DIR) $(ORIG_FW).irrxfw HIDFirmwareUpdaterTool.hacked
#sudo ./HIDFirmwareUpdaterTool.hacked -progress -pid 0x24f ../../../../..$(PWD)/$(ORIG_FW).irrxfw

clean_disasm::
	rm -f Disassembly/$(ORIG_FW).asm

clean::
	rm -f Firmware/dvorak.irrxfw Keymaps/dvorak.keys Firmware/dvorak.hex
	rm -f Firmware/dvorak-win.irrxfw Keymaps/dvorak-win.keys Firmware/dvorak-win.hex
	rm -f Firmware/$(ORIG_FW).def.irrxfw Firmware/$(ORIG_FW).def.hex Keymaps/$(ORIG_FW).def.keys
	rm -f Firmware/$(ORIG_FW).irrxfw Keymaps/$(ORIG_FW).keys Firmware/$(ORIG_FW).hex
	rm -f Tools/Patch Tools/FindKeys Tools/CheckSum Tools/Codec Tools/Upload
	rm -f Obsolete/HIDFirmwareUpdaterTool.hacked.tmp.xxd Obsolete/HIDFirmwareUpdaterTool.hacked Obsolete/HIDFirmwareUpdaterTool.s
	rm -fr Library
	rm -fr Packages/AlKybdFirmwareUpdate.pkg/
	rm -f Obsolete/HIDFirmwareUpdaterTool.xxd Obsolete/HIDFirmwareUpdaterTool

sys_clean:: clean
	sudo rm -rf /Library/Application\ Support/Apple/HIDFirmwareUpdater
