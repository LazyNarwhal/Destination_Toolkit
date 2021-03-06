

#include "DirectX11.h"
#include <math.h>
#include <vector>
#include <fstream>
#include "cWidgetSkin.h"

const std::string UIShader = std::string(
	"SamplerState sampler0;"
	"struct PSstruct {"
	"	float4 position : SV_Position;"
	"	float4 color    : COLOR;    "
	"	float2 texcoord : TexCoord;"
	"};"
	"Texture2D UITexture;"
	"PSstruct VS(float2 position : POSITION, float4 color: COLOR, float2 texcoord: TEXCOORD) { "
	"PSstruct Out;" 
	"Out.position = float4(position.xy, 1, 1);"
	"Out.color = color;"
	"Out.texcoord = texcoord;" 
	"return Out;" 
	"}"
	"float4 PS( PSstruct In ) : SV_Target0 {"
	"	float4 col = UITexture.Sample( sampler0, In.texcoord );"
	"    return col*In.color; "
	"}" 


	);


MY_UI::DirectX11::DirectX11(ID3D11Device* pDevice, ID3D11DeviceContext* context ){
	Device = pDevice;
	DeviceContext=context;
	UIInputLayout=0;
	UIPSShader=0;
	UIVSShader=0;
	UIVertexBuffer=0;
	UILastBlendState=UIBlendState=0;
	UISampler=0;
	UIDepthState=0;
	LastInputLayout=0;
	memset(LastBuffers, 0, sizeof(LastBuffers));
	memset(LastStrides, 0, sizeof(LastStrides));
	memset(LastOffsets, 0, sizeof(LastOffsets));
	memset(LastSOBuffers, 0, sizeof(LastSOBuffers));

	LastIndexBuffer=0;
	LastIndexBufferFormat=DXGI_FORMAT_R32_UINT;
	LastIndexBufferOffset=0;

	LastPSShader=0;
	LastVSShader=0;
	LastGSShader=0;
	LastHSShader=0;
	LastDSShader=0;
	LastPSSampler=0;

	LastDepthState=0;
	LastRasterizerState=UIRasterizerStateNormal=0;
	UIIndexBuffer=0;
	Draw_State_Index=0;
}

MY_UI::DirectX11::~DirectX11(){		
	DeInit();

}

