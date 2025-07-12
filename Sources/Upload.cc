#include <libusb.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <unistd.h>
#include "HexFile.inl"
#include "Format.inl"
#include "SyscallError.inl"

#if LIBUSB_API_VERSION < 0x0100010A
#	error "Libusb version 1.0.27 or later required"
#endif

using std::string;
using std::ios_base;
using std::ostream;
using std::ifstream;
using std::clog;
using std::cerr;
using std::endl;
using std::flush;
using std::to_string;
using std::runtime_error;
using std::hex;
using std::dec;
using std::array;
using std::vector;
using std::unique_ptr;
using std::strtoul;
using std::strlen;

uint_fast8_t verbosity = 0;

enum class LoaderCommand : uint8_t
{
	Enter  = 0x38,
	Write  = 0x39,
	Verify = 0x3a,
	Exit   = 0x3b,
};

enum StatusFlag : uint8_t
{
	Success =       0x01,
	BadLowSum =     0x02, // Checksum of 0x80 to 0x1300 invalid
	VerifyFailed =  0x04,
	Protected =     0x08,
	BadCheckSum =   0x10,
	ReadyToWrite =  0x20,
	BadHeader =     0x40, // Bad magic number 0xff or ordinal numbers
	BadCommand =    0x80,
};

struct alignas(1) LoaderMessage
{
	LoaderMessage() = default;

	// TODO payload view
	LoaderMessage(LoaderCommand command, uint8_t blockNum = 0, uint8_t secondHalf = 0, const uint8_t *payload = nullptr, size_t payloadLength = 0)
		: Magic(0xff)
		, Command(command)
		, Padding1(0)
		, BlockNum(blockNum)
		, SecondHalf(secondHalf)
	{
		for (uint_fast8_t i = 0; i < 8; ++i)
			Ordinal[i] = i;

		std::fill(Payload.begin(), Payload.end(), 0);

		if (payload)
			SetPayload(payload, payloadLength);
		else
			updateCheckSum();

		std::fill(Padding2.begin(), Padding2.end(), 0);
	}

	uint8_t GetBlockNum() const { return BlockNum; }

	// TODO: payload view
	void SetPayload(const uint8_t *payload, size_t payloadLength)
	{
		assert(payloadLength <= 32);

		std::copy(payload, payload + payloadLength, Payload.begin());
		updateCheckSum();
	}

	const uint8_t *AsBytes() const { return reinterpret_cast<const uint8_t *>(this); }

private:
	void updateCheckSum()
	{
		const uint8_t * const begin = AsBytes();
		const uint8_t * const end = &CheckSum;

		CheckSum = 0;
		for (const uint8_t *byte = begin; byte < end; ++byte)
			CheckSum+= *byte;
	}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-private-field"
	uint8_t Magic;			// always 0xff
	LoaderCommand Command;
	uint8_t Ordinal[8];		// always 0x00-07
	uint8_t Padding1;
	uint8_t BlockNum;
	uint8_t SecondHalf;		// 0 or 1 for first or second 32 bytes
	array<uint8_t, 32> Payload;
	uint8_t CheckSum;		// offset 45
	array<uint8_t, 18> Padding2;
#pragma clang diagnostic pop
};
static_assert(sizeof(LoaderMessage) <= 64);
static_assert(sizeof(LoaderMessage) >= 64);

static void verifyLibUSB(const string &desc, int usb_error)
{
	if (usb_error < 0)
		throw runtime_error(string("Could not ") + desc + ": " + libusb_strerror(usb_error));
}

static ostream &operator <<(ostream &os, const libusb_endpoint_descriptor &desc)
{
	os << "      Endpoint:\n";
	os << "        bEndpointAddress:    " << Format::Hex(desc.bEndpointAddress) << '\n';
	os << "        bmAttributes:        " << Format::Hex(desc.bmAttributes) << '\n';
	os << "        wMaxPacketSize:      " << Format::Dec(desc.wMaxPacketSize) << '\n';
	os << "        bInterval:           " << Format::Dec(desc.bInterval) << '\n';
	os << "        bRefresh:            " << Format::Dec(desc.bRefresh) << '\n';
	os << "        bSynchAddress:       " << Format::Dec(desc.bSynchAddress) << '\n';
	return os;
}

