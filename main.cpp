#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <DirectXTex.h>
#include <filesystem>
#include <iostream>
#include <string_view>

#include "slang_mips.h"

static void usage(const char* prog)
{
    std::cerr << "Usage: " << prog << " [--slang] <input.tga> <output.dds>\n"
              << "  --slang  generate mips via Slang GPU compute (default: CPU/DirectXTex)\n";
}

static std::filesystem::path exeDir()
{
    wchar_t buf[4096] = {};
    GetModuleFileNameW(nullptr, buf, 4096);
    return std::filesystem::path(buf).parent_path();
}

int main(int argc, char* argv[])
{
    bool useSlang = false;
    int  firstArg = 1;

    if (argc > 1 && std::string_view(argv[1]) == "--slang") 
    { 
        useSlang = true; firstArg = 2; 
    }

    if (argc - firstArg != 2) 
    { 
        usage(argv[0]); return 1;
    }

    std::filesystem::path src = argv[firstArg];
    std::filesystem::path dst = argv[firstArg + 1];

    if (src.extension() != ".tga" && src.extension() != ".TGA")
    { 
        std::cerr << "Input must be a .tga file\n"; 
        return 1; 
    }

    // load TGA 
    DirectX::TexMetadata  meta{};
    DirectX::ScratchImage image{};
    HRESULT hr = DirectX::LoadFromTGAFile(src.wstring().c_str(), &meta, image);
    if (FAILED(hr)) 
    { 
        std::cerr << "Failed to load TGA (hr=" << std::hex << hr << ")\n"; 
        return 1; 
    }

    std::cerr << "Loaded: " << meta.width << "x" << meta.height << "  fmt=" << meta.format << "\n";

    // generate mips 
    DirectX::ScratchImage mipped{};

    if (useSlang)
    {
        std::string shaderDir = (exeDir() / "shaders").string();
        if (!std::filesystem::exists(shaderDir))
            shaderDir = (std::filesystem::path(__FILE__).parent_path() / "shaders").string();

        std::vector<MipLevel> mips = generateMipsSlang
        (
            image.GetPixels(),
            static_cast<uint32_t>(meta.width),
            static_cast<uint32_t>(meta.height),
            shaderDir.c_str()
        );

        if (mips.empty()) 
        { 
            std::cerr << "Slang mip generation failed\n"; 
            return 1; 
        }

        DirectX::TexMetadata outMeta = meta;
        outMeta.mipLevels = mips.size();
        mipped.Initialize(outMeta);

        for (size_t i = 0; i < mips.size(); ++i)
        {
            const DirectX::Image* img = mipped.GetImage(i, 0, 0);
            std::memcpy(img->pixels, mips[i].pixels.data(), mips[i].pixels.size());
        }

        std::cout << "Mip levels (Slang): " << mips.size() << "\n";
    }
    else
    {
        hr = DirectX::GenerateMipMaps
        (
            image.GetImages(), image.GetImageCount(), image.GetMetadata(),
            DirectX::TEX_FILTER_BOX | DirectX::TEX_FILTER_FORCE_NON_WIC,
            0, mipped
        );
        
        if (FAILED(hr)) 
        { 
            std::cerr << "GenerateMipMaps failed (hr=" << std::hex << hr << ")\n"; 
            return 1; 
        }
        std::cout << "Mip levels (CPU): " << mipped.GetMetadata().mipLevels << "\n";
    }

    // save DDS 
    hr = DirectX::SaveToDDSFile(
        mipped.GetImages(), mipped.GetImageCount(), mipped.GetMetadata(),
        DirectX::DDS_FLAGS_NONE, dst.wstring().c_str());
    if (FAILED(hr)) 
    { 
        std::cerr << "SaveToDDSFile failed (hr=" << std::hex << hr << ")\n"; 
        return 1; 
    }

    std::cout << "Saved: " << dst << "\n";
    return 0;
}
