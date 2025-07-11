#include <iostream>
#include <fstream>
#include <unordered_map>

class USBKeys
{
public:
	static bool verbose;

	USBKeys()
	{
		std::ifstream hidKeysFile;
		static const std::string hidKeysPath = "Vendor/usb_hid_keys.h";
		hidKeysFile.open(hidKeysPath);
		if (!hidKeysFile)
			throw std::runtime_error(std::string("Could not open ") + hidKeysPath);

		while (hidKeysFile)
		{
			char pound;
			hidKeysFile >> pound;
			if (pound == '#')
			{
				std::string directive;
				hidKeysFile >> directive;
				if (directive != "define")
					continue;

				std::string keyName;
				hidKeysFile >> keyName;
				static const std::string identPrefix = "KEY_";
				if (keyName.compare(0, identPrefix.length(), identPrefix) != 0)
					continue;

				keyName.erase(0, identPrefix.length());

				std::string mid = keyName.substr(0, 4);
				if (mid == "MOD_" || mid == "ERR_")
					continue;

				static const std::string mediaPrefix = "MEDIA_";
				if (keyName.compare(0, mediaPrefix.length(), mediaPrefix) == 0)
					keyName.erase(0, mediaPrefix.length());

				if (keyName.length() > maxKeyNameLength)
					keyName.erase(maxKeyNameLength);

				u_int scanCode;
				hidKeysFile >> std::hex >> scanCode;
				AddKey(keyName, scanCode);
			}

			char restOfLine[1024];
			hidKeysFile.getline(restOfLine, std::size(restOfLine));
		}

		AddKey("EJECT", 0xfc, true);
		AddKey("FN", 0xf8, true);
	}

	static constexpr size_t maxKeyNameLength = 7;
	std::unordered_map<std::string, u_char> keyNames;
	std::unordered_map<u_char, std::string> keyCodes;

private:
	void AddKey(const std::string &keyName, u_char scanCode, bool overwrite = false)
	{
		if (keyCodes.find(scanCode) != keyCodes.end() && !overwrite)
		{
			std::cerr << "Already have a scancode for " << scanCode << ": " << keyCodes.at(scanCode) << std::endl;
			return;
		}

		keyCodes[scanCode] = keyName;

		//std::clog << keyName << scanCode << std::endl;
		if (keyNames.find(keyName) != keyNames.end() && !overwrite)
		{
			if (verbose)
				std::cerr << "Already have a name for " << keyName << ": " << std::hex << (u_int)keyNames.at(keyName) << std::endl;
			return;
		}

		keyNames[keyName] = scanCode;
	}
};

bool USBKeys::verbose = false;
