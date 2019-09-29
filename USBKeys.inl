#include <iostream>
#include <fstream>
#include <unordered_map>

class USBKeys
{
public:
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
				if (keyCodes.find(scanCode) != keyCodes.end())
				{
					std::cerr << "Already have a scancode for " << scanCode << ": " << keyCodes.at(scanCode) << std::endl;
					continue;
				}

				keyCodes[scanCode] = keyName;

				//std::clog << keyName << scanCode << std::endl;
				if (keyNames.find(keyName) != keyNames.end())
				{
					std::cerr << "Already have a name for " << keyName << ": " << std::hex << (u_int)keyNames.at(keyName) << std::endl;
					continue;
				}

				keyNames[keyName] = scanCode;
			}

			char restOfLine[1024];
			hidKeysFile.getline(restOfLine, std::size(restOfLine));
		}
	}

	static constexpr size_t maxKeyNameLength = 7;
	std::unordered_map<std::string, u_char> keyNames;
	std::unordered_map<u_char, std::string> keyCodes;
};
