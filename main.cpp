#include <DirectXTex.h>
#include <filesystem>
#include <iostream>

static void usage(const char* prog) {
    std::cerr << "Usage: " << prog << " <input.tga> <output.dds>\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) { usage(argv[0]); return 1; }

    std::filesystem::path src = argv[1];
    std::filesystem::path dst = argv[2];

    if (src.extension() != ".tga" && src.extension() != ".TGA") {
        std::cerr << "Input must be a .tga file\n";
        return 1;
    }

    // load
    DirectX::TexMetadata  meta{};
    DirectX::ScratchImage image{};

    HRESULT hr = DirectX::LoadFromTGAFile(src.wstring().c_str(), &meta, image);
    if (FAILED(hr)) {
        std::cerr << "Failed to load TGA (hr=" << std::hex << hr << ")\n";
        return 1;
    }

    std::cout << "Loaded: " << meta.width << "x" << meta.height
              << "  fmt=" << meta.format << "\n";

    //generate mips (standard box filter, non-WIC path)
    DirectX::ScratchImage mipped{};
    hr = DirectX::GenerateMipMaps(image.GetImages(),
                                  image.GetImageCount(),
                                  image.GetMetadata(),
                                  DirectX::TEX_FILTER_BOX | DirectX::TEX_FILTER_FORCE_NON_WIC,
                                  0,   // 0 = full mip chain
                                  mipped);
    if (FAILED(hr)) {
        std::cerr << "GenerateMipMaps failed (hr=" << std::hex << hr << ")\n";
        return 1;
    }

    std::cout << "Mip levels: " << mipped.GetMetadata().mipLevels << "\n";

    hr = DirectX::SaveToDDSFile(mipped.GetImages(),
                                mipped.GetImageCount(),
                                mipped.GetMetadata(),
                                DirectX::DDS_FLAGS_NONE,
                                dst.wstring().c_str());
    if (FAILED(hr)) {
        std::cerr << "SaveToDDSFile failed (hr=" << std::hex << hr << ")\n";
        return 1;
    }

    std::cout << "Saved: " << dst << "\n";
    return 0;
}
