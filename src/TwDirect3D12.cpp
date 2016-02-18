//  ---------------------------------------------------------------------------
//
//  @file       TwDirect3D12.cpp
//  @author     Johan Kohler
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  ---------------------------------------------------------------------------


#include "TwPrecomp.h"
#include "TwDirect3D12.h"
#include "TwMgr.h"
#include "TwColors.h"

#include <d3d12.h>


using namespace std;

const char *g_ErrCantCreateCommandAllocator12 = "Cannot create Direct3D12 command allocator";
const char *g_ErrCantCreateGraphicsCommandList12 = "Cannot create Direct3D12 graphics command list";
const char *g_ErrCantCreatePipelineState12 = "Cannot create Direct3D12 pipeline state";
const char *g_ErrCantCreateRootSignature12 = "Cannot create Direct3D12 root signature";
const char *g_ErrCantCreateDescriptorHeap12 = "Cannot create Direct3D12 descriptor heap";

//  ---------------------------------------------------------------------------
//  Shaders : In order to avoid linkage with D3DX11 or D3DCompile libraries,
//  vertex and pixel shaders are compiled offline in a pre-build step using
//  the fxc.exe compiler (from the DirectX SDK Aug'09 or later)

namespace D3D12Shaders
{
#ifdef _WIN64
#   ifdef _DEBUG
#       include "debug64\TwDirect3D12_LineRectVS.h"
#       include "debug64\TwDirect3D12_LineRectCstColorVS.h"
#       include "debug64\TwDirect3D12_LineRectPS.h"
#       include "debug64\TwDirect3D12_TextVS.h"
#       include "debug64\TwDirect3D12_TextCstColorVS.h"
#       include "debug64\TwDirect3D12_TextPS.h"
#   else
#       include "release64\TwDirect3D12_LineRectVS.h"
#       include "release64\TwDirect3D12_LineRectCstColorVS.h"
#       include "release64\TwDirect3D12_LineRectPS.h"
#       include "release64\TwDirect3D12_TextVS.h"
#       include "release64\TwDirect3D12_TextCstColorVS.h"
#       include "release64\TwDirect3D12_TextPS.h"
#   endif
#else
#   ifdef _DEBUG
#       include "debug32\TwDirect3D12_LineRectVS.h"
#       include "debug32\TwDirect3D12_LineRectCstColorVS.h"
#       include "debug32\TwDirect3D12_LineRectPS.h"
#       include "debug32\TwDirect3D12_TextVS.h"
#       include "debug32\TwDirect3D12_TextCstColorVS.h"
#       include "debug32\TwDirect3D12_TextPS.h"
#   else
#       include "release32\TwDirect3D12_LineRectVS.h"
#       include "release32\TwDirect3D12_LineRectCstColorVS.h"
#       include "release32\TwDirect3D12_LineRectPS.h"
#       include "release32\TwDirect3D12_TextVS.h"
#       include "release32\TwDirect3D12_TextCstColorVS.h"
#       include "release32\TwDirect3D12_TextPS.h"
#   endif
#endif
}

const RECT FullRect = { 0, 0, 16000, 16000 };
static bool RectIsFull(const RECT& r) { return r.left == FullRect.left && r.right == FullRect.right && r.top == FullRect.top && r.bottom == FullRect.bottom; }