bool MY_UI::DirectX11::Init(){
	ID3DBlob* blob=0;

	HR(CompileShaderFromMemory(UIShader, "PS", "ps_4_0", &blob)); 

	HR(Device->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &UIPSShader));
	RELEASECOM(blob);
	HR(CompileShaderFromMemory(UIShader, "VS", "vs_4_0", &blob)); 
	HR(Device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), 0, &UIVSShader));


	const D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,		0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0,		8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,       12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	int numElements = sizeof( layout ) / sizeof( layout[0] );

	HR( Device->CreateInputLayout( layout, numElements, blob->GetBufferPointer(), blob->GetBufferSize(), &UIInputLayout ) );
	RELEASECOM(blob);// release the VS blob after the input layout has been made

	//create an empty, dynamic vertex buffer
	D3D11_BUFFER_DESC vbdesc;
	vbdesc.ByteWidth = VERT_BUFFER_SIZE * sizeof( VERTEXFORMAT2D );
	vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbdesc.Usage = D3D11_USAGE_DYNAMIC;
	vbdesc.MiscFlags =0;
	HR(Device->CreateBuffer( &vbdesc, 0, &UIVertexBuffer));


	// create a rast state to cull none and fill solid. Also create the scissor rast state
	D3D11_RASTERIZER_DESC descras;
	descras.CullMode = (D3D11_CULL_MODE) D3D11_CULL_NONE;
	descras.FillMode = (D3D11_FILL_MODE) D3D11_FILL_SOLID;
	descras.FrontCounterClockwise = FALSE;
	descras.DepthBias = 0;
	descras.DepthBiasClamp = 0.0f;
	descras.SlopeScaledDepthBias = 0.0f;
	descras.AntialiasedLineEnable = FALSE;
	descras.DepthClipEnable = FALSE;
	descras.MultisampleEnable = (BOOL) false;
	descras.ScissorEnable = (BOOL) false;
	HR(Device->CreateRasterizerState(&descras, &UIRasterizerStateNormal));

	D3D11_BLEND_DESC blenddesc;
	blenddesc.AlphaToCoverageEnable = (BOOL) false;
	blenddesc.IndependentBlendEnable = (BOOL) false;
	blenddesc.RenderTarget[0].BlendOp = (D3D11_BLEND_OP) D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlend = (D3D11_BLEND) D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlend = (D3D11_BLEND) D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendOpAlpha = (D3D11_BLEND_OP) D3D11_BLEND_OP_ADD;
	blenddesc.RenderTarget[0].SrcBlendAlpha = (D3D11_BLEND) D3D11_BLEND_SRC_ALPHA;
	blenddesc.RenderTarget[0].DestBlendAlpha = (D3D11_BLEND) D3D11_BLEND_INV_SRC_ALPHA;
	blenddesc.RenderTarget[0].BlendEnable= true;
	blenddesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	HR(Device->CreateBlendState(&blenddesc, &UIBlendState));



	D3D11_SAMPLER_DESC sampledesc;
	sampledesc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
	sampledesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampledesc.AddressW = sampledesc.AddressV = sampledesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	sampledesc.MipLODBias = 0.0f;
	sampledesc.MaxAnisotropy = 1;

	sampledesc.BorderColor[0] = 0.0f;
	sampledesc.BorderColor[1] = 0.0f;
	sampledesc.BorderColor[2] = 0.0f;
	sampledesc.BorderColor[3] = 0.0f;

	sampledesc.MinLOD = -FLT_MAX;
	sampledesc.MaxLOD = FLT_MAX;
	HR(Device->CreateSamplerState(&sampledesc, &UISampler));




	D3D11_DEPTH_STENCIL_DESC depthdesc;
	memset(&depthdesc, 0, sizeof(depthdesc));
	depthdesc.DepthEnable = (BOOL) false;
	depthdesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthdesc.DepthFunc = D3D11_COMPARISON_NEVER;
	depthdesc.StencilEnable = (BOOL) false;

	HR(Device->CreateDepthStencilState(&depthdesc, &UIDepthState));
	std::vector<unsigned int> indexbuffer;
	unsigned int currindex =0;
	indexbuffer.resize((VERT_BUFFER_SIZE/4)*6 );
	for(unsigned int i=0; i<indexbuffer.size(); i+=6){
		indexbuffer[i + 0] = currindex;
		indexbuffer[i + 1] = currindex + 1;
		indexbuffer[i + 2] = currindex + 2;
		indexbuffer[i + 3] = currindex + 1;
		indexbuffer[i + 4] = currindex + 3;
		indexbuffer[i + 5] = currindex + 2;
		currindex+=4;
	}

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = (UINT)(sizeof(unsigned int) * indexbuffer.size());
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	D3D11_SUBRESOURCE_DATA iinitData;
	iinitData.pSysMem = &indexbuffer[0];
	HR(Device->CreateBuffer(&ibd, &iinitData, &UIIndexBuffer));
	Draw_States.resize(1);// at least one
	return true;
}
void MY_UI::DirectX11::DeInit(){
	RELEASECOM(UIInputLayout);
	RELEASECOM(UISampler);

	RELEASECOM(UIPSShader);
	RELEASECOM(UIVSShader);
	RELEASECOM(UIVertexBuffer);

	RELEASECOM(UIBlendState);
	RELEASECOM(UIIndexBuffer);	
	RELEASECOM(UIDepthState);
	RELEASECOM(UIRasterizerStateNormal);

}

