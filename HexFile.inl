#include <iostream>
#include <vector>
#include <variant>
#include <iomanip>

class HexReader
{
public:
	HexReader(std::istream &stream) : stream(stream), checkSum(0) { }

	template <typename T>
	HexReader &operator>>(T &item)
	{
		item = 0;
		for (u_int i = 0; i < sizeof(item); ++i)
		{
			for (u_int j = 0; j < 2; ++j)
			{
				u_char ch;
				stream >> ch;
				if (!isxdigit(ch))
					throw std::runtime_error(std::string("Expected hex digit, got ") + std::to_string(ch));

				u_char nybble = (ch <= '9') ? ch - '0' : toupper(ch) - '7';
				item = (item << 4) | nybble;
			}
			checkSum-= item & 0xff;
		}
		return *this;
	}

	std::istream &stream;
	int checkSum;
};

class HexWriter
{
public:
	HexWriter(std::ostream &stream) : stream(stream), checkSum(0)
	{
		stream << std::hex << std::noshowbase << std::setfill('0');
	}

	template <typename T>
	HexWriter &operator<<(const T &item)
	{
		int shift = (sizeof(item) - 1) << 3;
		do
		{
			u_char ch = (item >> shift) & 0xff;
			stream << std::setw(2) << (u_int)ch;
			checkSum-= ch;
			shift-= 8;
		}
		while (shift >= 0);

		return *this;
	}

	std::ostream &stream;
	int checkSum;
};

class HexFile
{
public:
	class Record
	{
	public:
		u_char &operator[](u_int address)
		{
			return std::get<std::vector<u_char>>(data)[address];
		}

		u_char length;
		u_int address;
		u_char type;
		std::variant<std::vector<u_char>, u_int> data;

		friend std::istream &operator>>(std::istream &is, HexFile::Record &record);
		friend std::ostream &operator<<(std::ostream &os, const HexFile::Record &record);
	};

	u_char &operator[](u_int address)
	{
		for (auto &record : records)
			if (record.address <= address && record.address + record.length > address)
				return record[address - record.address];

		throw std::out_of_range(std::string("No record at address ") + std::to_string(address));
	}

	std::vector<Record> records;

	friend std::istream &operator>>(std::istream &, HexFile &);
	friend std::istream &operator>>(std::istream &is, HexFile::Record &record);
	friend std::ostream &operator<<(std::ostream &os, const HexFile::Record &record);
};

std::istream &operator>>(std::istream &is, HexFile::Record &record)
{
	char colon;
	is >> colon;
	if (colon != ':')
		throw std::runtime_error(std::string("Expected : on input, got ") + colon + '(' + std::to_string(colon) + ')');

	HexReader hexReader{is};
	u_short shortAddr;
	hexReader >> record.length >> shortAddr >> record.type;
	record.address = shortAddr;

	switch (record.type)
	{
		case 0:
		{
			std::vector<u_char> &data = record.data.emplace<std::vector<u_char>>(record.length);
			for (auto &ch : data)
				hexReader >> ch;
			break;
		}

		case 1:
			break;

		case 4:
		{
			u_short segmentStart;
			hexReader >> segmentStart;
			record.data.emplace<u_int>(segmentStart << 8);
			break;
		}

		default:
			throw std::runtime_error("Unknown record type " + std::to_string(record.type));
	}

	u_char sumSoFar = hexReader.checkSum & 0xff;
	u_char checkByte;
	hexReader >> checkByte;
	if (checkByte != sumSoFar)
		throw std::runtime_error(std::string("Invalid check byte ") + std::to_string(checkByte) + " should be " + std::to_string(sumSoFar));

	return is;
}

std::ostream &operator<<(std::ostream &os, const HexFile::Record &record)
{
	os << ':';
	HexWriter hexWriter(os);
	hexWriter << record.length << (u_short)record.address << record.type;

	switch (record.type)
	{
		case 0:
			for (auto &ch : std::get<std::vector<u_char>>(record.data))
				hexWriter << ch;
			break;

		case 1:
			break;

		case 4:
			u_short shortAddress = std::get<u_int>(record.data) >> 8;
			hexWriter << shortAddress;
			break;
	}

	hexWriter << (u_char)(hexWriter.checkSum & 0xff);
	os << "\r\n";
	return os;
}

std::istream &operator>>(std::istream &is, HexFile &hexFile)
{
	static const bool verbose = false;

	u_int segmentStart = 0;
	bool done = false;
	do
	{
		HexFile::Record record;
		if (is >> record)
		{
			if (verbose)
				std::clog << "Record type " << (u_int)record.type << " at " << record.address << ", length " << (u_int)record.length << std::endl;

			if (record.type == 0)
			{
				record.address+= segmentStart;
				if (verbose)
					std::clog << "New segment start " << std::hex << std::setw(6) << segmentStart << std::endl;
			}
			else if (record.type == 4)
				segmentStart = record.address;
			else if (record.type == 1)
				done = true;
			hexFile.records.push_back(record);
		}
	}
	while (is && !done);

	if (!done)
		std::cerr << "Warning: Expected end record before EOF" << std::endl;

	return is;
}

std::ostream &operator<<(std::ostream &os, const HexFile &hexFile)
{
	for (auto &record : hexFile.records)
		os << record;

	return os;
}
