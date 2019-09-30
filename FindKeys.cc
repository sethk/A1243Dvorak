#include "USBKeys.inl"

#include <iostream>
#include <fstream>
#include <unordered_map>
#include <iomanip>
#include <vector>

using namespace std;

template <typename T>
T hexRead(istream &is, int &checkSum)
{
	T item = 0;
	for (u_int i = 0; i < sizeof(item); ++i)
	{
		for (u_int j = 0; j < 2; ++j)
		{
			u_char ch;
			is >> ch;
			if (!isxdigit(ch))
				throw runtime_error("Expected hex digit, got "s + to_string(ch));

			u_char nybble = (ch <= '9') ? ch - '0' : toupper(ch) - '7';
			item = (item << 4) | nybble;
		}
		checkSum-= item & 0xff;
	}
	return item;
}

int
main(void)
{
	ios_base::sync_with_stdio(false);
	freopen(NULL, "rb", stdin);

	USBKeys keys;

	u_int segmentStart = 0;
	cin.exceptions(ifstream::failbit | ifstream::badbit);
	while (cin)
	{
		char colon;
		cin >> colon;
		if (colon != ':')
			throw runtime_error("Expected : on input, got "s + colon + '(' + to_string(colon) + ')');

		int checkSum = 0;
		u_char len = hexRead<u_char>(cin, checkSum);
		u_int addr = hexRead<u_short>(cin, checkSum);
		addr+= segmentStart;
		u_char recType = hexRead<u_char>(cin, checkSum);

		clog << "Record type " << (u_int)recType << " at " << addr << ", length " << (u_int)len << endl;

		switch (recType)
		{
			case 0x0:
				static const u_int startOffset = 4;
				static const u_int bytesPerLine = 8;
				for (auto i = 0; i < len; ++i)
				{
					u_char byte = hexRead<u_char>(cin, checkSum);
					if (((i - startOffset) % bytesPerLine) == 0)
						cout << endl << hex << setw(4) << setfill('0') << addr << ':';

					u_char scanCode = byte;
					char leftDelim = '(', rightDelim = ')';
					if (scanCode >= 0xe0)
					{
						leftDelim = '<';
						rightDelim = '>';
						scanCode-= 0x10;
					}

					auto keyName = keys.keyCodes.find(scanCode);

					cout << ' '
							<< leftDelim << setw(USBKeys::maxKeyNameLength) << setfill(' ')
							<< ((keyName != keys.keyCodes.end()) ? keyName->second : "???"s) << rightDelim
							<< hex << setw(2) << setfill('0') << (u_int)byte;

					++addr;
				}
				clog << endl;
				break;

			case 0x1:
				return 0;

			case 0x4:
				segmentStart = hexRead<u_short>(cin, checkSum) << 8;
				clog << "New segment start " << hex << setw(6) << segmentStart << endl;
				break;

			default:
				throw runtime_error("Unknown record type " + to_string(recType));
		}

		int dummy;
		u_char checkByte = hexRead<u_char>(cin, dummy);
		if (checkByte != (checkSum & 0xff))
			throw runtime_error("Invalid check byte "s + to_string(checkByte) + " should be " +
					to_string(checkSum & 0xff));
	}
}