static ostream &operator <<(ostream &os, const libusb_interface_descriptor &iface)
{
	os << "    Interface:\n";
	os << "      bInterfaceNumber:      " << Format::Hex(iface.bInterfaceNumber) << '\n';
	os << "      bAlternateSetting:     " << Format::Hex(iface.bAlternateSetting) << '\n';
	os << "      bNumEndpoints:         " << Format::Hex(iface.bNumEndpoints) << '\n';
	os << "      bInterfaceClass:       " << Format::Hex(iface.bInterfaceClass) << '\n';
	os << "      bInterfaceSubClass:    " << Format::Hex(iface.bInterfaceSubClass) << '\n';
	os << "      bInterfaceProtocol:    " << Format::Hex(iface.bInterfaceProtocol) << '\n';
	os << "      iInterface:            " << Format::Dec(iface.iInterface) << '\n';

	for (uint_fast8_t i = 0; i < iface.bNumEndpoints; i++)
		os << iface.endpoint[i];

	return os;
}

static ostream &operator <<(ostream &os, const libusb_usb_2_0_extension_descriptor &usb_2_0_ext_cap)
{
	os << "    USB 2.0 Extension Capabilities:\n";
	os << "      bDevCapabilityType:    " << Format::Dec(usb_2_0_ext_cap.bDevCapabilityType) << '\n';
	os << "      bmAttributes:          " << Format::Hex(usb_2_0_ext_cap.bmAttributes) << '\n';
	return os;
}

static ostream &operator <<(ostream &os, const libusb_bos_descriptor &bos)
{
	os << "  Binary Object Store (BOS):\n";
	os << "    wTotalLength:            " << Format::Dec(bos.wTotalLength) << '\n';
	os << "    bNumDeviceCaps:          " << Format::Dec(bos.bNumDeviceCaps) << '\n';

	for (uint_fast8_t capIndex = 0; capIndex < bos.bNumDeviceCaps; capIndex++)
	{
		libusb_bos_dev_capability_descriptor *cap = bos.dev_capability[capIndex];

		if (cap->bDevCapabilityType == LIBUSB_BT_USB_2_0_EXTENSION)
		{
			libusb_usb_2_0_extension_descriptor *usb20ExtPtr;

			verifyLibUSB("get USB 2.0 extension descriptor", libusb_get_usb_2_0_extension_descriptor(nullptr, cap, &usb20ExtPtr));
			unique_ptr<libusb_usb_2_0_extension_descriptor, decltype(&libusb_free_usb_2_0_extension_descriptor)> usb20Ext(usb20ExtPtr, &libusb_free_usb_2_0_extension_descriptor);

			clog << *usb20Ext.get();
		}
#if false
		else if (cap->bDevCapabilityType == LIBUSB_BT_SS_USB_DEVICE_CAPABILITY) {
			struct libusb_ss_usb_device_capability_descriptor *ss_dev_cap;

			ret = libusb_get_ss_usb_device_capability_descriptor(NULL, dev_cap, &ss_dev_cap);
			if (ret < 0)
				return;

			throw "Error";
			//print_ss_usb_cap(ss_dev_cap);
			libusb_free_ss_usb_device_capability_descriptor(ss_dev_cap);
		}
#endif // false
	}
	return os;
}

static ostream &operator <<(ostream &os, const libusb_interface &iface)
{
	for (int altIndex = 0; altIndex < iface.num_altsetting; altIndex++)
		os << iface.altsetting[altIndex];

	return os;
}

static ostream &operator <<(ostream &os, const libusb_config_descriptor &config)
{
	os << "  Configuration:\n";
	os << "    wTotalLength:            " << Format::Dec(config.wTotalLength) << '\n';
	os << "    bNumInterfaces:          " << Format::Dec(config.bNumInterfaces) << '\n';
	os << "    bConfigurationValue:     " << Format::Dec(config.bConfigurationValue) << '\n';
	os << "    iConfiguration:          " << Format::Dec(config.iConfiguration) << '\n';
	os << "    bmAttributes:            " << Format::Hex(config.bmAttributes) << '\n';
	os << "    MaxPower:                " << Format::Dec(config.MaxPower) << '\n';

	for (uint_fast8_t ifIndex = 0; ifIndex < config.bNumInterfaces; ifIndex++)
		os << config.interface[ifIndex];

	return os;
}

