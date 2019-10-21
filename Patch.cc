#include "HexFile.inl"
#include "USBKeys.inl"

#include <iostream>
#include <fstream>
#include <sstream>
#include <charconv>

using namespace std;

static int
ByteSum(const string &s)
{
	int hexSum = 0;
	for (char ch : s)
		hexSum+= ch;

	return hexSum;
}

//static const u_int checkSumAddress = 0x1fc0;

static HexFile::Record &
FindCheckSumRecord(HexFile &input, u_int address)
{
	for (auto &record : input.records)
	{
		if (record.address == address)
			return record;
	}

	throw runtime_error("Missing CheckSum record at address " + to_string(address));
}

static bool
GenCheckSumRecord(HexFile::Record &record, size_t byteIndex, int targetCheckSum)
{
	for (u_int byteValue = 0; byteValue < 0x100; ++byteValue)
	{
		record[byteIndex] = byteValue;

		ostringstream oss;
		oss << record;

		auto byteSum = ByteSum(oss.str());
		if (byteSum == targetCheckSum)
			return true;

		if (byteIndex < record.length - 1)
		{
			if (GenCheckSumRecord(record, byteIndex + 1, targetCheckSum))
				return true;
		}
	}

	return false;
}

static void
FixCheckSum(HexFile &input, u_int address)
{
	int diff;
	{
		ostringstream oss;
		oss << input;
		auto checkSum = ByteSum(oss.str());
		diff = checkSum - 0x1057f8;
	}

	if (diff == 0)
		return;

	clog << "Fixing checksum (difference " << diff << ")" << endl;

	int targetCheckSum;
	HexFile::Record &record = FindCheckSumRecord(input, address);
	{
		ostringstream oss;
		oss << record;
		auto recordCheckSum = ByteSum(oss.str());
		targetCheckSum = recordCheckSum - diff;
	}

	for (u_char &ch : get<vector<u_char>>(record.data))
		ch = 0x0;

	if (!GenCheckSumRecord(record, record.length - 5, targetCheckSum))
		throw runtime_error("Could not fix checksum");
}

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
			else if (directive == "checksum")
				FixCheckSum(input, address);
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

	cout << input;
}
