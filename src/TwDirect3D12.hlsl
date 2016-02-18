//  ---------------------------------------------------------------------------
//
//  @file       TwDirect3D12.hlsl
//  @author     Johan Kohler
//	@brief		AntTweakBar shaders and techniques for Direct3D12 support
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  ---------------------------------------------------------------------------
 
#define rootSig	"RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT ), " \
				"RootConstants( num32BitConstants=8, b0 )," \
				"DescriptorTable( SRV( t0, numDescriptors=1 ) )," \
				"StaticSampler( s0 )"

struct rootConsts_s
{
	float4 Offset;
	float4 CstColor;
};
ConstantBuffer<rootConsts_s> rootConsts : register(b0);


// Shaders for lines and rectangles

struct LineRectPSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

[RootSignature(rootSig)]
LineRectPSInput LineRectVS(float4 pos : POSITION, float4 color : COLOR) 
{
    LineRectPSInput ps; 
    ps.Pos = pos + rootConsts.Offset;
    ps.Color = color; 
    return ps; 
}

[RootSignature(rootSig)]
LineRectPSInput LineRectCstColorVS(float4 pos : POSITION, float4 color : COLOR)
{
    LineRectPSInput ps; 
    ps.Pos = pos + rootConsts.Offset;
    ps.Color = rootConsts.CstColor;
    return ps; 
}

[RootSignature(rootSig)]
float4 LineRectPS(LineRectPSInput input) : SV_TARGET
{ 
    return input.Color; 
}

// Shaders for text

Texture2D g_Font : register(t0);

SamplerState g_FontSampler : register(s0)
{ 
    Filter = MIN_MAG_MIP_POINT; 
    AddressU = BORDER; 
    AddressV = BORDER; 
    BorderColor = float4(0, 0, 0, 0); 
};

struct TextPSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
    float2 Tex : TEXCOORD0; 
};

[RootSignature(rootSig)]
TextPSInput TextVS(float4 pos : POSITION, float4 color : COLOR, float2 tex : TEXCOORD0)
{
    TextPSInput ps; 
    ps.Pos = pos + rootConsts.Offset;
    ps.Color = color; 
    ps.Tex = tex; 
    return ps; 
}

[RootSignature(rootSig)]
TextPSInput TextCstColorVS(float4 pos : POSITION, float4 color : COLOR, float2 tex : TEXCOORD0)
{
    TextPSInput ps; 
    ps.Pos = pos + rootConsts.Offset;
    ps.Color = rootConsts.CstColor;
    ps.Tex = tex; 
    return ps; 
}
   
[RootSignature(rootSig)]
float4 TextPS(TextPSInput input) : SV_TARGET
{ 
    return g_Font.Sample(g_FontSampler, input.Tex) * input.Color; 
}