int CTwGraphDirect3D12::Init()
{
	assert(g_TwMgr != NULL);
	assert(g_TwMgr->m_Device != NULL);

	HRESULT hr;

	m_D3DDev = static_cast<ID3D12Device *>(g_TwMgr->m_Device);
	m_D3DDevInitialRefCount = m_D3DDev->AddRef() - 1;

	m_Drawing = false;
	m_OffsetX = m_OffsetY = 0;
	m_WndWidth = 0;
	m_WndHeight = 0;

	m_UploadResource = NULL;
	m_UploadResourceSize = 128 * 1024;
	m_UploadResourceUsed = m_UploadResourceSize;
	
	m_D3DGraphCmdList = NULL;
	g_TwMgr->m_GraphContext = NULL;

	m_Font = NULL;
	m_FontResource = NULL;

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc;
	memset(&heapDesc, 0, sizeof(heapDesc));
	heapDesc.NumDescriptors = 1;
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	hr = m_D3DDev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap));
	if (FAILED(hr))
	{
		g_TwMgr->SetLastError(g_ErrCantCreateDescriptorHeap12);
		Shut();
		return 0;
	}

	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = { 0 };

	desc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	desc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	desc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	desc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	desc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	desc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	desc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	desc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

	desc.SampleMask = UINT_MAX;
	desc.NumRenderTargets = 1;
	desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	desc.SampleDesc.Count = 1;

	{
		D3D12_INPUT_ELEMENT_DESC vertexDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(CLineRectVtx, m_Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		desc.InputLayout.pInputElementDescs = vertexDesc;
		desc.InputLayout.NumElements = 2;

		desc.PS.pShaderBytecode = D3D12Shaders::g_LineRectPS;
		desc.PS.BytecodeLength = sizeof(D3D12Shaders::g_LineRectPS);
		desc.VS.pShaderBytecode = D3D12Shaders::g_LineRectVS;
		desc.VS.BytecodeLength = sizeof(D3D12Shaders::g_LineRectVS);

		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;

		desc.RasterizerState.AntialiasedLineEnable = TRUE;
		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Line_AA_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		desc.RasterizerState.AntialiasedLineEnable = FALSE;

		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Line_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Tri_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		desc.VS.pShaderBytecode = D3D12Shaders::g_LineRectCstColorVS;
		desc.VS.BytecodeLength = sizeof(D3D12Shaders::g_LineRectCstColorVS);
		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_TriCstColor_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		desc.VS.pShaderBytecode = D3D12Shaders::g_LineRectVS;
		desc.VS.BytecodeLength = sizeof(D3D12Shaders::g_LineRectVS);

		desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Tri_CW_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Tri_CCW_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}

		hr = m_D3DDev->CreateRootSignature(0, D3D12Shaders::g_LineRectPS, sizeof(D3D12Shaders::g_LineRectPS), IID_PPV_ARGS(&m_RootSignature));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreateRootSignature12);
			Shut();
			return 0;
		}
	}

	{
		D3D12_INPUT_ELEMENT_DESC textLayout[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(CTextVtx, m_Color), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CTextVtx, m_UV), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
		};

		desc.InputLayout.pInputElementDescs = textLayout;
		desc.InputLayout.NumElements = 3;
		desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

		desc.PS.pShaderBytecode = D3D12Shaders::g_TextPS;
		desc.PS.BytecodeLength = sizeof(D3D12Shaders::g_TextPS);
		desc.VS.pShaderBytecode = D3D12Shaders::g_TextVS;
		desc.VS.BytecodeLength = sizeof(D3D12Shaders::g_TextVS);

		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_Text_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}


		desc.VS.pShaderBytecode = D3D12Shaders::g_TextCstColorVS;
		desc.VS.BytecodeLength = sizeof(D3D12Shaders::g_TextCstColorVS);

		hr = m_D3DDev->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&m_TextCstColor_PSO));
		if (FAILED(hr))
		{
			g_TwMgr->SetLastError(g_ErrCantCreatePipelineState12);
			Shut();
			return 0;
		}
	}


	return 1;
}

int CTwGraphDirect3D12::Shut()
{
	return 0;
}

