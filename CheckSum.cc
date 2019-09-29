#include <iostream>

using namespace std;

static const bool verbose = false;

int
main(void)
{
	u_int checkSum = 0;

	ios_base::sync_with_stdio(false);

	freopen(NULL, "rb", stdin);
	u_char ch;

	cin >> noskipws;

	while (cin >> ch)
	{
		checkSum+= ch;
		if (verbose)
			clog << "ch(" << ch << "), checkSum " << hex << checkSum << endl;
	}

	clog << "CheckSum is " << hex << checkSum;
	if (checkSum != 0x1057f8)
		clog << " (discrepancy " << dec << (int)(checkSum - 0x1057f8) << ')';
	clog << endl;
}
