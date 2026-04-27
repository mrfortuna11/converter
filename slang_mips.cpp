#include "slang_mips.h"

#include <slang.h>
#include <slang-gfx.h>
#include <slang-com-ptr.h>

#include <iostream>
#include <cstring>
#include <algorithm>

using Slang::ComPtr;

static uint32_t mipCount(uint32_t w, uint32_t h)
{
    uint32_t n = 1;
    while (w > 1 || h > 1) { w = std::max(1u, w / 2); h = std::max(1u, h / 2); ++n; }
    return n;
}

static bool check(gfx::Result r, const char* msg)
{
    if (SLANG_FAILED(r)) {
        std::cerr << "[slang] " << msg << " failed (0x" << std::hex << r << std::dec << ")\n";
        return false;
    }
    return true;
}

static gfx::SubresourceRange mipRange(uint32_t mip)
{
    gfx::SubresourceRange r = {};
    r.aspectMask     = gfx::TextureAspect::Color;
    r.mipLevel       = (int)mip;
    r.mipLevelCount  = 1;
    r.baseArrayLayer = 0;
    r.layerCount     = 1;
    return r;
}

std::vector<MipLevel> generateMipsSlang(
    const uint8_t* srcPixels,
    uint32_t       width,
    uint32_t       height,
    const char*    shaderDir)
{
    using namespace gfx;

    // debug layer 
    // struct DbgCb : IDebugCallback {
    //     SLANG_NO_THROW void SLANG_MCALL handleMessage(
    //         DebugMessageType type, DebugMessageSource, const char* msg) override
    //     { std::cerr << "[gfx] " << (int)type << ": " << msg << "\n"; }
    // };
    // static DbgCb dbg;
    // gfxSetDebugCallback(&dbg);
    // gfxEnableDebugLayer(true);

    // device 
    IDevice::Desc deviceDesc = {};
    deviceDesc.deviceType = DeviceType::DirectX12;
    ComPtr<IDevice> device;
    if (!check(gfxCreateDevice(&deviceDesc, device.writeRef()), "gfxCreateDevice")) return {};

    // slang session
    ComPtr<slang::ISession> slangSession;
    {
        ComPtr<slang::IGlobalSession> global;
        slang::createGlobalSession(global.writeRef());

        slang::SessionDesc sd = {};
        slang::TargetDesc  td = {};
        td.format  = SLANG_DXIL;
        td.profile = global->findProfile("sm_6_0");
        sd.targets     = &td;
        sd.targetCount = 1;

        std::string path = shaderDir;
        const char* paths[] = { path.c_str() };
        sd.searchPaths     = paths;
        sd.searchPathCount = 1;

        if (!check(global->createSession(sd, slangSession.writeRef()), "createSession")) return {};
    }

    // compile shader 
    ComPtr<slang::IModule>     shaderModule;
    ComPtr<slang::IEntryPoint> entryPoint;
    {
        ComPtr<slang::IBlob> diag;
        shaderModule = slangSession->loadModule("buildmip", diag.writeRef());
        if (diag && diag->getBufferSize() > 0)
            std::cerr << (const char*)diag->getBufferPointer();
        if (!shaderModule) { std::cerr << "[slang] loadModule failed\n"; return {}; }

        if (!check(shaderModule->findEntryPointByName("computeMain", entryPoint.writeRef()),
                   "findEntryPointByName")) return {};
    }

    slang::IComponentType* components[] = { shaderModule, entryPoint };
    ComPtr<slang::IComponentType> linked;
    {
        ComPtr<slang::IBlob> diag;
        slangSession->createCompositeComponentType(components, 2, linked.writeRef(), diag.writeRef());
        if (diag && diag->getBufferSize() > 0)
            std::cerr << (const char*)diag->getBufferPointer();
        if (!linked) 
        { 
            std::cerr << "[slang] link failed\n"; 
            return {}; 
        }
    }

    // gfx program + pipeline 
    IShaderProgram::Desc progDesc = {};
    progDesc.slangGlobalScope = linked.get();
    ComPtr<IShaderProgram> program;
    if (!check(device->createProgram(progDesc, program.writeRef()), "createProgram")) 
        return {}; 
    

    ComputePipelineStateDesc pipelineDesc = {};
    pipelineDesc.program = program.get();
    ComPtr<IPipelineState> pipeline;
    if (!check(device->createComputePipelineState(pipelineDesc, pipeline.writeRef()), "createComputePipelineState")) 
        return {};

    // heap + queue 
    ITransientResourceHeap::Desc heapDesc = {};
    heapDesc.constantBufferSize = 4 * 1024;
    ComPtr<ITransientResourceHeap> heap;
    if (!check(device->createTransientResourceHeap(heapDesc, heap.writeRef()), "createTransientResourceHeap")) 
        return {};

    ICommandQueue::Desc queueDesc = {};
    queueDesc.type = ICommandQueue::QueueType::Graphics;
    ComPtr<ICommandQueue> queue;
    if (!check(device->createCommandQueue(queueDesc, queue.writeRef()), "createCommandQueue")) 
        return {};

    const uint32_t nMips = mipCount(width, height);

    // GPU texture (all mips, UAV) 
    ITextureResource::Desc texDesc = {};
    texDesc.type          = IResource::Type::Texture2D;
    texDesc.numMipLevels  = (int)nMips;
    texDesc.arraySize     = 1;
    texDesc.size.width    = (int)width;
    texDesc.size.height   = (int)height;
    texDesc.size.depth    = 1;
    texDesc.format        = Format::R8G8B8A8_UNORM;
    texDesc.defaultState  = ResourceState::CopyDestination;
    texDesc.allowedStates = ResourceStateSet
    (
        ResourceState::CopyDestination,
        ResourceState::UnorderedAccess,
        ResourceState::CopySource
    );

    ComPtr<ITextureResource> gpuTex;
    if (!check(device->createTextureResource(texDesc, nullptr, gpuTex.writeRef()), "createTextureResource")) 
        return {};

    // upload mip 0
    {
        ITextureResource::SubresourceData subData = {};
        subData.data    = srcPixels;
        subData.strideY = width * 4;
        subData.strideZ = 0;

        heap->synchronizeAndReset();
        ComPtr<ICommandBuffer> cmdBuf = heap->createCommandBuffer();
        {
            IResourceCommandEncoder* enc = cmdBuf->encodeResourceCommands();
            enc->uploadTextureData
            (
                gpuTex,
                mipRange(0),
                ITextureResource::Offset3D{0, 0, 0},
                ITextureResource::Extents{(int)width, (int)height, 1},
                &subData, 1
            );
            ITextureResource* texPtr = gpuTex.get();
            enc->textureBarrier
            (
                1, &texPtr,
                ResourceState::CopyDestination,
                ResourceState::UnorderedAccess
            );
            enc->endEncoding();
        }
        cmdBuf->close();
        queue->executeCommandBuffer(cmdBuf, nullptr);
        queue->waitOnHost();
    }

    // generate mips on GPU 
    for (uint32_t mip = 0; mip + 1 < nMips; ++mip)
    {
        uint32_t srcW = std::max(1u, width  >> mip);
        uint32_t srcH = std::max(1u, height >> mip);
        uint32_t dstW = std::max(1u, srcW / 2);
        uint32_t dstH = std::max(1u, srcH / 2);

        IResourceView::Desc viewDesc = {};
        viewDesc.type   = IResourceView::Type::UnorderedAccess;
        viewDesc.format = Format::R8G8B8A8_UNORM;

        viewDesc.subresourceRange = mipRange(mip);
        ComPtr<IResourceView> srcView;
        if (!check(device->createTextureView(gpuTex, viewDesc, srcView.writeRef()),"createTextureView src")) 
            return {};

        viewDesc.subresourceRange = mipRange(mip + 1);
        ComPtr<IResourceView> dstView;
        if (!check(device->createTextureView(gpuTex, viewDesc, dstView.writeRef()), "createTextureView dst")) 
            return {};

        heap->synchronizeAndReset();
        ComPtr<ICommandBuffer> cmdBuf = heap->createCommandBuffer();
        {
            IComputeCommandEncoder* encoder = cmdBuf->encodeComputeCommands();
            IShaderObject* rootObj = encoder->bindPipeline(pipeline);

            ShaderOffset srcOff = {}; 
            srcOff.bindingRangeIndex = 0;
            ShaderOffset dstOff = {}; 
            dstOff.bindingRangeIndex = 1;
            check(rootObj->setResource(srcOff, srcView), "setResource src");
            check(rootObj->setResource(dstOff, dstView), "setResource dst");

            check
            (   encoder->dispatchCompute
                (
                    ((int)dstW + 15) / 16,
                    ((int)dstH + 15) / 16,
                    1
                ), 
                "dispatchCompute"
            );
            encoder->endEncoding();
        }
        cmdBuf->close();
        queue->executeCommandBuffer(cmdBuf, nullptr);
        queue->waitOnHost();
    }

    // transition to CopySource for readback 
    {
        heap->synchronizeAndReset();
        ComPtr<ICommandBuffer> cmdBuf = heap->createCommandBuffer();
        {
            IResourceCommandEncoder* enc = cmdBuf->encodeResourceCommands();
            ITextureResource* texPtr = gpuTex.get();
            enc->textureBarrier
            (
                1, &texPtr,
                ResourceState::UnorderedAccess,
                ResourceState::CopySource
            );
            enc->endEncoding();
        }
        cmdBuf->close();
        queue->executeCommandBuffer(cmdBuf, nullptr);
        queue->waitOnHost();
    }
    // std::cerr << "[slang] transition done\n";

    // readback all mips 
    std::vector<MipLevel> result(nMips);
    for (uint32_t mip = 0; mip < nMips; ++mip)
    {
        uint32_t mW = std::max(1u, width  >> mip);
        uint32_t mH = std::max(1u, height >> mip);
        // align row stride to D3D12_TEXTURE_DATA_PITCH_ALIGNMENT (256)
        constexpr size_t kPitchAlign = 256;
        size_t alignedStride = (mW * 4 + kPitchAlign - 1) & ~(kPitchAlign - 1);
        size_t sz = alignedStride * mH;

        result[mip].width  = mW;
        result[mip].height = mH;
        result[mip].pixels.resize((size_t)mW * mH * 4);

        IBufferResource::Desc bd = {};
        bd.sizeInBytes   = sz;
        bd.defaultState  = ResourceState::CopyDestination;
        bd.allowedStates = ResourceStateSet(ResourceState::CopyDestination);
        bd.memoryType    = MemoryType::ReadBack;

        ComPtr<IBufferResource> staging;
        if (!check(device->createBufferResource(bd, nullptr, staging.writeRef()), "staging")) 
            return {};

        // std::cerr << "[slang] readback mip " << mip << " stride=" << alignedStride << "\n";

        heap->synchronizeAndReset();
        ComPtr<ICommandBuffer> cmdBuf = heap->createCommandBuffer();
        {
            IResourceCommandEncoder* encoder = cmdBuf->encodeResourceCommands();
            encoder->copyTextureToBuffer
            (
                staging, 0, sz, alignedStride,
                gpuTex, ResourceState::CopySource,
                mipRange(mip),
                ITextureResource::Offset3D{0, 0, 0},
                ITextureResource::Extents{(int)mW, (int)mH, 1}
            );
            encoder->endEncoding();
        }
        cmdBuf->close();
        queue->executeCommandBuffer(cmdBuf, nullptr);
        queue->waitOnHost();
        // std::cerr << "[slang] readback mip " << mip << " copy done\n";

        void* mapped = nullptr;
        MemoryRange range{ 0, sz };
        if (!check(staging->map(&range, &mapped), "buffer.map")) 
            return {};

        // copy row by row to strip alignment padding
        const uint8_t* src = static_cast<const uint8_t*>(mapped);
        uint8_t*    dst = result[mip].pixels.data();
        for (uint32_t row = 0; row < mH; ++row)
            std::memcpy(dst + row * mW * 4, src + row * alignedStride, mW * 4);

        staging->unmap(nullptr);
    }

    return result;
}
