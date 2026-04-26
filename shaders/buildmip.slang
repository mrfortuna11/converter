RWTexture2D<float4> srcTexture;
RWTexture2D<float4> dstTexture;

[shader("compute")]
[numthreads(16, 16, 1)]
void computeMain(uint3 threadIdx : SV_DispatchThreadID)
{
    uint dstWidth, dstHeight;
    dstTexture.GetDimensions(dstWidth, dstHeight);

    uint x = threadIdx.x;
    uint y = threadIdx.y;
    if (x >= dstWidth || y >= dstHeight) return;

    float4 v0 = srcTexture[uint2(x * 2,     y * 2    )];
    float4 v1 = srcTexture[uint2(x * 2 + 1, y * 2    )];
    float4 v2 = srcTexture[uint2(x * 2,     y * 2 + 1)];
    float4 v3 = srcTexture[uint2(x * 2 + 1, y * 2 + 1)];

    dstTexture[uint2(x, y)] = (v0 + v1 + v2 + v3) * 0.25;
}
