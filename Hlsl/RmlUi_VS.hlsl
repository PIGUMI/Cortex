// RmlUi 2D UI 用の頂点シェーダー。RenderInterface_DX12 が直接コンパイルする
// (Shader/Material のリフレクション経由パイプラインとは別系統)。

cbuffer Projection : register(b0) // フレームに1回更新 (CBV)
{
	float4x4 projection;
};

cbuffer Translation : register(b1) // 描画のたびに更新 (ルート定数)
{
	float2 translation;
};

struct VSInput
{
	float2 position : POSITION;
	float4 color    : COLOR;
	float2 uv       : TEXCOORD;
};

struct VSOutput
{
	float4 position : SV_POSITION;
	float4 color    : COLOR;
	float2 uv       : TEXCOORD;
};

VSOutput main(VSInput input)
{
	VSOutput output;
	const float2 worldPos = input.position + translation;
	output.position = mul(projection, float4(worldPos, 0.0f, 1.0f));
	output.color = input.color;
	output.uv = input.uv;
	return output;
}
