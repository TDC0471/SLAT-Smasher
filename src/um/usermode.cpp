#include <iostream>
#include <Windows.h>

int main()
{

    HANDLE driver = CreateFile(L"\\\\.\\driber", GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (driver == NULL) {
        std::cout << "Failed getting handle to the driver!" << std::endl;
        return 0;
    }
    std::cout << "Got driver handle!" << std::endl;

    char buffer[50];
    DWORD read = 0;
    DWORD code = 0x800; // Hello!

    char c = 'y';
    while (c != 'q') {
        DeviceIoControl(driver, code, NULL, NULL, NULL, NULL, &read, NULL);
        std::cin >> c;
    }
    CloseHandle(driver);
    return 0;
}