void MY_UI::DirectX11::Begin()// get the states that this will change to set at the end call
{
	////GET ALL THE SET STATES so they can be set after the UI is finished drawing
	DeviceContext->OMGetBlendState(&UILastBlendState, LastBlendFactor, &LastBlendMask);
	DeviceContext->RSGetState(&LastRasterizerState);
	DeviceContext->OMGetDepthStencilState(&LastDepthState, &LastStencilRef);
	DeviceContext->IAGetInputLayout(&LastInputLayout);
	DeviceContext->IAGetPrimitiveTopology(&LastTopology);
	DeviceContext->IAGetVertexBuffers(0, 8, LastBuffers, LastStrides, LastOffsets);
	DeviceContext->IAGetIndexBuffer(&LastIndexBuffer, &LastIndexBufferFormat, &LastIndexBufferOffset);
	DeviceContext->SOGetTargets(4, LastSOBuffers);

	DeviceContext->PSGetSamplers(0, 1, &LastPSSampler);
	DeviceContext->PSGetShader(&LastPSShader,0,0);
	DeviceContext->GSGetShader(&LastGSShader,0,0);
	DeviceContext->VSGetShader(&LastVSShader,0,0);
	DeviceContext->DSGetShader(&LastDSShader,0,0);
	DeviceContext->HSGetShader(&LastHSShader,0,0);

	// make sure to set these to null 
	DeviceContext->GSSetShader(0,0,0);
	DeviceContext->DSSetShader(0,0,0);
	DeviceContext->HSSetShader(0,0,0);

	///set the needed UI states
	float factor[4] = {0, 0, 0, 0};
	DeviceContext->OMSetBlendState(UIBlendState, factor, ~0);
	DeviceContext->OMSetDepthStencilState(UIDepthState, 0);
	DeviceContext->RSSetState( UIRasterizerStateNormal);
	DrawCalls =0;

}

void MY_UI::DirectX11::End(){// reset the device to its original state{

	// set all the previous states back 
	DeviceContext->OMSetBlendState(UILastBlendState, LastBlendFactor, LastBlendMask);
	RELEASECOM(UILastBlendState);
	DeviceContext->RSSetState(LastRasterizerState);
	RELEASECOM(LastRasterizerState);
	DeviceContext->OMSetDepthStencilState(LastDepthState, LastStencilRef);
	RELEASECOM(LastDepthState);
	DeviceContext->IASetInputLayout(LastInputLayout);
	RELEASECOM(LastInputLayout);
	DeviceContext->IASetPrimitiveTopology(LastTopology);
	DeviceContext->IASetVertexBuffers(0, 8, LastBuffers, LastStrides, LastOffsets);
	for(unsigned int i=0; i<8;i++) RELEASECOM(LastBuffers[i]);

	DeviceContext->IASetIndexBuffer(LastIndexBuffer, LastIndexBufferFormat, LastIndexBufferOffset);
	RELEASECOM(LastIndexBuffer);
	UINT temp[8] = {0};
	DeviceContext->SOSetTargets(4, LastSOBuffers, temp);
	for(unsigned int i=0; i<4;i++) RELEASECOM(LastSOBuffers[i]);
	DeviceContext->PSSetShader(LastPSShader,0,0);
	RELEASECOM(LastPSShader);
	
	DeviceContext->PSSetSamplers(0, 1, &LastPSSampler);
	RELEASECOM(LastPSSampler);
	DeviceContext->GSSetShader(LastGSShader,0,0);
	RELEASECOM(LastGSShader);
	DeviceContext->VSSetShader(LastVSShader,0,0);
	RELEASECOM(LastVSShader);
	DeviceContext->DSSetShader(LastDSShader,0,0);
	RELEASECOM(LastDSShader);
	DeviceContext->HSSetShader(LastHSShader,0,0);
	RELEASECOM(LastHSShader);
}

