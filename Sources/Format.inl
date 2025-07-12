#include <sstream>

namespace Format
{
	template<typename T>
	std::string ToString(const T &Value)
	{
		std::ostringstream oss;
		oss << Value;
		return oss.str();
	}

	template<typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	struct Dec
	{
		Dec(T value, int minWidth = -1) : Value(value), MinWidth(minWidth) { }

		T Value;
		int MinWidth;
	};

	template<typename T> static inline std::ostream &operator <<(std::ostream &os, const Dec<T> &dec);

	// Avoid ambiguity with char
	template<>
	inline std::ostream &operator <<(std::ostream &os, const Dec<char> &dec)
	{
		return os << std::setw(dec.MinWidth) << std::setfill(' ') << std::dec << static_cast<int>(dec.Value);
	}

	// Avoid ambiguity with unsigned char
	template<>
	inline std::ostream &operator <<(std::ostream &os, const Dec<unsigned char> &dec)
	{
		return os << std::setw(dec.MinWidth) << std::setfill(' ') << std::dec << static_cast<unsigned>(dec.Value);
	}

	template<typename T>
	static inline std::ostream &operator <<(std::ostream &os, const Dec<T> &dec)
	{
		return os << std::setw(dec.MinWidth) << std::setfill(' ') << std::dec << dec.Value;
	}

	template<typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	struct Hex
	{
		Hex(T value, int minWidth = sizeof(T) * 2) : Value(value), MinWidth(minWidth) { }

		T Value;
		uint8_t MinWidth;
	};

	template<typename T> static inline std::ostream &operator <<(std::ostream &os, const Hex<T> &hex);

	// Avoid ambiguity with char
	template<>
	inline std::ostream &operator <<(std::ostream &os, const Hex<char> &hex)
	{
		return os << "0x" << std::setw(hex.MinWidth) << std::setfill('0') << std::hex << static_cast<int>(hex.Value) << std::dec;
	}

	// Avoid ambiguity with unsigned char
	template<>
	inline std::ostream &operator <<(std::ostream &os, const Hex<unsigned char> &hex)
	{
		return os << "0x" << std::setw(hex.MinWidth) << std::setfill('0') << std::hex << static_cast<unsigned>(hex.Value) << std::dec;
	}

	template<typename T>
	static inline std::ostream &operator <<(std::ostream &os, const Hex<T> &hex)
	{
		return os << "0x" << std::setw(hex.MinWidth) << std::setfill('0') << std::hex << hex.Value << std::dec;
	}

	template<typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	struct Oct
	{
		Oct(T value) : Value(value) { }

		T Value;
	};

	template<typename T>
	static inline std::ostream &operator <<(std::ostream &os, const Oct<T> &oct)
	{
		return os << '0' << std::oct << oct.Value << std::dec;
	}

	template<typename T, std::enable_if_t<std::is_integral<T>::value, bool> = true>
	struct Bin
	{
		Bin(T value) : Value(value) { }

		T Value;
	};

	template<typename T>
	static inline std::ostream &operator <<(std::ostream &os, const Bin<T> &bin)
	{
		int width = sizeof(bin.Value) * 8;
		os << "0b";
		for (int bitIndex = 0, msbIndex = width - 1; bitIndex < width; ++bitIndex, --msbIndex)
			os << ((bin.Value & (1u << msbIndex)) ? '1' : '0');
		return os;
	}

	struct HexDump
	{
		HexDump(const uint8_t *bytes, size_t len) : Bytes(bytes), Length(len) { }

		const uint8_t *Bytes;
		size_t Length;
	};

	static inline std::ostream &operator <<(std::ostream &os, const HexDump &dump)
	{
		size_t offset = 0;
		while (offset < dump.Length)
		{
			size_t lineWidth = std::min<size_t>(32, dump.Length - offset);

			for (size_t indexInLine = 0; indexInLine < 32; ++indexInLine)
			{
				size_t totalOffset = offset + indexInLine;

				if (totalOffset < dump.Length)
					os << ' ' << std::setw(2) << std::setfill('0') << std::hex << +dump.Bytes[totalOffset];
				else
					os << "   ";
			}
			
			os << std::dec << " |";

			for (size_t indexInLine = 0; indexInLine < 32; ++indexInLine)
			{
				size_t totalOffset = offset + indexInLine;

				if (totalOffset < dump.Length)
				{
					if (isprint(dump.Bytes[totalOffset]))
						os << dump.Bytes[totalOffset];
					else
						os << '.';
				}
				else
					os << ' ';
			}

			os << "\n";

			offset+= lineWidth;
		}

		return os;
	}

	struct CString
	{
		CString(const char *str, size_t size) : String(str), Size(size) { }
		template<size_t N> CString(const char (&bytes)[N]) : String(bytes), Size(N) { }
		const char * const String;
		const size_t Size;
	};

	static inline std::ostream &operator <<(std::ostream &os, const CString &cStr)
	{
		os << '"';
		for (size_t index = 0; index < cStr.Size; ++index)
		{
			unsigned char ch = cStr.String[index];
			if (ch == '\n')
				os << "\\n";
			else if (ch == '\t')
				os << "\\t";
			else if (ch == '\v')
				os << "\\v";
			else if (isprint(ch))
				os << ch;
			else
				os << '\\' << std::oct << static_cast<int>(ch);
		}
		os << '"';
		return os;
	}
};