static void getDeviceDescriptor(libusb_device &device, libusb_device_descriptor &desc)
{
	verifyLibUSB("get device descriptor", libusb_get_device_descriptor(&device, &desc));
}

static ostream &operator <<(ostream &os, libusb_device &device)
{
	os << "Dev (bus " << Format::Dec(libusb_get_bus_number(&device), 2);
	os << " address " << Format::Dec(libusb_get_device_address(&device), 2) << "): ";

	libusb_device_descriptor desc;
	getDeviceDescriptor(device, desc);
	os << Format::Hex(desc.idVendor) << " - " << Format::Hex(desc.idProduct);

	os << " speed: ";
	switch (libusb_get_device_speed(&device))
	{
		case LIBUSB_SPEED_LOW: os << "1.5 Mbps"; break;
		case LIBUSB_SPEED_FULL: os << "12 Mbps"; break;
		case LIBUSB_SPEED_HIGH: os << "480 Mbps"; break;
		case LIBUSB_SPEED_SUPER: os << "5 Gbps"; break;
		case LIBUSB_SPEED_SUPER_PLUS: os << "10 Gbps"; break;
#if notyet
		case LIBUSB_SPEED_SUPER_PLUS_X2:os << "20 Gbps"; break;
#endif
		default: os << "Unknown";
	}
	return os;
}

static ostream &operator <<(ostream &os, libusb_device_handle &handle)
{
	libusb_device *device = libusb_get_device(&handle);
	libusb_device_descriptor desc;
	getDeviceDescriptor(*device, desc);

	unsigned char string[256];
	if (desc.iManufacturer)
	{
		verifyLibUSB("Convert manufacturer string", libusb_get_string_descriptor_ascii(&handle, desc.iManufacturer, string, sizeof(string)));
		os << "  Manufacturer:              " << string << '\n';
	}

	if (desc.iProduct)
	{
		verifyLibUSB("Convert product string", libusb_get_string_descriptor_ascii(&handle, desc.iProduct, string, sizeof(string)));
		os << "  Product:                   " << string << '\n';
	}

	if (desc.iSerialNumber && verbosity)
	{
		verifyLibUSB("Convert serial number string", libusb_get_string_descriptor_ascii(&handle, desc.iSerialNumber, string, sizeof(string)));
		os << "  Serial Number:             " << string << '\n';
	}

	if (verbosity > 1)
	{
		for (uint_fast8_t configIndex = 0; configIndex < desc.bNumConfigurations; configIndex++)
		{
			struct libusb_config_descriptor *configPtr;

			verifyLibUSB("get config descriptor", libusb_get_config_descriptor(device, configIndex, &configPtr));
			unique_ptr<libusb_config_descriptor, decltype(&libusb_free_config_descriptor)> config(configPtr, libusb_free_config_descriptor);

			os << *config.get();
		}

		if (desc.bcdUSB >= 0x0201)
		{
			libusb_bos_descriptor *bosPtr;
			verifyLibUSB("get binary object store", libusb_get_bos_descriptor(&handle, &bosPtr));
			unique_ptr<libusb_bos_descriptor, decltype(&libusb_free_bos_descriptor)> bos(bosPtr, &libusb_free_bos_descriptor);

			os << *bos.get();
		}
	}

	return os;
}

static void sendBootModeRequest(libusb_device_handle &handle, uint8_t mode)
{
	verifyLibUSB(
			"send boot mode request",
			libusb_control_transfer(
				&handle,
				LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
				LIBUSB_REQUEST_SET_CONFIGURATION,
				0x0300 | mode,
				0x0000,
				&mode,
				sizeof(mode),
				5000
			)
	);
}

