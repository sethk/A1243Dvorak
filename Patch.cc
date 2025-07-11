#include "HexFile.inl"
#include "USBKeys.inl"

#include <iostream>
#include <fstream>
#include <sstream>
#include <charconv>

using namespace std;

int
main(int ac, char *av[])
{
	bool verbose = false;

	ios_base::sync_with_stdio(false);
	freopen(NULL, "rb", stdin);

	if (ac != 2)
	{
		cerr << "usage: " << getprogname() << " <file.patch>" << endl;
		return 64; // EX_USAGE
	}

	HexFile input;
	if (!(cin >> input))
		throw runtime_error("Could not load HexFile from input");

	ifstream patchStream(av[1]);
	if (!patchStream.is_open())
		throw runtime_error("Could not open "s + av[1]);

	USBKeys keys;

	string line;
	u_int lineNum = 0;
	u_int numErrors = 0;
	while (getline(patchStream, line))
	{
		++lineNum;
		line.erase(find(line.begin(), line.end(), ';'), line.end());
		istringstream lineStream(line);

		try
		{
			u_int address;
			char colon;
			if (!(lineStream >> hex >> noshowbase >> address >> colon) || colon != ':')
			{
				if (lineStream.eof())
					continue;

				throw runtime_error("Expected address:");
			}

			string directive;
			if (!(lineStream >> directive))
				throw runtime_error("Expected directive");

			if (directive == "keys")
			{
				string keyName;
				while ((lineStream >> keyName))
				{
					for (auto &ch : keyName)
						ch = toupper(ch);

					u_char scanCode;
					auto scanKey = keys.keyNames.find(keyName);
					if (scanKey != keys.keyNames.end())
					{
						scanCode = scanKey->second;
						if (scanCode >= 0xe0 && scanCode < 0xe8)
							scanCode+= 0x10;
					}
					else
					{
						try
						{
							size_t pos;
							scanCode = stoul(keyName, &pos, 16);
							if (pos != keyName.size())
								throw invalid_argument("Unparsed scan code: " + keyName.substr(pos));
						}
						catch (invalid_argument &err)
						{
							lineStream.clear();
							throw runtime_error("Unknown key code " + keyName);
							++numErrors;
							continue;
						}
					}

					if (verbose)
						clog << hex << address << ": " << keyName << endl;

					input[address] = scanCode;

					++address;
				}

				if (!lineStream.eof())
					throw runtime_error("Expected key name or hex code");
			}
			else if (directive == "db")
			{
				u_int uintChar;
				while ((lineStream >> hex >> uintChar))
				{
					if (uintChar > 0xff)
						clog << "Byte literal " << uintChar << " too large" << endl;

					input[address] = uintChar;

					++address;
				}

				if (!lineStream.eof())
					throw runtime_error("Expected hex byte");
			}
			else
				throw runtime_error("Unrecognized directive "s + directive);
		}
		catch (runtime_error &err)
		{
			cerr << av[1] << ':' << dec << lineNum << ':' << lineStream.tellg() << ": error: " <<  err.what() << endl;
			++numErrors;
		}
	}

	if (numErrors > 0)
		return 1;

	input.UpdateLowSum();

	cout << input;
}