void CTwGraphDirect3D12::BeginDraw(int _WndWidth, int _WndHeight)
{
	assert(m_Drawing == false && _WndWidth > 0 && _WndHeight > 0);
	m_Drawing = true;

	m_D3DGraphCmdList = (ID3D12GraphicsCommandList *)g_TwMgr->m_GraphContext;

	m_WndWidth = _WndWidth;
	m_WndHeight = _WndHeight;
	m_OffsetX = m_OffsetY = 0;
	if (m_D3DGraphCmdList == NULL)
		return;

	// Reuse resources from last frame.
	if (!m_UploadResourcesFreeThisFrame.empty())
	{
		if (m_UploadResourcesFree.empty())
		{
			std::swap(m_UploadResourcesFree, m_UploadResourcesFreeThisFrame);
		}
		else
		{
			m_UploadResourcesFree.insert(m_UploadResourcesFree.end(), m_UploadResourcesFreeThisFrame.begin(), m_UploadResourcesFreeThisFrame.end());
		}
	}

	// Free any upload resources used last frame, this is usually due to the upload buffer not being big enough for the data to upload.
	for (ID3D12Resource *res : m_ResourcesToFree)
	{
		res->Release();
	}
	m_ResourcesToFree.clear();

	// Copying to GPU memory can only be done when we have a graphics cmd list, here we process any copies that was requested while we did not have one, like during initialization.
	if (!deferedCopying.empty())
	{
		for (std::pair<UploadBuffer, ID3D12Resource *> &copy : deferedCopying)
		{
			D3D12_RESOURCE_BARRIER barrier;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			barrier.Transition.pResource = copy.second;
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.Subresource = 0;
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			m_D3DGraphCmdList->ResourceBarrier(1, &barrier);

			D3D12_RESOURCE_DESC desc = copy.second->GetDesc();
			if(desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{
				m_D3DGraphCmdList->CopyBufferRegion(copy.second, 0, copy.first.resource, copy.first.offset, copy.first.bytes);
			}
			else if (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
			{
				D3D12_TEXTURE_COPY_LOCATION loc[2] = { 0 };
				loc[0].Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				loc[0].pResource = copy.first.resource;
				m_D3DDev->GetCopyableFootprints(&desc, 0, 1, copy.first.offset, &loc[0].PlacedFootprint, NULL, NULL, NULL);
				loc[1].Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				loc[1].pResource = copy.second;
				loc[1].SubresourceIndex = 0;

				m_D3DGraphCmdList->CopyTextureRegion(&loc[1], 0, 0, 0, &loc[0], NULL);
			}


			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

			m_D3DGraphCmdList->ResourceBarrier(1, &barrier);
			m_ResourcesToFree.push_back(copy.first.resource);
			m_ResourcesToFree.push_back(copy.second);
		}
		deferedCopying.clear();
	}

	D3D12_VIEWPORT vp;
	vp.Width = (FLOAT)_WndWidth;
	vp.Height = (FLOAT)_WndHeight;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;

	m_D3DGraphCmdList->RSSetViewports(1, &vp);
	
	m_ViewportAndScissorRects[0] = FullRect;
	m_ViewportAndScissorRects[1] = FullRect;
	m_D3DGraphCmdList->RSSetScissorRects(1, m_ViewportAndScissorRects);

}

void CTwGraphDirect3D12::EndDraw()
{
	assert(m_Drawing == true);
	m_Drawing = false;
}

bool CTwGraphDirect3D12::IsDrawing()
{
	return m_Drawing;
}

void CTwGraphDirect3D12::Restore()
{
	if(m_FontResource) m_FontResource->Release();
	m_Font = NULL;
}


void CTwGraphDirect3D12::ChangeViewport(int _X0, int _Y0, int _Width, int _Height, int _OffsetX, int _OffsetY)
{
	if (_Width>0 && _Height>0)
	{
		assert(m_D3DGraphCmdList != NULL);
		/* viewport changes screen coordinates, use scissor instead
		D3D11_VIEWPORT vp;
		vp.TopLeftX = _X0;
		vp.TopLeftY = _Y0;
		vp.Width = _Width;
		vp.Height = _Height;
		vp.MinDepth = 0;
		vp.MaxDepth = 1;
		m_D3DDev->RSSetViewports(1, &vp);
		*/

		m_ViewportAndScissorRects[0].left = _X0;
		m_ViewportAndScissorRects[0].right = _X0 + _Width - 1;
		m_ViewportAndScissorRects[0].top = _Y0;
		m_ViewportAndScissorRects[0].bottom = _Y0 + _Height - 1;
		if (RectIsFull(m_ViewportAndScissorRects[1]))
			m_D3DGraphCmdList->RSSetScissorRects(1, m_ViewportAndScissorRects); // viewport clipping only
		else
			m_D3DGraphCmdList->RSSetScissorRects(2, m_ViewportAndScissorRects);

		m_OffsetX = _X0 + _OffsetX;
		m_OffsetY = _Y0 + _OffsetY;
	}
}

void CTwGraphDirect3D12::RestoreViewport()
{
	assert(m_D3DGraphCmdList != NULL);
	m_ViewportAndScissorRects[0] = FullRect;
	m_D3DGraphCmdList->RSSetScissorRects(1, m_ViewportAndScissorRects + 1); // scissor only

	m_OffsetX = m_OffsetY = 0;
}

void CTwGraphDirect3D12::SetScissor(int _X0, int _Y0, int _Width, int _Height)
{
	if (_Width > 0 && _Height > 0)
	{
		assert(m_D3DGraphCmdList != NULL);
		m_ViewportAndScissorRects[1].left = _X0 - 2;
		m_ViewportAndScissorRects[1].right = _X0 + _Width - 3;
		m_ViewportAndScissorRects[1].top = _Y0 - 1;
		m_ViewportAndScissorRects[1].bottom = _Y0 + _Height - 1;
		if (RectIsFull(m_ViewportAndScissorRects[0]))
			m_D3DGraphCmdList->RSSetScissorRects(1, m_ViewportAndScissorRects + 1); // no viewport clipping
		else
			m_D3DGraphCmdList->RSSetScissorRects(2, m_ViewportAndScissorRects);
	}
	else
	{
		m_ViewportAndScissorRects[1] = FullRect;
		m_D3DGraphCmdList->RSSetScissorRects(1, m_ViewportAndScissorRects); // apply viewport clipping only
	}
}

//  ---------------------------------------------------------------------------

static inline float ToNormScreenX(int x, int wndWidth)
{
	return 2.0f*((float)x - 0.5f) / wndWidth - 1.0f;
}

static inline float ToNormScreenY(int y, int wndHeight)
{
	return 1.0f - 2.0f*((float)y - 0.5f) / wndHeight;
}

static inline color32 ToR8G8B8A8(color32 col)
{
	return (col & 0xff00ff00) | ((col >> 16) & 0xff) | ((col << 16) & 0xff0000);
}

void * CTwGraphDirect3D12::UploadBuffer::Map()
{
	D3D12_RANGE zeroRange = { 0 };
	void *ret = NULL;
	HRESULT hr = resource->Map(0, &zeroRange, &ret);
	assert(!FAILED(hr));
	return static_cast<void *>(static_cast<char *>(ret) + offset);
}

void CTwGraphDirect3D12::UploadBuffer::Unmap()
{
	D3D12_RANGE range = { offset, offset+bytes };
	resource->Unmap(0, &range);
}

uint64_t CTwGraphDirect3D12::UploadBuffer::GpuAddr()
{
	return resource->GetGPUVirtualAddress() + offset;
}

CTwGraphDirect3D12::UploadBuffer CTwGraphDirect3D12::AllocUploadBuffer(uint32_t bytes, uint32_t align/*=16*/)
{
	CTwGraphDirect3D12::UploadBuffer ret;

	m_UploadResourceUsed = (m_UploadResourceUsed + align - 1) & ~(align - 1);

	if (bytes > m_UploadResourceSize)
	{
		D3D12_HEAP_PROPERTIES heapProp = { D3D12_HEAP_TYPE_UPLOAD };
		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = bytes;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		m_D3DDev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&ret.resource));
		ret.offset = 0;
		ret.bytes = bytes;
		m_ResourcesToFree.push_back(ret.resource);
	}
	else
	{
		if (bytes > m_UploadResourceSize - m_UploadResourceUsed)
		{
			if(m_UploadResource)
			{
				m_UploadResourcesFreeThisFrame.push_back(m_UploadResource);
			}
			m_UploadResourceUsed = 0;
			if(m_UploadResourcesFree.empty())
			{
				D3D12_HEAP_PROPERTIES heapProp = { D3D12_HEAP_TYPE_UPLOAD };
				D3D12_RESOURCE_DESC desc;
				memset(&desc, 0, sizeof(desc));
				desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
				desc.Width = m_UploadResourceSize;
				desc.Height = 1;
				desc.DepthOrArraySize = 1;
				desc.MipLevels = 1;
				desc.SampleDesc.Count = 1;
				desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

				m_D3DDev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, NULL, IID_PPV_ARGS(&m_UploadResource));
			}
			else
			{
				m_UploadResource = m_UploadResourcesFree.back();
				m_UploadResourcesFree.pop_back();
			}
		}
		ret.resource = m_UploadResource;
		ret.offset = m_UploadResourceUsed;
		ret.bytes = bytes;
		m_UploadResourceUsed += bytes;
	}
	return ret;
}