static void claimInterface(libusb_device_handle &handle, int ifIndex)
{
	verifyLibUSB("enable auto detach of kernel driver", libusb_set_auto_detach_kernel_driver(&handle, 1));
	verifyLibUSB(string("claim interface ") + Format::ToString(ifIndex), libusb_claim_interface(&handle, ifIndex));
}

static void setBootMode(libusb_device_handle &handle, bool loaderMode, int ifIndex)
{
	clog << "\nPutting device into " << (loaderMode ? "bootloader" : "keyboard") << " mode...\n" << '\n';

	sendBootModeRequest(handle, loaderMode ? 0x0a : 0xb);

	verifyLibUSB("release interface", libusb_release_interface(&handle, ifIndex));
}

struct DeviceNotFound : runtime_error
{
	DeviceNotFound(const string& message) : runtime_error(message) { }
};

template<unsigned NumProductIDs>
static libusb_device *
findDevice(libusb_device **devices, size_t numDevices,
		int busNumber, int deviceAddress,
		uint16_t vendorId, const uint16_t (&productIDs)[NumProductIDs])
{
	for (size_t deviceIndex = 0; deviceIndex < numDevices; ++deviceIndex)
	{
		libusb_device *device = devices[deviceIndex];
		libusb_device_descriptor desc;
		getDeviceDescriptor(*device, desc);

		if (busNumber != -1)
		{
			if (libusb_get_bus_number(device) == busNumber &&
					libusb_get_device_address(device) == deviceAddress)
			{
				if (desc.idVendor != vendorId)
					cerr << "Warning: idVendor " << Format::Hex(desc.idVendor) << " does not match expected value " << Format::Hex(vendorId) << '\n';
				auto productIt = std::find(productIDs, productIDs + NumProductIDs, desc.idProduct);
				if (productIt == productIDs + NumProductIDs)
				{
					cerr << "Warning: idProduct " << Format::Hex(desc.idProduct) << " does not match any expected value:";
					for (unsigned productIndex = 0; productIndex < NumProductIDs; ++productIndex)
						cerr << ' ' << Format::Hex(productIDs[productIndex]);
					cerr << '\n';
				}

				return device;
			}
		}
		else if (desc.idVendor == vendorId && std::find(productIDs, productIDs + NumProductIDs, desc.idProduct) != productIDs + NumProductIDs)
			return device;
	}

	if (busNumber != -1)
		throw DeviceNotFound(string("Could not find device at bus ") + Format::ToString(busNumber) + " and device address " + Format::ToString(deviceAddress));
	else
	{
		string error = string("Could not find device with idVendor ") + Format::ToString(Format::Hex(vendorId));
		error+= " and idProduct in {";
		for (unsigned productIndex = 0; productIndex < NumProductIDs; ++productIndex)
			error+= ' ' + Format::ToString(Format::Hex(productIDs[productIndex]));
		error+= " }";
		throw DeviceNotFound(error);
	}

	return nullptr;
}

using DeviceHandle = unique_ptr<libusb_device_handle, decltype(&libusb_close)>;

DeviceHandle
openDevice(libusb_device &device)
{
	clog << "Opening " << device << '\n';

	libusb_device_handle *handlePtr;
	verifyLibUSB("open device", libusb_open(&device, &handlePtr));
	DeviceHandle handle(handlePtr, &libusb_close);

	clog << *handle << '\n';

	return handle;
}

