#include <iostream>
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

int main() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    IWICImagingFactory* pFactory = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) return 1;

    IWICBitmapDecoder* pDecoder = nullptr;
    hr = pFactory->CreateDecoderFromFilename(L"Resources/Textures/Characters/char_tank.png", nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (FAILED(hr)) {
        std::cout << "FAILED: " << std::hex << hr << std::endl;
    } else {
        std::cout << "SUCCESS" << std::endl;
    }
    return 0;
}