ID3D12Resource * CTwGraphDirect3D12::CreateGPUCopy(UploadBuffer &upload, D3D12_RESOURCE_DESC *inDesc /* = nullptr */)
{
	D3D12_HEAP_PROPERTIES heapProp = { D3D12_HEAP_TYPE_DEFAULT };
	D3D12_RESOURCE_DESC desc;
	if(inDesc == NULL)
	{
		memset(&desc, 0, sizeof(desc));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		desc.Width = upload.bytes;
		desc.Height = 1;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		inDesc = &desc;
	}

	ID3D12Resource * ret;
	m_D3DDev->CreateCommittedResource(&heapProp, D3D12_HEAP_FLAG_NONE, inDesc, D3D12_RESOURCE_STATE_COPY_DEST, NULL, IID_PPV_ARGS(&ret));

	if(m_D3DGraphCmdList)
	{
		D3D12_RESOURCE_BARRIER barrier;
		barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barrier.Transition.pResource = ret;
		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_GENERIC_READ;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.Subresource = 0;
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		m_D3DGraphCmdList->ResourceBarrier(1, &barrier);

		if (inDesc->Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
		{
			m_D3DGraphCmdList->CopyBufferRegion(ret, 0, upload.resource, upload.offset, upload.bytes);
		}
		else if (inDesc->Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D)
		{
			D3D12_TEXTURE_COPY_LOCATION loc[2] = { 0 };
			loc[0].Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			loc[0].pResource = upload.resource;
			m_D3DDev->GetCopyableFootprints(inDesc, 0, 1, upload.offset, &loc[0].PlacedFootprint, NULL, NULL, NULL);
			loc[1].Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
			loc[1].pResource = ret;
			loc[1].SubresourceIndex = 0;

			m_D3DGraphCmdList->CopyTextureRegion(&loc[1], 0, 0, 0, &loc[0], NULL);
		}


		barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;

		m_D3DGraphCmdList->ResourceBarrier(1, &barrier);
	}
	else
	{
		upload.resource->AddRef();
		ret->AddRef();
		deferedCopying.push_back(make_pair(upload, ret));
	}
	
	return ret;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D12::DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color0, color32 _Color1, bool _AntiAliased /*= false*/)
{
	assert(m_Drawing == true);
	assert(m_D3DGraphCmdList != NULL);

	UploadBuffer resource = AllocUploadBuffer(sizeof(CLineRectVtx) * 2);

	CLineRectVtx *vertices = (CLineRectVtx *)resource.Map();
	// Fill vertex buffer
	vertices[0].m_Pos[0] = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
	vertices[0].m_Pos[1] = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
	vertices[0].m_Pos[2] = 0;
	vertices[0].m_Color = ToR8G8B8A8(_Color0);
	vertices[1].m_Pos[0] = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
	vertices[1].m_Pos[1] = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);
	vertices[1].m_Pos[2] = 0;
	vertices[1].m_Color = ToR8G8B8A8(_Color1);

	resource.Unmap();

	if (_AntiAliased)
		m_D3DGraphCmdList->SetPipelineState(m_Line_AA_PSO);
	else
		m_D3DGraphCmdList->SetPipelineState(m_Line_PSO);
	m_D3DGraphCmdList->SetGraphicsRootSignature(m_RootSignature);

	CConstants rs = {
		{0,0,0,0},
		{1,1,1,1}
	};
	m_D3DGraphCmdList->SetGraphicsRoot32BitConstants(0, sizeof(rs) / sizeof(uint32_t), &rs, 0);

	D3D12_VERTEX_BUFFER_VIEW vbView;
	vbView.BufferLocation = resource.GpuAddr();
	vbView.SizeInBytes = resource.bytes;
	vbView.StrideInBytes = sizeof(vertices[0]);

	m_D3DGraphCmdList->IASetVertexBuffers(0, 1, &vbView);
	m_D3DGraphCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
	m_D3DGraphCmdList->DrawInstanced(2, 1, 0, 0);
}

