// Per-pixel color data passed through the pixel shader.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD;
};

Texture2D g_texture : register(t0);
SamplerState g_sampler : register(s0);

// A pass-through function for the (interpolated) color data.
float4 main(PixelShaderInput input) : SV_TARGET
{
	//float3 filter = float3(input.uv[1], input.uv[0], 0.85f);
	//return float4(filter, 1.0f);

	float4 textureOutput = g_texture.Sample(g_sampler, input.uv);
	
	return float4(textureOutput[0], textureOutput[1], textureOutput[2], textureOutput[3]);

	//return g_texture.Load(1, 1);
}
