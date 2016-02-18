#pragma once
//  ---------------------------------------------------------------------------
//
//  @file       TwDirect3D11.h
//  @brief      Direct3D11 graphic functions
//  @author     Johan Kohler
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  note:       Private header
//
//  ---------------------------------------------------------------------------

#if !defined ANT_TW_DIRECT3D12_INCLUDED
#define ANT_TW_DIRECT3D12_INCLUDED

#include "TwGraph.h"

//  ---------------------------------------------------------------------------



class CTwGraphDirect3D12 : public ITwGraph
{
public:
	virtual int                 Init();
	virtual int                 Shut();
	virtual void                BeginDraw(int _WndWidth, int _WndHeight);
	virtual void                EndDraw();
	virtual bool                IsDrawing();
	virtual void                Restore();
	virtual void                DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color0, color32 _Color1, bool _AntiAliased = false);
	virtual void                DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color, bool _AntiAliased = false) { DrawLine(_X0, _Y0, _X1, _Y1, _Color, _Color, _AntiAliased); }
	virtual void                DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color00, color32 _Color10, color32 _Color01, color32 _Color11);
	virtual void                DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color) { DrawRect(_X0, _Y0, _X1, _Y1, _Color, _Color, _Color, _Color); }
	virtual void                DrawTriangles(int _NumTriangles, int *_Vertices, color32 *_Colors, Cull _CullMode);

	virtual void *              NewTextObj();
	virtual void                DeleteTextObj(void *_TextObj);
	virtual void                BuildText(void *_TextObj, const std::string *_TextLines, color32 *_LineColors, color32 *_LineBgColors, int _NbLines, const CTexFont *_Font, int _Sep, int _BgWidth);
	virtual void                DrawText(void *_TextObj, int _X, int _Y, color32 _Color, color32 _BgColor);

	virtual void                ChangeViewport(int _X0, int _Y0, int _Width, int _Height, int _OffsetX, int _OffsetY);
	virtual void                RestoreViewport();
	virtual void                SetScissor(int _X0, int _Y0, int _Width, int _Height);

protected:
	struct ID3D12Device *       m_D3DDev;
	unsigned int                m_D3DDevInitialRefCount;

	struct ID3D12GraphicsCommandList * m_D3DGraphCmdList;

	bool                        m_Drawing;
	int                         m_WndWidth;
	int                         m_WndHeight;
	int                         m_OffsetX;
	int                         m_OffsetY;

	RECT                        m_ViewportAndScissorRects[2];

	struct ID3D12PipelineState *m_Line_PSO;
	struct ID3D12PipelineState *m_Line_AA_PSO;
	struct ID3D12PipelineState *m_Tri_PSO;
	struct ID3D12PipelineState *m_Tri_CW_PSO;
	struct ID3D12PipelineState *m_Tri_CCW_PSO;
	struct ID3D12PipelineState *m_TriCstColor_PSO;
	struct ID3D12PipelineState *m_Text_PSO;
	struct ID3D12PipelineState *m_TextCstColor_PSO;
	struct ID3D12RootSignature *m_RootSignature;

	struct ID3D12DescriptorHeap*m_srvDescriptorHeap;

	std::vector<struct ID3D12Resource *> m_ResourcesToFree;
	std::vector<struct ID3D12Resource *> m_UploadResourcesFree;
	std::vector<struct ID3D12Resource *> m_UploadResourcesFreeThisFrame;
	struct ID3D12Resource *     m_UploadResource;
	uint32_t                    m_UploadResourceSize;
	uint32_t                    m_UploadResourceUsed;

	const CTexFont *            m_Font;
	struct ID3D12Resource *     m_FontResource;

	struct CLineRectVtx
	{
		float                   m_Pos[3];
		color32                 m_Color;
	};
	struct CTextVtx
	{
		float                   m_Pos[3];
		color32                 m_Color;
		float                   m_UV[2];
	};
	struct CConstants
	{
		float                   m_Offset[4];
		float                   m_CstColor[4];
	};

	struct CTextObj
	{
		struct ID3D12Resource * m_TextVertexBuffer;
		struct ID3D12Resource * m_BgVertexBuffer;
		int                     m_NbTextVerts;
		int                     m_NbBgVerts;
		bool                    m_LineColors;
		bool                    m_LineBgColors;
	};

	struct UploadBuffer
	{
		struct ID3D12Resource * resource;
		uint32_t                offset;
		uint32_t                bytes;
		void *                  Map();
		void                    Unmap();
		uint64_t                GpuAddr();
	};

	std::vector<std::pair<UploadBuffer, struct ID3D12Resource *>> deferedCopying;
	UploadBuffer                AllocUploadBuffer(uint32_t bytes,uint32_t align=16);
	struct ID3D12Resource *     CreateGPUCopy(UploadBuffer &upload, struct D3D12_RESOURCE_DESC *inDesc = nullptr );
};

#endif