void CTwGraphDirect3D12::DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color00, color32 _Color10, color32 _Color01, color32 _Color11)
{
	assert(m_Drawing == true);
	assert(m_D3DGraphCmdList != NULL);

	// border adjustment
	if (_X0<_X1)
		++_X1;
	else if (_X0>_X1)
		++_X0;
	if (_Y0<_Y1)
		++_Y1;
	else if (_Y0>_Y1)
		++_Y0;

	float x0 = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
	float y0 = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
	float x1 = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
	float y1 = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);

	UploadBuffer resource = AllocUploadBuffer(sizeof(CLineRectVtx) * 4);

	CLineRectVtx *vertices = (CLineRectVtx *)resource.Map();
	// Fill vertex buffer
	vertices[0].m_Pos[0] = x0;
	vertices[0].m_Pos[1] = y0;
	vertices[0].m_Pos[2] = 0;
	vertices[0].m_Color = ToR8G8B8A8(_Color00);
	vertices[1].m_Pos[0] = x1;
	vertices[1].m_Pos[1] = y0;
	vertices[1].m_Pos[2] = 0;
	vertices[1].m_Color = ToR8G8B8A8(_Color10);
	vertices[2].m_Pos[0] = x0;
	vertices[2].m_Pos[1] = y1;
	vertices[2].m_Pos[2] = 0;
	vertices[2].m_Color = ToR8G8B8A8(_Color01);
	vertices[3].m_Pos[0] = x1;
	vertices[3].m_Pos[1] = y1;
	vertices[3].m_Pos[2] = 0;
	vertices[3].m_Color = ToR8G8B8A8(_Color11);

	resource.Unmap();

	m_D3DGraphCmdList->SetPipelineState(m_Tri_PSO);
	m_D3DGraphCmdList->SetGraphicsRootSignature(m_RootSignature);


	CConstants rs = {
		{ 0,0,0,0 },
		{ 1,1,1,1 }
	};
	m_D3DGraphCmdList->SetGraphicsRoot32BitConstants(0, sizeof(rs) / sizeof(uint32_t), &rs, 0);

	D3D12_VERTEX_BUFFER_VIEW vbView;
	vbView.BufferLocation = resource.GpuAddr();
	vbView.SizeInBytes = resource.bytes;
	vbView.StrideInBytes = sizeof(vertices[0]);

	m_D3DGraphCmdList->IASetVertexBuffers(0, 1, &vbView);
	m_D3DGraphCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	m_D3DGraphCmdList->DrawInstanced(4, 1, 0, 0);
}