static uint8_t recvStatus(libusb_device_handle &handle, uint8_t blockNum)
{
	if (verbosity)
		clog << "Receiving bulk loader message from device" << endl;

	uint8_t response[64];
	int respLength;
	verifyLibUSB(
			"receive bulk tranfer response from device",
			libusb_bulk_transfer(
				&handle,
				LIBUSB_ENDPOINT_IN | 1,
				response,
				sizeof(response),
				&respLength,
				8000
			)
	);

	if (verbosity > 0)
	{
		clog << "Received " << Format::Dec(respLength) << " bytes of data from device:\n";
		clog << Format::HexDump(response, respLength) << '\n';
	}

	uint8_t status = response[0];
	if (verbosity)
		clog << "Response status " << Format::Hex(status) << '/' << Format::Bin(status) << endl;

	if (status & StatusFlag::BadLowSum)
		throw runtime_error("Invalid checksum for ROM range 0x80 - 0x1300");
	if (status & StatusFlag::VerifyFailed)
		throw runtime_error(string("Block verification failed for block ") + Format::ToString(Format::Dec(blockNum)));
	if (status & StatusFlag::Protected)
		throw runtime_error("Protected flash block error");
	if (status & StatusFlag::BadCheckSum)
		throw runtime_error("Invalid block checksum");
	if (status & StatusFlag::BadHeader)
		throw runtime_error("Invalid command header");
	if (status & StatusFlag::BadCommand)
		throw runtime_error("Invalid command");

	if (!(status & StatusFlag::ReadyToWrite))
		throw runtime_error("Device not in ready state");

	return status;
}

static uint8_t sendMessage(libusb_device_handle &handle, const LoaderMessage &message)
{
	uint8_t *bytes = reinterpret_cast<uint8_t *>(const_cast<LoaderMessage *>(&message));
	const size_t length = sizeof(message);

	if (verbosity)
	{
		clog << "Sending bulk loader message to device: " << endl;
		clog << Format::HexDump(bytes, length) << endl;
	}

	int sentLength;
	verifyLibUSB(
			"send bulk transfer loader message to device",
			libusb_bulk_transfer(
				&handle,
				LIBUSB_ENDPOINT_OUT | 2,
				bytes,
				length,
				&sentLength,
				8000
			)
	);

	if (static_cast<size_t>(sentLength) < length)
		throw runtime_error(string("Only sent ") + std::to_string(sentLength) + " bytes of bulk data to device");

	return recvStatus(handle, message.GetBlockNum());
}

static void startUpdate(libusb_device_handle &handle)
{
	LoaderMessage message(LoaderCommand::Enter);
	sendMessage(handle, message);
}

static void finishUpdate(libusb_device_handle &handle)
{
	LoaderMessage message(LoaderCommand::Exit);
	uint8_t status = sendMessage(handle, message);

	if (!(status & StatusFlag::Success))
		throw runtime_error("Final verification failed");
}

static void writeOrVerifyBlock(libusb_device_handle &handle, const HexFile::Record &record, bool shouldWrite)
{
	uint8_t blockNum = (record.address / 64);

	//if (blockNum != 127)
		//return;
	//if (blockNum == 47 || blockNum == 48 || blockNum == 49 || blockNum == 50 || blockNum == 75 || blockNum == 76)
//		return;
	if (blockNum == 76 || blockNum == 78 || blockNum == 127)
		return;

	if (verbosity)
		clog << (shouldWrite ? "Writing" : "Verifying") << " block #" << Format::Dec(blockNum) << " at " << Format::Hex(record.address) << endl;
	else
		clog << '.' << flush;

	for (int half = 0; half < 2; ++half)
	{
		LoaderMessage message(shouldWrite ? LoaderCommand::Write : LoaderCommand::Verify, blockNum, half);
		message.SetPayload(std::get<vector<u_char>>(record.data).data() + half * 32, 32);
		sendMessage(handle, message);
	}
}

struct DeviceListDeleter
{
	void operator ()(libusb_device **devices)
	{
		libusb_free_device_list(devices, 1);
	}
};

using DeviceListPtr = unique_ptr<libusb_device *, DeviceListDeleter>;

DeviceListPtr getDeviceList(libusb_context &context, size_t &outNumDevices)
{
	libusb_device **devicesPtr;
	ssize_t numDevices = libusb_get_device_list(&context, &devicesPtr);
	verifyLibUSB("list attached devices", numDevices);
	outNumDevices = numDevices;

	std::sort(
			devicesPtr, devicesPtr + numDevices,
			[](libusb_device *lhs, libusb_device *rhs)
			{
				if (libusb_get_bus_number(lhs) < libusb_get_bus_number(rhs))
					return true;
				if (libusb_get_device_address(lhs) < libusb_get_device_address(rhs))
					return true;
				return false;
			}
		);

	return DeviceListPtr(devicesPtr, DeviceListDeleter());
}

