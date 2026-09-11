// RmlUi 2D UI 用のピクセルシェーダー。
// 頂点色・テクスチャとも premultiplied alpha 前提 (RenderInterface_DX12 側で保証)。

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PSInput
{
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float2 uv       : TEXCOORD;
};

float4 main(PSInput input) : SV_TARGET
{
	return gTexture.Sample(gSampler, input.uv) * input.color;
}