void CTwGraphDirect3D12::DrawTriangles(int _NumTriangles, int *_Vertices, color32 *_Colors, Cull _CullMode)
{
	assert(m_Drawing == true);
	assert(m_D3DGraphCmdList != NULL);

	if (_NumTriangles <= 0)
		return;

	UploadBuffer resource = AllocUploadBuffer(sizeof(CLineRectVtx) * _NumTriangles * 3);
	CLineRectVtx *vertices = (CLineRectVtx *)resource.Map();

	for (int i = 0; i < _NumTriangles * 3; i++)
	{
		vertices[i].m_Pos[0] = ToNormScreenX(_Vertices[i*2+0] + m_OffsetX, m_WndWidth);
		vertices[i].m_Pos[1] = ToNormScreenY(_Vertices[i*2+1] + m_OffsetY, m_WndHeight);
		vertices[i].m_Pos[2] = 0;
		vertices[i].m_Color = ToR8G8B8A8(_Colors[i]);
	}

	resource.Unmap();
	switch (_CullMode)
	{
	default:
	case ITwGraph::CULL_NONE:
		m_D3DGraphCmdList->SetPipelineState(m_Tri_PSO);
		break;
	case ITwGraph::CULL_CW:
		m_D3DGraphCmdList->SetPipelineState(m_Tri_CW_PSO);
		break;
	case ITwGraph::CULL_CCW:
		m_D3DGraphCmdList->SetPipelineState(m_Tri_CCW_PSO);
		break;
	}
	m_D3DGraphCmdList->SetGraphicsRootSignature(m_RootSignature);


	CConstants rs = {
		{ 0,0,0,0 },
		{ 1,1,1,1 }
	};
	m_D3DGraphCmdList->SetGraphicsRoot32BitConstants(0, sizeof(rs) / sizeof(uint32_t), &rs, 0);

	D3D12_VERTEX_BUFFER_VIEW vbView;
	vbView.BufferLocation = resource.GpuAddr();
	vbView.SizeInBytes = resource.bytes;
	vbView.StrideInBytes = sizeof(vertices[0]);

	m_D3DGraphCmdList->IASetVertexBuffers(0, 1, &vbView);
	m_D3DGraphCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	m_D3DGraphCmdList->DrawInstanced(_NumTriangles*3, 1, 0, 0);
}

void * CTwGraphDirect3D12::NewTextObj()
{
	CTextObj *ret = new CTextObj;
	memset(ret, 0, sizeof(*ret));
	return ret;
}

void CTwGraphDirect3D12::DeleteTextObj(void *_TextObj)
{
	CTextObj *textObj = (CTextObj *)_TextObj;
	if(textObj->m_BgVertexBuffer) textObj->m_BgVertexBuffer->Release();
	if(textObj->m_TextVertexBuffer) textObj->m_TextVertexBuffer->Release();
	delete textObj;
}