void MY_UI::DirectX11::DrawBufferd(){	
	for(size_t i(0); i < Draw_State_Index +1; i++){
		if(Draw_States[i].NumVerts == 0) continue;
		Draw(Draw_States[i]);
	}
}
void MY_UI::DirectX11::DrawBack(bool popback){
	Draw(Draw_States[ Draw_State_Index ]);
	if(popback){
		Draw_States[ Draw_State_Index ].NumVerts=0;
		Draw_State_Index -= 1;
		CurrentTexture = Draw_States[ Draw_State_Index ].texture;//reset this
	}
}
void MY_UI::DirectX11::Draw(const MyDrawState& drawstate){
	++DrawCalls;

	D3D11_MAPPED_SUBRESOURCE sb;
	memset(&sb, 0, sizeof(D3D11_MAPPED_SUBRESOURCE));

	HR(DeviceContext->Map(UIVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sb));
	memcpy( sb.pData, &drawstate.Verts[0] , drawstate.NumVerts * sizeof( VERTEXFORMAT2D ) );
	DeviceContext->Unmap(UIVertexBuffer, 0);

	DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	DeviceContext->IASetInputLayout(UIInputLayout);

	DeviceContext->VSSetShader(UIVSShader, 0, 0);
	DeviceContext->PSSetShader(UIPSShader, 0, 0);
	DeviceContext->PSSetSamplers(0, 1, &UISampler); 
	DeviceContext->PSSetShaderResources(0, 1, (ID3D11ShaderResourceView* const *)&drawstate.texture);
	
	UINT stride [] = { sizeof(VERTEXFORMAT2D) };
	UINT offset [] = { 0 };
	// set the vertex buffer
	DeviceContext->IASetIndexBuffer(UIIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	DeviceContext->IASetVertexBuffers(0, 1, &UIVertexBuffer, stride, offset);
	// apply the correct technique 

	DeviceContext->DrawIndexed((drawstate.NumVerts/4)*6, 0, 0);
}
void MY_UI::DirectX11::GotoNextBufferSlot(){		
	if(Draw_States.size() <= Draw_State_Index +1)  Draw_States.resize(Draw_States.size() + 16);// add more space if needed
	Draw_States[ Draw_State_Index ].Verts.resize(Draw_States[ Draw_State_Index ].NumVerts);// resize the buffer in case there is hardly any data inside
	Draw_State_Index+=1;// goto the next index
}
void MY_UI::DirectX11::AddVert( float x, float y, float u, float v, Utilities::Color col){
	if ( Draw_States[ Draw_State_Index ].NumVerts >= VERT_BUFFER_SIZE) GotoNextBufferSlot();

	MyDrawState& drawstate = Draw_States[ Draw_State_Index ];
	VERTEXFORMAT2D& vert = drawstate.Verts[drawstate.NumVerts];
	vert.x = (x*Inv_Wndx) -1.0f;
	vert.y = (y*Inv_Wndy) +1.0f;
	vert.u = u;
	vert.v = v;	
	vert.color = col.color;
	drawstate.NumVerts+=1;
}
bool MY_UI::DirectX11::SetTexture(MY_UI::Utilities::Texture& pTexture, bool drawingnow){
	void* pImage = pTexture.Tex;
	if ( pImage ==0) return false;// Missing image, not loaded properly?
	if( (CurrentTexture!= pImage) | (drawingnow)){
		GotoNextBufferSlot();
	}
	Draw_States[ Draw_State_Index ].texture = pImage;
	CurrentTexture =pImage;

	return true;
}

void MY_UI::DirectX11::DrawTexturedRect_Clip(MY_UI::Utilities::Texture& pTexture, MY_UI::Utilities::UVs& uvs, MY_UI::Utilities::Rect rect,  MY_UI::Utilities::Color color_tl, MY_UI::Utilities::Color color_tr, MY_UI::Utilities::Color color_bl, MY_UI::Utilities::Color color_br, bool drawnow){
	if(!SetTexture(pTexture, drawnow)) return;

	Utilities::Point tl(rect.left, rect.top);
	Utilities::Point tr(rect.left + rect.width, rect.top);
	Utilities::Point bl(rect.left, rect.top + rect.height);
	Utilities::Point br(rect.left + rect.width, rect.top + rect.height);
	// if all the points are not within the cliprect, dont draw it
	bool brin = ClipRects.back().Within(br);
	bool trin = ClipRects.back().Within(tr);

	bool blin = ClipRects.back().Within(bl);
	bool tlin = ClipRects.back().Within(tl);

	if( (!brin) & (!trin) & (!blin) & (!tlin)) return;// all points are outside the cliprect


	float left= static_cast<float>(rect.left);
	float top = static_cast<float>(rect.top);
	float width = static_cast<float>(rect.width);
	float right = width + left;
	float height = static_cast<float>(rect.height);
	float bottom = height + top;
	// resize the buffer if needed
	if(Draw_States[ Draw_State_Index ].Verts.size() <= 4 + Draw_States[ Draw_State_Index ].NumVerts ) Draw_States[ Draw_State_Index ].Verts.resize(Draw_States[ Draw_State_Index ].Verts.size() + 200);
	if( (brin) & (trin) & (blin) & (tlin)){// all points are fully contained inside the cliprect

		AddVert( left, top,					uvs.u1, uvs.v1, color_tl );
		AddVert( right, top,				uvs.u2, uvs.v1, color_tr );
		AddVert( left, bottom,				uvs.u1, uvs.v2, color_bl );
		AddVert( right, bottom,				uvs.u2, uvs.v2, color_br );


	} else {// this means the rect is partially in the clip region. Use the cpu to clip it

		Utilities::Rect& r = ClipRects.back();
		float newleft= static_cast<float>(Utilities::Clamp<int>(rect.left, r.left, r.left + r.width));
		float newtop = static_cast<float>(Utilities::Clamp<int>(rect.top, r.top, r.top + r.height));
		float newright = static_cast<float>(Utilities::Clamp<int>(rect.width + rect.left, r.left, r.left + r.width));
		float newbottom = static_cast<float>(Utilities::Clamp<int>(rect.height + rect.top, r.top, r.top + r.height));

		float difleft = newleft - left;
		float diftop = newtop - top;
		float difright = newright - right;
		float difbottom = newbottom - bottom;

		difleft /= width;
		diftop /= height;
		difright /= width;
		difbottom /= height;

		float u1 = uvs.u1;
		float v1 = uvs.v1;
		float u2 = uvs.u2;
		float v2 = uvs.v2;

		float uwidth = u2 - u1;
		float vheight = v2 - v1;

		u1 = u1 + (uwidth*difleft);
		u2 = u2 + (uwidth*difright);
		v1 = v1 + (vheight*diftop);
		v2 = v2 + (vheight*difbottom);

		AddVert( newleft, newtop,					u1, v1, color_tl );
		AddVert( newright, newtop,					u2, v1, color_tr );
		AddVert( newleft, newbottom,				u1, v2, color_bl );
		AddVert( newright, newbottom,				u2, v2, color_br );

	}
}

void MY_UI::DirectX11::DrawTexturedRect_NoClip(MY_UI::Utilities::Texture& pTexture,  MY_UI::Utilities::UVs& uvs, MY_UI::Utilities::Rect rect,  MY_UI::Utilities::Color color_tl, MY_UI::Utilities::Color color_tr, MY_UI::Utilities::Color color_bl, MY_UI::Utilities::Color color_br, bool drawnow){
	if(!SetTexture(pTexture, drawnow)) return;

	float left= static_cast<float>(rect.left);
	float top = static_cast<float>(rect.top);
	float width = static_cast<float>(rect.width);
	float right = width + left;
	float height = static_cast<float>(rect.height);
	float bottom = height + top;
	if(Draw_States[ Draw_State_Index ].Verts.size() <= 4 + Draw_States[ Draw_State_Index ].NumVerts ) Draw_States[ Draw_State_Index ].Verts.resize(Draw_States[ Draw_State_Index ].Verts.size() + 200);
	AddVert( left, top,					uvs.u1, uvs.v1, color_tl );
	AddVert( right, top,				uvs.u2, uvs.v1, color_tr );
	AddVert( left, bottom,				uvs.u1, uvs.v2, color_bl );
	AddVert( right, bottom,				uvs.u2, uvs.v2, color_br );
}
void MY_UI::DirectX11::DrawLine_Clip(MY_UI::Utilities::Texture& pTexture, MY_UI::Utilities::Point beg, MY_UI::Utilities::Point end, int thickness, MY_UI::Utilities::Color color_beg, MY_UI::Utilities::Color color_end, bool drawnow){
	// havent implemented the clip version yet. . . .
	DrawLine_NoClip(pTexture, beg, end, thickness, color_beg, color_end, drawnow);
}

void MY_UI::DirectX11::DrawLine_NoClip(MY_UI::Utilities::Texture& pTexture, MY_UI::Utilities::Point beg, MY_UI::Utilities::Point end, int thickness, MY_UI::Utilities::Color color_beg, MY_UI::Utilities::Color color_end, bool drawnow){
	if(!SetTexture(pTexture, drawnow)) return;

	float beg_x= static_cast<float>(beg.x);
	float beg_y = static_cast<float>(beg.y);
	float end_x = static_cast<float>(end.x);
	float end_y = static_cast<float>(end.y);

	float otherx = end_x-beg_x;
	float othery = end_y-beg_y;

	// now find the normal, make sure to use the largest as the direction, but negaive
	if(othery > otherx) othery = -othery;
	else otherx = -otherx;

	//other is now the vector between beg and other
	float inv_mag = otherx*otherx + othery*othery;
	// no check for div by zero here!
	inv_mag = 1.0f/sqrt(inv_mag);
	otherx*=inv_mag;
	othery*=inv_mag;
	// the components must be swapped 
	float temp = otherx;
	otherx = othery;
	othery = temp;

	//other is now the normalized vector pointing perpendicular to both beg and end
	thickness>>=1;// divide by two since thickness is the total thickness
	float thick = static_cast<float>(thickness);
	float topleft_x = otherx*thick + beg_x;
	float topleft_y = othery*thick + beg_y;

	float botleft_x = -otherx*thick + beg_x;
	float botleft_y = -othery*thick + beg_y;

	float topright_x = otherx*thick + end_x;
	float topright_y = othery*thick + end_y;

	float botright_x = -otherx*thick + end_x;
	float botright_y = -othery*thick + end_y;
	if(Draw_States[ Draw_State_Index ].Verts.size() <= 4 + Draw_States[ Draw_State_Index ].NumVerts ) Draw_States[ Draw_State_Index ].Verts.resize(Draw_States[ Draw_State_Index ].Verts.size() + 200);

	AddVert( topleft_x, topleft_y,		pTexture.u1, pTexture.v1, color_beg );
	AddVert( topright_x, topright_y,	pTexture.u2, pTexture.v1, color_end );
	AddVert( botleft_x, botleft_y,		pTexture.u1, pTexture.v2, color_beg );
	AddVert( botright_x, botright_y,	pTexture.u2, pTexture.v2, color_end );

}


void MY_UI::DirectX11::StartClip(MY_UI::Utilities::Rect& rect){
	ClipRects.push_back(rect);
}

void MY_UI::DirectX11::EndClip(){
	ClipRects.pop_back();
}

bool MY_UI::DirectX11::LoadTexture( MY_UI::Utilities::Texture* pTexture ){	
	std::ifstream file(pTexture->FileName.c_str());
	if(!file) return false;// missingfile, dont attempt to load
	file.close();
	D3DX11_IMAGE_INFO finfo;
	memset(&finfo, 0, sizeof(D3DX11_IMAGE_INFO));
	HR(D3DX11GetImageInfoFromFileA(pTexture->FileName.c_str(), 0, &finfo, 0));

	ID3D11ShaderResourceView *resource(0);

	HR(D3DX11CreateShaderResourceViewFromFileA( Device, pTexture->FileName.c_str(), 0, 0, &resource, 0));
	pTexture->Tex = resource;// store this instead of the ID3D11Texture2D because a ID3D11Texture2D object can be retrived from ID3D11ShaderResourceView by calling GetResource
	pTexture->width = static_cast<float>(finfo.Width);
	pTexture->height = static_cast<float>(finfo.Height);

	return true;
}

void MY_UI::DirectX11::FreeTexture( MY_UI::Utilities::Texture* pTexture ){
	ID3D11ShaderResourceView* pImage = (ID3D11ShaderResourceView*) pTexture->Tex;
	if ( !pImage ) return;
	pImage->Release();
	pTexture->clear();
	return;
}