static void listDevices(libusb_device **devices, size_t numDevices)
{
	for (size_t deviceIndex = 0; deviceIndex < numDevices; ++deviceIndex)
	{
		libusb_device *device = devices[deviceIndex];
		clog << *device << '\n';

		if (verbosity)
		{
			DeviceHandle handle = openDevice(*device);
			clog << *handle.get() << '\n';
		}
	}
}

// for each record
// recordBegin
// if the endAddr is less or equal to beginSummed
// 	 skip it
// else if the beginAddr is greater than or equal to endSummed
//   skip it
// else
//   if endAddr is greater than endSummed
//   	endOffset = summedEnd - beginAddr
//   else
//   	endOffset = recordLength
//   endif
//   if beginAddr is less than beginSummed
//   	beginOffset = beginSummed - beginAddr
//   else
//   	beginOffset = 0
//   endif
// endif

static void readHexFile(const char *fileName, HexFile &hexFile, bool ignoreCheckSum)
{
	ifstream hexStream(fileName);
	if (!hexStream.is_open())
		throw SyscallError(string("Could not open hex file ") + fileName);

	hexStream >> hexFile;
	hexStream.close();

	uint16_t computedSum = hexFile.SumLowBlocks();
	uint16_t storedSum = hexFile.GetStoredLowSum();

	if (!ignoreCheckSum && computedSum != storedSum)
		throw runtime_error(string("Stored checksum in ") + fileName + " (" + Format::ToString(Format::Hex(storedSum)) +
				") does not match computed sum (" + Format::ToString(Format::Hex(computedSum)) + ')');

	clog << "Firmware blocks from " << Format::Hex(hexFile.BeginSummed) << " to " << Format::Hex(hexFile.EndSummed) <<
			" have sum " << Format::Hex(computedSum) << ", stored sum is " << Format::Hex(storedSum) << endl;
}

static void writeOrVerify(libusb_device_handle &handle, const HexFile &hexFile, bool shouldWrite)
{
	if (!verbosity)
		clog << (shouldWrite ? "Writing" : "Verifying") << " blocks [" << flush;

	uint16_t segment = 0;

	for (const HexFile::Record &record : hexFile.records)
	{
		switch (record.type)
		{
			case 0:
			{
				if (segment > 0)
					continue;

				if (record.length != 64)
				{
					clog << "Invalid record length " << Format::Dec(record.length) << endl;
					continue;
				}

				if (record.address > 64 * 0xff)
					throw runtime_error("Record address " + to_string(record.address) + " too high");

				if (record.address & 63)
					throw runtime_error("Record address " + to_string(record.address) + " not 64-byte aligned");

				writeOrVerifyBlock(handle, record, shouldWrite);

				break;
			}

			case 4:
				if (verbosity)
					clog << "New segment starting at " << Format::Hex(record.address) << '\n';

				segment = std::get<uint16_t>(record.data);

				break;

			case 1:
				break;
		}
	}

	if (!verbosity)
		clog << ']' << endl;
}