void CTwGraphDirect3D12::BuildText(void *_TextObj, const std::string *_TextLines, color32 *_LineColors, color32 *_LineBgColors, int _NbLines, const CTexFont *_Font, int _Sep, int _BgWidth)
{
	assert(m_Drawing == true);
	assert(_TextObj != NULL);
	assert(_Font != NULL);

	CTextObj *textObj = (CTextObj *)_TextObj;

	if (_Font != m_Font)
	{
		if (m_FontResource) 
		{ 
			m_ResourcesToFree.push_back(m_FontResource);
			m_FontResource = NULL;
		}

		D3D12_RESOURCE_DESC desc;
		memset(&desc, 0, sizeof(desc));
		desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		desc.Width = _Font->m_TexWidth;
		desc.Height = _Font->m_TexHeight;
		desc.DepthOrArraySize = 1;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.Format = DXGI_FORMAT_R8_UNORM;
		desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

		uint64_t rowBytes=0,totalBytes=0;
		m_D3DDev->GetCopyableFootprints(&desc, 0, 1, 0, NULL, NULL, &rowBytes, &totalBytes);
		UploadBuffer texUpload = AllocUploadBuffer(totalBytes,512);

		uint8_t *dest = (uint8_t *)texUpload.Map();
		for (int y = 0; y < desc.Height; y++)
		{
			memcpy(dest + y * rowBytes, _Font->m_TexBytes + desc.Width*y, desc.Width);
		}
		texUpload.Unmap();

		m_Font = _Font;
		m_FontResource = CreateGPUCopy(texUpload, &desc);
		D3D12_SHADER_RESOURCE_VIEW_DESC textureDesc;
		textureDesc.Format = desc.Format;
		textureDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		textureDesc.Shader4ComponentMapping = D3D12_ENCODE_SHADER_4_COMPONENT_MAPPING(
			D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1, 
			D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1, 
			D3D12_SHADER_COMPONENT_MAPPING_FORCE_VALUE_1, 
			D3D12_SHADER_COMPONENT_MAPPING_FROM_MEMORY_COMPONENT_0);
		textureDesc.Texture2D.MostDetailedMip = 0;
		textureDesc.Texture2D.MipLevels = 1;
		textureDesc.Texture2D.PlaneSlice = 0;
		textureDesc.Texture2D.ResourceMinLODClamp = 0;

		m_D3DDev->CreateShaderResourceView(m_FontResource, &textureDesc, m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	}

	if (textObj->m_BgVertexBuffer) { m_ResourcesToFree.push_back(textObj->m_BgVertexBuffer); textObj->m_BgVertexBuffer = NULL; }
	if (textObj->m_TextVertexBuffer) { m_ResourcesToFree.push_back(textObj->m_TextVertexBuffer); textObj->m_TextVertexBuffer = NULL; }
	int nbTextVerts = 0;
	int line;
	for (line = 0; line < _NbLines; ++line)
		nbTextVerts += 6 * (int)_TextLines[line].length();
	int nbBgVerts = 0;
	if (_BgWidth > 0)
		nbBgVerts = _NbLines * 6;

	textObj->m_LineColors = (_LineColors != NULL);
	textObj->m_LineBgColors = (_LineBgColors != NULL);

	// (re)create text vertex buffer if needed, and map it
	UploadBuffer textVertsUpload;
	CTextVtx *textVerts = NULL;
	if (nbTextVerts > 0)
	{
		textVertsUpload = AllocUploadBuffer(nbTextVerts * sizeof(CTextVtx));
		textVerts = (CTextVtx *)textVertsUpload.Map();
	}

	UploadBuffer bgVertsUpload;
	CLineRectVtx *bgVerts = NULL;
	if (nbBgVerts > 0)
	{
		bgVertsUpload = AllocUploadBuffer(nbBgVerts * sizeof(CLineRectVtx));
		bgVerts = (CLineRectVtx *)bgVertsUpload.Map();
	}

	int x, x1, y, y1, i, len;
	float px, px1, py, py1;
	unsigned char ch;
	const unsigned char *text;
	color32 lineColor = COLOR32_RED;
	CTextVtx vtx;
	vtx.m_Pos[2] = 0;
	CLineRectVtx bgVtx;
	bgVtx.m_Pos[2] = 0;
	int textVtxIndex = 0;
	int bgVtxIndex = 0;
	for (line = 0; line < _NbLines; ++line)
	{
		x = 0;
		y = line * (_Font->m_CharHeight + _Sep);
		y1 = y + _Font->m_CharHeight;
		len = (int)_TextLines[line].length();
		text = (const unsigned char *)(_TextLines[line].c_str());
		if (_LineColors != NULL)
			lineColor = ToR8G8B8A8(_LineColors[line]);

		if (textVerts != NULL)
			for (i = 0; i < len; ++i)
			{
				ch = text[i];
				x1 = x + _Font->m_CharWidth[ch];

				px = ToNormScreenX(x, m_WndWidth);
				py = ToNormScreenY(y, m_WndHeight);
				px1 = ToNormScreenX(x1, m_WndWidth);
				py1 = ToNormScreenY(y1, m_WndHeight);

				vtx.m_Color = lineColor;

				vtx.m_Pos[0] = px;
				vtx.m_Pos[1] = py;
				vtx.m_UV[0] = _Font->m_CharU0[ch];
				vtx.m_UV[1] = _Font->m_CharV0[ch];
				textVerts[textVtxIndex++] = vtx;

				vtx.m_Pos[0] = px1;
				vtx.m_Pos[1] = py;
				vtx.m_UV[0] = _Font->m_CharU1[ch];
				vtx.m_UV[1] = _Font->m_CharV0[ch];
				textVerts[textVtxIndex++] = vtx;

				vtx.m_Pos[0] = px;
				vtx.m_Pos[1] = py1;
				vtx.m_UV[0] = _Font->m_CharU0[ch];
				vtx.m_UV[1] = _Font->m_CharV1[ch];
				textVerts[textVtxIndex++] = vtx;

				vtx.m_Pos[0] = px1;
				vtx.m_Pos[1] = py;
				vtx.m_UV[0] = _Font->m_CharU1[ch];
				vtx.m_UV[1] = _Font->m_CharV0[ch];
				textVerts[textVtxIndex++] = vtx;

				vtx.m_Pos[0] = px1;
				vtx.m_Pos[1] = py1;
				vtx.m_UV[0] = _Font->m_CharU1[ch];
				vtx.m_UV[1] = _Font->m_CharV1[ch];
				textVerts[textVtxIndex++] = vtx;

				vtx.m_Pos[0] = px;
				vtx.m_Pos[1] = py1;
				vtx.m_UV[0] = _Font->m_CharU0[ch];
				vtx.m_UV[1] = _Font->m_CharV1[ch];
				textVerts[textVtxIndex++] = vtx;

				x = x1;
			}

		if (_BgWidth > 0 && bgVerts != NULL)
		{
			if (_LineBgColors != NULL)
				bgVtx.m_Color = ToR8G8B8A8(_LineBgColors[line]);
			else
				bgVtx.m_Color = ToR8G8B8A8(COLOR32_BLACK);

			px = ToNormScreenX(-1, m_WndWidth);
			py = ToNormScreenY(y, m_WndHeight);
			px1 = ToNormScreenX(_BgWidth + 1, m_WndWidth);
			py1 = ToNormScreenY(y1, m_WndHeight);

			bgVtx.m_Pos[0] = px;
			bgVtx.m_Pos[1] = py;
			bgVerts[bgVtxIndex++] = bgVtx;

			bgVtx.m_Pos[0] = px1;
			bgVtx.m_Pos[1] = py;
			bgVerts[bgVtxIndex++] = bgVtx;

			bgVtx.m_Pos[0] = px;
			bgVtx.m_Pos[1] = py1;
			bgVerts[bgVtxIndex++] = bgVtx;

			bgVtx.m_Pos[0] = px1;
			bgVtx.m_Pos[1] = py;
			bgVerts[bgVtxIndex++] = bgVtx;

			bgVtx.m_Pos[0] = px1;
			bgVtx.m_Pos[1] = py1;
			bgVerts[bgVtxIndex++] = bgVtx;

			bgVtx.m_Pos[0] = px;
			bgVtx.m_Pos[1] = py1;
			bgVerts[bgVtxIndex++] = bgVtx;
		}
	}
	assert(textVtxIndex == nbTextVerts);
	assert(bgVtxIndex == nbBgVerts);

	textObj->m_NbTextVerts = textVtxIndex;
	textObj->m_NbBgVerts = bgVtxIndex;
	if (textVerts)
	{
		textVertsUpload.Unmap();
		textObj->m_TextVertexBuffer = CreateGPUCopy(textVertsUpload);
	}

	if (bgVerts)
	{
		bgVertsUpload.Unmap();
		textObj->m_BgVertexBuffer = CreateGPUCopy(bgVertsUpload);
	}
}

void CTwGraphDirect3D12::DrawText(void *_TextObj, int _X, int _Y, color32 _Color, color32 _BgColor)
{
	assert(m_Drawing == true);
	assert(m_D3DGraphCmdList != NULL);
	assert(_TextObj != NULL);

	CTextObj *textObj = (CTextObj *)_TextObj;
	float dx = 2.0f*(float)(_X + m_OffsetX) / m_WndWidth;
	float dy = -2.0f*(float)(_Y + m_OffsetY) / m_WndHeight;

	if (textObj->m_BgVertexBuffer)
	{
		CConstants rs = {
			{ dx,dy,0,0 },
			{ 1,1,1,1 }
		};
		Color32ToARGBf(_BgColor, rs.m_CstColor + 3, rs.m_CstColor + 0, rs.m_CstColor + 1, rs.m_CstColor + 2);

		m_D3DGraphCmdList->SetGraphicsRoot32BitConstants(0, sizeof(rs) / sizeof(uint32_t), &rs, 0);

		if (_BgColor != 0 || !textObj->m_LineBgColors) // use a constant bg color
			m_D3DGraphCmdList->SetPipelineState(m_TriCstColor_PSO);
		else
			m_D3DGraphCmdList->SetPipelineState(m_Tri_PSO);

		D3D12_VERTEX_BUFFER_VIEW vbView;
		vbView.BufferLocation = textObj->m_BgVertexBuffer->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(CLineRectVtx)*textObj->m_NbBgVerts;
		vbView.StrideInBytes = sizeof(CLineRectVtx);

		m_D3DGraphCmdList->IASetVertexBuffers(0, 1, &vbView);
		m_D3DGraphCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_D3DGraphCmdList->DrawInstanced(textObj->m_NbBgVerts, 1, 0, 0);

	}

	if (textObj->m_TextVertexBuffer)
	{
		CConstants rs = {
			{ dx,dy,0,0 },
			{ 1,1,1,1 }
		};
		Color32ToARGBf(_Color, rs.m_CstColor + 3, rs.m_CstColor + 0, rs.m_CstColor + 1, rs.m_CstColor + 2);

		m_D3DGraphCmdList->SetGraphicsRoot32BitConstants(0, sizeof(rs) / sizeof(uint32_t), &rs, 0);

		if (_Color != 0 || !textObj->m_LineColors) // use a constant color
			m_D3DGraphCmdList->SetPipelineState(m_TextCstColor_PSO);
		else
			m_D3DGraphCmdList->SetPipelineState(m_Text_PSO);

		D3D12_VERTEX_BUFFER_VIEW vbView;
		vbView.BufferLocation = textObj->m_TextVertexBuffer->GetGPUVirtualAddress();
		vbView.SizeInBytes = sizeof(CTextVtx)*textObj->m_NbTextVerts;
		vbView.StrideInBytes = sizeof(CTextVtx);

		m_D3DGraphCmdList->IASetVertexBuffers(0, 1, &vbView);
		m_D3DGraphCmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		m_D3DGraphCmdList->SetDescriptorHeaps(1, &m_srvDescriptorHeap);
		m_D3DGraphCmdList->SetGraphicsRootDescriptorTable(1, m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());

		m_D3DGraphCmdList->DrawInstanced(textObj->m_NbTextVerts, 1, 0, 0);
	}
}