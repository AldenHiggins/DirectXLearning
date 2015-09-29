// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

Texture2D g_texture : register(t1);
SamplerState g_sampler : register(s1);

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	float3 filter = float3(0.45f, input.uv[0], 0.85f);
	return float4(filter, 1.0f);

	//return g_texture.Sample(g_sampler, input.uv);
}
