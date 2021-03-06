#include "DirectX11.h"
#include <math.h>
#include <vector>
#include <fstream>
#include "DX_Texture.h"
#include "IFont.h"
#include "Common.h"

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
	"	return col*In.color;"
	"}" 


	);


MY_UI_Too::DirectX11::DirectX11(ID3D11Device* pDevice, ID3D11DeviceContext* context ){
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
	memset(LastRTVs, 0, sizeof(LastRTVs));

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
	ResetRT=false;
}

MY_UI_Too::DirectX11::~DirectX11(){		
	DeInit();
}

bool MY_UI_Too::DirectX11::Init(){
	MY_UI_Too::Renderer::Init();//call base init
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
	sampledesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
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
	return true;
}
void MY_UI_Too::DirectX11::DeInit(){

	OUTPUT_DEBUG_MSG("Shuttng down Renderer");
	for(unsigned int i=0; i<8;i++) RELEASECOM(LastRTVs[i]);
	RELEASECOM(LastDTV);

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
void MY_UI_Too::DirectX11::Get_Render_States(){
	
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
}
void MY_UI_Too::DirectX11::Restore_Render_States(){
	if(ResetRT){
		UINT views = 1;
		DeviceContext->RSSetViewports(views, &LastViewPort);
		DeviceContext->OMSetRenderTargets(8, LastRTVs, LastDTV);
		for(unsigned int i=0; i<8;i++) RELEASECOM(LastRTVs[i]);
		RELEASECOM(LastDTV);
	}
	ResetRT=false;
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
void MY_UI_Too::DirectX11::Set_Render_Target(MY_UI_Too::Interfaces::ITexture* texture){
	if(texture!=nullptr){
		assert(texture->Get_Render_Texture()!=nullptr);
		UINT views = 1;
		DeviceContext->RSGetViewports(&views, &LastViewPort);
		DeviceContext->OMGetRenderTargets(8, LastRTVs, &LastDTV);

		D3D11_VIEWPORT vp;
		memset(&vp, 0, sizeof(D3D11_VIEWPORT));
		Utilities::Point p=texture->Get_Dimensions();
		vp.Height = (FLOAT)p.top;
		vp.Width = (FLOAT)p.left;
		vp.MaxDepth = 1;
		DeviceContext->RSSetViewports(1, &vp);
		ID3D11RenderTargetView* ptrs[1] = { (ID3D11RenderTargetView*)texture->Get_Render_Texture()};
		DeviceContext->OMSetRenderTargets(1, ptrs, NULL);
		ResetRT=true;
	}
}


void MY_UI_Too::DirectX11::Draw(MyDrawState& drawstate){
	if(drawstate.changed){
		D3D11_MAPPED_SUBRESOURCE sb;
		memset(&sb, 0, sizeof(D3D11_MAPPED_SUBRESOURCE));

		HR(DeviceContext->Map(UIVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sb));
		memcpy( sb.pData, &drawstate.Verts[0] , drawstate.NumVerts * sizeof( VERTEXFORMAT2D ) );
		DeviceContext->Unmap(UIVertexBuffer, 0);
	}
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

MY_UI_Too::Interfaces::ITexture* MY_UI_Too::DirectX11::LoadTexture(std::string filename, bool as_rendertarget){	
	if(!FileExists(filename)) return nullptr;

	D3DX11_IMAGE_INFO finfo;
	memset(&finfo, 0, sizeof(D3DX11_IMAGE_INFO));
	HR(D3DX11GetImageInfoFromFileA(filename.c_str(), 0, &finfo, 0));
	Interfaces::ITexture* tex = new Utilities::DX_Texture();
	tex->Set_Dimensions(Utilities::Point(finfo.Width, finfo.Height));

	if(as_rendertarget){
		ID3D11Resource* Texture_=nullptr;
		D3D11_TEXTURE2D_DESC desc;
		desc.Width  = (UINT)finfo.Width;
		desc.Height = (UINT) finfo.Height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.MipLevels = 1;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		desc.CPUAccessFlags = 0;
		desc.ArraySize = 1;
		desc.MiscFlags = 0;
		HR(Device->CreateTexture2D(&desc, NULL, (ID3D11Texture2D **) &Texture_));
		D3D11_RESOURCE_DIMENSION type;
		Texture_->GetType(&type);
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		memset(&rtvDesc, 0, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
		ID3D11RenderTargetView *rtv=nullptr;

		rtvDesc.Format = desc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		HR(Device->CreateRenderTargetView(Texture_, &rtvDesc, &rtv));


		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
		memset(&srvDesc, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
		ID3D11ShaderResourceView *srv=nullptr;
		srvDesc.Format =  desc.Format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = desc.MipLevels;
		HR(Device->CreateShaderResourceView(Texture_, &srvDesc, &srv));
		tex->Set_Texture(srv);
		tex->Set_Render_Texture(rtv);
		RELEASECOM(Texture_);// this isnt needed. The srv and rtv hold the texture
	} else {
		ID3D11ShaderResourceView *resource(0);
		HR(D3DX11CreateShaderResourceViewFromFileA( Device, filename.c_str(), 0, 0, &resource, 0));
		tex->Set_Texture(resource);
	}
	return tex;
}

MY_UI_Too::Interfaces::ITexture* MY_UI_Too::DirectX11::CreateTexture(unsigned int width, unsigned int height,Utilities::Color* buffer, bool as_rendertarget){	

	Interfaces::ITexture* tex = new Utilities::DX_Texture();
	tex->Set_Dimensions(Utilities::Point(width, height));

	ID3D11Resource* Texture_=nullptr;
	D3D11_TEXTURE2D_DESC desc;
	desc.Width  = (UINT)width;
	desc.Height = (UINT)height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.MipLevels = 1;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	UINT flags = D3D11_BIND_SHADER_RESOURCE;
	if(as_rendertarget) flags |=D3D11_BIND_RENDER_TARGET;
	desc.BindFlags = flags;
	desc.CPUAccessFlags = 0;
	desc.ArraySize = 1;
	desc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA dat, *data(NULL);
	dat.SysMemSlicePitch=0;
	dat.pSysMem = buffer;
	dat.SysMemPitch = width*sizeof(Utilities::Color);
	if(buffer) data = &dat;
	HR(Device->CreateTexture2D(&desc, data, (ID3D11Texture2D **) &Texture_));

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	memset(&srvDesc, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	ID3D11ShaderResourceView *srv=nullptr;
	srvDesc.Format =  desc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	HR(Device->CreateShaderResourceView(Texture_, &srvDesc, &srv));
	tex->Set_Texture(srv);

	if(as_rendertarget){
		D3D11_RESOURCE_DIMENSION type;
		Texture_->GetType(&type);
		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		memset(&rtvDesc, 0, sizeof(D3D11_RENDER_TARGET_VIEW_DESC));
		ID3D11RenderTargetView *rtv=nullptr;

		rtvDesc.Format = desc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;
		HR(Device->CreateRenderTargetView(Texture_, &rtvDesc, &rtv));
		tex->Set_Render_Texture(rtv);
		float colors[] = {1, 1, 1, 0};
		DeviceContext->ClearRenderTargetView(rtv, colors);
	}
	RELEASECOM(Texture_);// this isnt needed. The srv and rtv hold the texture
	return tex;
}
