#include <string>
#include <stdexcept>

struct SyscallError : public std::runtime_error
{
	SyscallError(const std::string &msg, int errNo = errno)
		: Super(msg + ": " + strerror(errNo))
		, ErrNo(errNo)
	{
	}

	int ErrNo;

private:
	typedef std::runtime_error Super;
};