int main(int ac, char * const *av)
{
	int deviceAddress = -1, busNumber = -1;
	bool assumeLoader = false;
	bool ignoreCheckSum = false;
	const char *execName = av[0];
	extern int optind;
	extern char *optarg;

	ios_base::sync_with_stdio(false);

	bool listOnly = false;

	int ch;
	while ((ch = getopt(ac, av, "a:b:chlvL")) != -1)
		switch (ch)
		{
			case 'a':
			{
				char *end;
				deviceAddress = strtoul(optarg, &end, 10);
				if (!*optarg || *end)
				{
					cerr << "Invalid device address " << optarg << '\n';
					goto usage;
				}
				break;
			}

			case 'b':
			{
				char *end;
				busNumber = strtoul(optarg, &end, 10);
				if (!*optarg || *end)
				{
					cerr << "Invalid bus number " << optarg << '\n';
					goto usage;
				}
				break;
			}

			case 'c':
				ignoreCheckSum = true;
				break;

			case 'h':
				goto usage;

			case 'l':
				listOnly = true;
				break;

			case 'v':
				++verbosity;
				break;

			case 'L':
				assumeLoader = true;
				break;

			default:
				cerr << execName << ": Unknown option -- " << static_cast<char>(ch) << '\n';
				goto usage;
		}
	
	ac-= optind;
	av+= optind;

	if ((busNumber != -1) != (deviceAddress != -1))
	{
		cerr << "You must specify both bus number and device address, or neither\n";
		goto usage;
	}

	if (!listOnly && ac != 1)
	{
usage:
		cerr << "usage: " << execName << " [-chvL] [-b <bus-num> -a <dev-addr> ] { <file.hex> | -l }\n";
		cerr << "<file.hex>\tFirmware image to use\n";
		cerr << "-l\t\tList devices and then exit\n";
		cerr << "-v\t\tIncrease verbosity level\n";
		cerr << "-c\t\tIgnore checksum errors in the firmware image file\n";
		cerr << "-b <bus-num>\tSpecify bus number device is attached to\n";
		cerr << "-a <dev-addr>\tSpecify device address on bus\n";
		cerr << "-L\t\tAssume the device is already in bootloader mode\n";
		cerr << "-h\t\tShow this help\n";
		return 64; // EX_USAGE
	}

	try
	{
		int logLevel = (verbosity > 1) ? LIBUSB_LOG_LEVEL_DEBUG : LIBUSB_LOG_LEVEL_WARNING;

		const libusb_init_option options[] =
		{
			{ LIBUSB_OPTION_LOG_LEVEL, { logLevel } }
		};

		libusb_context *contextPtr;
		verifyLibUSB("initialize USB context", libusb_init_context(&contextPtr, options, std::size(options)));
		unique_ptr<libusb_context, decltype(&libusb_exit)> context(contextPtr, &libusb_exit);

		size_t numDevices;
		DeviceListPtr devices = getDeviceList(*context.get(), numDevices);

		if (listOnly)
		{
			listDevices(devices.get(), numDevices);
			return 0;
		}

		HexFile hexFile;
		readHexFile(av[0], hexFile, ignoreCheckSum);

		if (!assumeLoader)
		{
			try
			{
				static const uint16_t productIDs[] = { 0x220, 0x24f };
				libusb_device *keyboardDevice = findDevice(devices.get(), numDevices, busNumber, deviceAddress, 0x05ac, productIDs);

				DeviceHandle keyboardHandle = openDevice(*keyboardDevice);

				libusb_device_descriptor desc;
				getDeviceDescriptor(*libusb_get_device(keyboardHandle.get()), desc);
				clog << "  Device (firmware version): " << Format::Hex(desc.bcdDevice) << '\n';

				// check firmware version

				claimInterface(*keyboardHandle.get(), 0);
				setBootMode(*keyboardHandle.get(), true, 0);

				clog << "Waiting for device to restart...\n";
				sleep(2);

				busNumber = -1;
				deviceAddress = -1;

				devices = getDeviceList(*context.get(), numDevices);
			}
			catch (DeviceNotFound &error)
			{
				if (busNumber != -1)
				{
					// We couldn't find the specific device asked for, don't try to look for the same device in boot
					// mode
					throw error;
				}

				clog << error.what() << '\n';
				clog << "Looking for device already in bootloader mode...\n";
			}
		}

		const uint16_t productIDs[] = { 0x228 };
		libusb_device *loaderDevice = findDevice(devices.get(), numDevices, busNumber, deviceAddress, 0x5ac, productIDs);
		if (!loaderDevice)
			throw runtime_error("Could not find loader device");

		DeviceHandle loaderHandle = openDevice(*loaderDevice);

		// Unref the other devices
		devices.reset();
		numDevices = 0;

		claimInterface(*loaderHandle.get(), 0);

		startUpdate(*loaderHandle.get());

		writeOrVerify(*loaderHandle.get(), hexFile, false);

		finishUpdate(*loaderHandle.get());

		//setBootMode(*loaderHandle.get(), false, 0);
	}
	catch (runtime_error &error)
	{
		cerr << "\nFatal error: " << error.what() << endl;
		return 1;
	}

	return 0;
}
