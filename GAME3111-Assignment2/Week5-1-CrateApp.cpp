/** @file Week5-1-CrateApp.cpp
 *  @brief Texture Demo!
 *   Texture mapping is a technique that allows us to map image data onto a triangle, 
 *   thereby enabling us to increase the details and realism of our scene significantly. 
 *   In this demo, we build a cube and turn it into a stone by mapping a stone texture on each side.
 * 
 *   @note: uv coordinates start from the upper left corner (v-axis is facing down).
 *   st coordinates start from the lower left corner (t-axis is facing up).
 *   Direct3D uses a texture coordinate system that consists of a u-axis that runs
 *   horizontally to the image and a v-axis that runs vertically to the image. The coordinates,
 *   (u, v) such that 0 ≤ u, v ≤ 1, identify an element on the texture called a texel.
 *
 *   Controls:
 *   Hold down '1' key to view scene in wireframe mode.
 *   Hold the left mouse button down and move the mouse to rotate.
 *   Hold the right mouse button down and move the mouse to zoom in and out.
 *
 *  @author Hooman Salamat
 */

#include "../Common/d3dApp.h"
#include "../Common/MathHelper.h"
#include "../Common/UploadBuffer.h"
#include "../Common/GeometryGenerator.h"
#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;



// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify obect data we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

    // Primitive topology.
    D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
};

enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

class CrateApp : public D3DApp
{
public:
    CrateApp(HINSTANCE hInstance);
    CrateApp(const CrateApp& rhs) = delete;
    CrateApp& operator=(const CrateApp& rhs) = delete;
    ~CrateApp();

    virtual bool Initialize()override;

private:
    virtual void OnResize()override;
    virtual void Update(const GameTimer& gt)override;
    virtual void Draw(const GameTimer& gt)override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y)override;

    void OnKeyboardInput(const GameTimer& gt);
	void UpdateCamera(const GameTimer& gt);
	void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	//step5
	void LoadTextures();

    void BuildRootSignature();

	//step9
	void BuildDescriptorHeaps();

    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
	void BuildTreeSpritesGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	//step20
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;

	//step11
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;

	//step7
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

    std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

    ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;
 
	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

    PassConstants mMainPassCB;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.3f*XM_PI;
	float mPhi = 0.4f*XM_PI;
	float mRadius = 2.5f;

    POINT mLastMousePos;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
    PSTR cmdLine, int showCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    try
    {
        CrateApp theApp(hInstance);
        if(!theApp.Initialize())
            return 0;

        return theApp.Run();
    }
    catch(DxException& e)
    {
        MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
        return 0;
    }
}

CrateApp::CrateApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

CrateApp::~CrateApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool CrateApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    // Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    //step6
	LoadTextures();

    BuildRootSignature();

	//step10
	BuildDescriptorHeaps();

    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildTreeSpritesGeometry();
	BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildPSOs();

    // Execute the initialization commands.
    ThrowIfFailed(mCommandList->Close());
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until initialization is complete.
    FlushCommandQueue();

    return true;
}
 
void CrateApp::OnResize()
{
    D3DApp::OnResize();

    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&mProj, P);
}

void CrateApp::Update(const GameTimer& gt)
{
    OnKeyboardInput(gt);
	UpdateCamera(gt);

    // Cycle through the circular frame resource array.
    mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
    mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

    // Has the GPU finished processing the commands of the current frame resource?
    // If not, wait until the GPU has completed commands up to this fence point.
    if(mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

	AnimateMaterials(gt);
	UpdateObjectCBs(gt);
	UpdateMaterialCBs(gt);
	UpdateMainPassCB(gt);
}

void CrateApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mOpaquePSO.Get()));

    mCommandList->RSSetViewports(1, &mScreenViewport);
    mCommandList->RSSetScissorRects(1, &mScissorRect);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Clear the back buffer and depth buffer.
    mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
    mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(mCommandList.Get(), mOpaqueRitems);

    // Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(mCommandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
    mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Swap the back and front buffers
    ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

    // Advance the fence value to mark commands up to this fence point.
    mCurrFrameResource->Fence = ++mCurrentFence;

    // Add an instruction to the command queue to set a new fence point. 
    // Because we are on the GPU timeline, the new fence point won't be 
    // set until the GPU finishes processing all the commands prior to this Signal().
    mCommandQueue->Signal(mFence.Get(), mCurrentFence);
}

void CrateApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    mLastMousePos.x = x;
    mLastMousePos.y = y;

    SetCapture(mhMainWnd);
}

void CrateApp::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

void CrateApp::OnMouseMove(WPARAM btnState, int x, int y)
{
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        mPhi += dy;

        // Restrict the angle mPhi.
        mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if((btnState & MK_RBUTTON) != 0)
    {
        // Make each pixel correspond to 0.2 unit in the scene.
        float dx = 0.05f*static_cast<float>(x - mLastMousePos.x);
        float dy = 0.05f*static_cast<float>(y - mLastMousePos.y);

        // Update the camera radius based on input.
        mRadius += dx - dy;

        // Restrict the radius.
        mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
    }

    mLastMousePos.x = x;
    mLastMousePos.y = y;
}
 
void CrateApp::OnKeyboardInput(const GameTimer& gt)
{
}
 
void CrateApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius*sinf(mPhi)*cosf(mTheta);
	mEyePos.z = mRadius*sinf(mPhi)*sinf(mTheta);
	mEyePos.y = mRadius*cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void CrateApp::AnimateMaterials(const GameTimer& gt)
{
	
}

void CrateApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for(auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if(e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for(auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if(mat->NumFramesDirty > 0)
		{
			XMMATRIX matTransform = XMLoadFloat4x4(&mat->MatTransform);

			MaterialConstants matConstants;
			matConstants.DiffuseAlbedo = mat->DiffuseAlbedo;
			matConstants.FresnelR0 = mat->FresnelR0;
			matConstants.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matConstants.MatTransform, XMMatrixTranspose(matTransform));

			currMaterialCB->CopyData(mat->MatCBIndex, matConstants);

			// Next FrameResource need to be updated too.
			mat->NumFramesDirty--;
		}
	}
}

void CrateApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = XMLoadFloat4x4(&mView);
	XMMATRIX proj = XMLoadFloat4x4(&mProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	mMainPassCB.EyePosW = mEyePos;
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	mMainPassCB.Lights[0].Position = { 1.f, -1.0f, 1.0f };

	mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };

	mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };


	
	/*mMainPassCB.Lights[0].Strength = { 0.0f, 0.0f, 0.1f };
	mMainPassCB.Lights[0].FalloffStart = 10.0f;
	mMainPassCB.Lights[0].Direction = { 0.0f, 1.0f, 0.0f };
	mMainPassCB.Lights[0].FalloffEnd = 50.0f;
	mMainPassCB.Lights[0].Position = { 0.0f, -0.1f, 0.0f };
	mMainPassCB.Lights[0].SpotPower = 21.0;*/
	
	
	

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

//step 8
void CrateApp::LoadTextures()
{
	auto woodCrateTex = std::make_unique<Texture>();
	woodCrateTex->Name = "woodCrateTex";
	woodCrateTex->Filename = L"../Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), woodCrateTex->Filename.c_str(),
		woodCrateTex->Resource, woodCrateTex->UploadHeap));

	auto brickTex = std::make_unique<Texture>();
	brickTex->Name = "brickTex";
	brickTex->Filename = L"../Textures/bricks.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), brickTex->Filename.c_str(),
		brickTex->Resource, brickTex->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"../Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));

	auto marbleTex = std::make_unique<Texture>();
	marbleTex->Name = "marbleTex";
	marbleTex->Filename = L"../Textures/marble.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), marbleTex->Filename.c_str(),
		marbleTex->Resource, marbleTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"../Textures/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	mTextures[brickTex->Name] = std::move(brickTex);
	mTextures[woodCrateTex->Name] = std::move(woodCrateTex);
	mTextures[grassTex->Name] = std::move(grassTex);
	mTextures[marbleTex->Name] = std::move(marbleTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
}

void CrateApp::BuildRootSignature()
{
	//step22
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

	auto staticSamplers = GetStaticSamplers();

    // A root signature is an array of root parameters.
	//The Init function of the CD3DX12_ROOT_SIGNATURE_DESC class has two parameters that allow you to
		//define an array of so - called static samplers your application can use.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),  //6 samplers!
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}

//step12
//Once a texture resource is created, we need to create an SRV descriptor to it which we
//can set to a root signature parameter slot for use by the shader programs.
void CrateApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 5;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto woodCrateTex = mTextures["woodCrateTex"]->Resource;
	auto brickTex = mTextures["brickTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;
	auto marbleTex = mTextures["marbleTex"]->Resource;

	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
 
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	//This mapping enables the shader resource view (SRV) to choose how memory gets routed to the 4 return components in a shader after a memory fetch.
	//When a texture is sampled in a shader, it will return a vector of the texture data at the specified texture coordinates.
	//This field provides a way to reorder the vector components returned when sampling the texture.
	//D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING  will not reorder the components and just return the data in the order it is stored in the texture resource.
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = woodCrateTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	//The number of mipmap levels to view, starting at MostDetailedMip.This field, along with MostDetailedMip allows us to
	//specify a subrange of mipmap levels to view.You can specify - 1 to indicate to view
	//all mipmap levels from MostDetailedMip down to the last mipmap level.
	srvDesc.Texture2D.MipLevels = -1;
	//Specifies the minimum mipmap level that can be accessed. 0.0 means all the mipmap levels can be accessed.
	//Specifying 3.0 means mipmap levels 3.0 to MipCount - 1 can be accessed.
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = brickTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = brickTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(brickTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = marbleTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = marbleTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(marbleTex.Get(), &srvDesc, hDescriptor);

	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);


}

void CrateApp::BuildShadersAndInputLayout()
{
	/*const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"ALPHA_TEST",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_0");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "PS", "ps_5_0");

	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

    mInputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		step3
		The texture coordinates determine what part of the texture gets mapped on the triangles.
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };*/

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};

	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\Default.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	mTreeSpriteInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

void CrateApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;

	GeometryGenerator::MeshData grid = geoGen.CreateGrid(30.0f, 30.0f, 15, 15);
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 2);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.5f, 1.0f, 8.0f, 10, 5);
	GeometryGenerator::MeshData pyramid = geoGen.CreateSquarePyramid(5.0f, 7.0f, 7.0f, 2);
	GeometryGenerator::MeshData cone = geoGen.CreateCone(1.5f, 1.5f, 6.0f, 3.0f);
	GeometryGenerator::MeshData torus = geoGen.CreateTorus(1.5f, 0.3f, 8, 6);
	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(2.0f, 2.0f, 1.0f, 1);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 6.0f, 6.0f);

	UINT gridVertexOffset = 0;
	UINT boxVertexOffset = grid.Vertices.size();
	UINT cylinderVertexOffset = boxVertexOffset + box.Vertices.size();
	UINT pyramidVertexOffset = cylinderVertexOffset + cylinder.Vertices.size();
	UINT coneVertexOffset = pyramidVertexOffset + pyramid.Vertices.size();
	UINT torusVertexOffset = coneVertexOffset + cone.Vertices.size();
	UINT diamondVertexOffset = torusVertexOffset + torus.Vertices.size();
	UINT sphereVertexOffset = diamondVertexOffset + diamond.Vertices.size();

	UINT gridIndexOffset = 0;
	UINT boxIndexOffset = grid.Indices32.size();
	UINT cylinderIndexOffset = boxIndexOffset + box.Indices32.size();
	UINT pyramidIndexOffset = cylinderIndexOffset + cylinder.Indices32.size();
	UINT coneIndexOffset = pyramidIndexOffset + pyramid.Indices32.size();
	UINT torusIndexOffset = coneIndexOffset + cone.Indices32.size();
	UINT diamondIndexOffset = torusIndexOffset + torus.Indices32.size();
	UINT sphereIndexOffset = diamondIndexOffset + diamond.Indices32.size();


	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry pyramidSubmesh;
	pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
	pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
	pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;

	SubmeshGeometry coneSubmesh;
	coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
	coneSubmesh.StartIndexLocation = coneIndexOffset;
	coneSubmesh.BaseVertexLocation = coneVertexOffset;

	SubmeshGeometry torusSubmesh;
	torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
	torusSubmesh.StartIndexLocation = torusIndexOffset;
	torusSubmesh.BaseVertexLocation = torusVertexOffset;

	SubmeshGeometry diamondSubmesh;
	diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
	diamondSubmesh.StartIndexLocation = diamondIndexOffset;
	diamondSubmesh.BaseVertexLocation = diamondVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	auto totalVertexCount = grid.Vertices.size() + box.Vertices.size() + cylinder.Vertices.size() + pyramid.Vertices.size() + cone.Vertices.size() + torus.Vertices.size() + diamond.Vertices.size() + sphere.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
	}
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}
	for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = pyramid.Vertices[i].Position;
		vertices[k].Normal = pyramid.Vertices[i].Normal;
		vertices[k].TexC = pyramid.Vertices[i].TexC;
	}
	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;
	}
	for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torus.Vertices[i].Position;
		vertices[k].Normal = torus.Vertices[i].Normal;
		vertices[k].TexC = torus.Vertices[i].TexC;
	}
	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Normal = diamond.Vertices[i].Normal;
		vertices[k].TexC = diamond.Vertices[i].TexC;
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));

    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "boxGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(Vertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	geo->DrawArgs["grid"] = gridSubmesh;
	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["pyramid"] = pyramidSubmesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["torus"] = torusSubmesh;
	geo->DrawArgs["diamond"] = diamondSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;

	mGeometries["boxGeo"] = std::move(geo);
}

void CrateApp::BuildTreeSpritesGeometry()
{
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		float x = MathHelper::RandF(-45.0f, 45.0f);
		float z = MathHelper::RandF(-45.0f, 45.0f);
		float y = 0.0f;

		// Move tree slightly above land height.
		y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	}

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(TreeSpriteVertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "treeSpritesGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->VertexBufferCPU));
	CopyMemory(geo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

	ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->IndexBufferCPU));
	CopyMemory(geo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

	geo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), vertices.data(), vbByteSize, geo->VertexBufferUploader);

	geo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(md3dDevice.Get(),
		mCommandList.Get(), indices.data(), ibByteSize, geo->IndexBufferUploader);

	geo->VertexByteStride = sizeof(TreeSpriteVertex);
	geo->VertexBufferByteSize = vbByteSize;
	geo->IndexFormat = DXGI_FORMAT_R16_UINT;
	geo->IndexBufferByteSize = ibByteSize;

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["points"] = submesh;

	mGeometries["treeSpritesGeo"] = std::move(geo);
}

void CrateApp::BuildPSOs()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), 
		mShaders["standardVS"]->GetBufferSize()
	};
	opaquePsoDesc.PS = 
	{ 
		reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()),
		mShaders["opaquePS"]->GetBufferSize()
	};
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;

	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
    //ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


	//
	// PSO for transparent objects
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;

	D3D12_RENDER_TARGET_BLEND_DESC transparencyBlendDesc;
	transparencyBlendDesc.BlendEnable = true;
	transparencyBlendDesc.LogicOpEnable = false;
	transparencyBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparencyBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparencyBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparencyBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparencyBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparencyBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparencyBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparencyBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	//
	// PSO for alpha tested objects
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()),
		mShaders["alphaTestedPS"]->GetBufferSize()
	};
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	//
	// PSO for tree sprites
	//
	D3D12_GRAPHICS_PIPELINE_STATE_DESC treeSpritePsoDesc = opaquePsoDesc;
	treeSpritePsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteVS"]->GetBufferPointer()),
		mShaders["treeSpriteVS"]->GetBufferSize()
	};
	treeSpritePsoDesc.GS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpriteGS"]->GetBufferPointer()),
		mShaders["treeSpriteGS"]->GetBufferSize()
	};
	treeSpritePsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(mShaders["treeSpritePS"]->GetBufferPointer()),
		mShaders["treeSpritePS"]->GetBufferSize()
	};
	//step1
	treeSpritePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	treeSpritePsoDesc.InputLayout = { mTreeSpriteInputLayout.data(), (UINT)mTreeSpriteInputLayout.size() };
	treeSpritePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&treeSpritePsoDesc, IID_PPV_ARGS(&mPSOs["treeSprites"])));

}

void CrateApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}
//step13
void CrateApp::BuildMaterials()
{
	auto woodCrate = std::make_unique<Material>();
	woodCrate->Name = "woodCrate";
	woodCrate->MatCBIndex = 0;
	//! We add an index to our material definition, which references an SRV in the descriptor
	//! heap specifying the texture associated with the material :
	woodCrate->DiffuseSrvHeapIndex = 0;
	woodCrate->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	woodCrate->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	woodCrate->Roughness = 0.2f;

	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->MatCBIndex = 1;
	bricks->DiffuseSrvHeapIndex = 1;
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks->Roughness = 0.1f;

	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->MatCBIndex = 2;
	grass->DiffuseSrvHeapIndex = 2;
	grass->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	grass->Roughness = 0.1f;

	auto marble = std::make_unique<Material>();
	marble->Name = "marble";
	marble->MatCBIndex = 3;
	marble->DiffuseSrvHeapIndex = 3;
	marble->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	marble->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	marble->Roughness = 0.1f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 4;
	treeSprites->DiffuseSrvHeapIndex = 4;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	mMaterials["bricks"] = std::move(bricks);
	mMaterials["woodCrate"] = std::move(woodCrate);
	mMaterials["grass"] = std::move(grass);
	mMaterials["marble"] = std::move(marble);
	mMaterials["treeSprites"] = std::move(treeSprites);
}

//step14
void CrateApp::BuildRenderItems()
{
	// floor
	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(1.0f, -1.0f, 1.0f));
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->ObjCBIndex = 0;
	gridRitem->Mat = mMaterials["grass"].get();
	gridRitem->Geo = mGeometries["boxGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(gridRitem));

	// main building
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(1.0f, 3.0f, 1.0f));
	XMStoreFloat4x4(&boxRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Mat = mMaterials["woodCrate"].get();
	boxRitem->Geo = mGeometries["boxGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	// tower 1
	auto CylRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem->World, XMMatrixTranslation(5.5f, 3.0f, 5.5f));
	XMStoreFloat4x4(&CylRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem->ObjCBIndex = 2;
	CylRitem->Mat = mMaterials["woodCrate"].get();
	CylRitem->Geo = mGeometries["boxGeo"].get();
	CylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem->IndexCount = CylRitem->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem->StartIndexLocation = CylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem->BaseVertexLocation = CylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem));

	// tower 2
	auto CylRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem1->World, XMMatrixTranslation(-3.5f, 3.0f, 5.5f));
	XMStoreFloat4x4(&CylRitem1->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem1->ObjCBIndex = 3;
	CylRitem1->Mat = mMaterials["woodCrate"].get();
	CylRitem1->Geo = mGeometries["boxGeo"].get();
	CylRitem1->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem1->IndexCount = CylRitem1->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem1->StartIndexLocation = CylRitem1->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem1->BaseVertexLocation = CylRitem1->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem1));

	// tower 3
	auto CylRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem2->World, XMMatrixTranslation(-3.5f, 3.0f, -3.5f));
	XMStoreFloat4x4(&CylRitem2->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem2->ObjCBIndex = 4;
	CylRitem2->Mat = mMaterials["woodCrate"].get();
	CylRitem2->Geo = mGeometries["boxGeo"].get();
	CylRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem2->IndexCount = CylRitem2->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem2->StartIndexLocation = CylRitem2->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem2->BaseVertexLocation = CylRitem2->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem2));

	// tower 4
	auto CylRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem3->World, XMMatrixTranslation(5.5f, 3.0f, -3.5f));
	XMStoreFloat4x4(&CylRitem3->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem3->ObjCBIndex = 5;
	CylRitem3->Mat = mMaterials["woodCrate"].get();
	CylRitem3->Geo = mGeometries["boxGeo"].get();
	CylRitem3->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem3->IndexCount = CylRitem3->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem3->StartIndexLocation = CylRitem3->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem3->BaseVertexLocation = CylRitem3->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem3));

	// building top
	auto pyramidRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&pyramidRitem->World, XMMatrixTranslation(1.0f, 7.0f, 1.0f));
	XMStoreFloat4x4(&pyramidRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	pyramidRitem->ObjCBIndex = 6;
	pyramidRitem->Mat = mMaterials["marble"].get();
	pyramidRitem->Geo = mGeometries["boxGeo"].get();
	pyramidRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	pyramidRitem->IndexCount = pyramidRitem->Geo->DrawArgs["pyramid"].IndexCount;
	pyramidRitem->StartIndexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].StartIndexLocation;
	pyramidRitem->BaseVertexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(pyramidRitem));

	// tower top 1
	auto coneRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem->World, XMMatrixTranslation(5.5f, 7.7f, 5.5f));
	XMStoreFloat4x4(&coneRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem->ObjCBIndex = 7;
	coneRitem->Mat = mMaterials["marble"].get();
	coneRitem->Geo = mGeometries["boxGeo"].get();
	coneRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem->IndexCount = coneRitem->Geo->DrawArgs["cone"].IndexCount;
	coneRitem->StartIndexLocation = coneRitem->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem->BaseVertexLocation = coneRitem->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem));

	// tower top 2
	auto coneRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem2->World, XMMatrixTranslation(-3.5f, 7.7f, 5.5f));
	XMStoreFloat4x4(&coneRitem2->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem2->ObjCBIndex = 8;
	coneRitem2->Mat = mMaterials["marble"].get();
	coneRitem2->Geo = mGeometries["boxGeo"].get();
	coneRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem2->IndexCount = coneRitem2->Geo->DrawArgs["cone"].IndexCount;
	coneRitem2->StartIndexLocation = coneRitem2->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem2->BaseVertexLocation = coneRitem2->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem2));

	// tower top 3
	auto coneRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem3->World, XMMatrixTranslation(-3.5f, 7.7f, -3.5f));
	XMStoreFloat4x4(&coneRitem3->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem3->ObjCBIndex = 9;
	coneRitem3->Mat = mMaterials["marble"].get();
	coneRitem3->Geo = mGeometries["boxGeo"].get();
	coneRitem3->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem3->IndexCount = coneRitem3->Geo->DrawArgs["cone"].IndexCount;
	coneRitem3->StartIndexLocation = coneRitem3->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem3->BaseVertexLocation = coneRitem3->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem3));

	//  tower top 3
	auto coneRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem4->World, XMMatrixTranslation(5.5f, 7.7f, -3.5f));
	XMStoreFloat4x4(&coneRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem4->ObjCBIndex = 10;
	coneRitem4->Mat = mMaterials["marble"].get();
	coneRitem4->Geo = mGeometries["boxGeo"].get();
	coneRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem4->IndexCount = coneRitem4->Geo->DrawArgs["cone"].IndexCount;
	coneRitem4->StartIndexLocation = coneRitem4->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem4->BaseVertexLocation = coneRitem4->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem4));

	// building loop 
	auto torusRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&torusRitem4->World, XMMatrixTranslation(1.0f, 5.0f, -3.2f));
	XMStoreFloat4x4(&torusRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	torusRitem4->ObjCBIndex = 11;
	torusRitem4->Mat = mMaterials["woodCrate"].get();
	torusRitem4->Geo = mGeometries["boxGeo"].get();
	torusRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	torusRitem4->IndexCount = torusRitem4->Geo->DrawArgs["torus"].IndexCount;
	torusRitem4->StartIndexLocation = torusRitem4->Geo->DrawArgs["torus"].StartIndexLocation;
	torusRitem4->BaseVertexLocation = torusRitem4->Geo->DrawArgs["torus"].BaseVertexLocation;
	mAllRitems.push_back(std::move(torusRitem4));

	// ornament 
	auto diamondRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&diamondRitem4->World, XMMatrixTranslation(1.0f, 5.0f, -3.4f));
	XMStoreFloat4x4(&diamondRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	diamondRitem4->ObjCBIndex = 12;
	diamondRitem4->Mat = mMaterials["woodCrate"].get();
	diamondRitem4->Geo = mGeometries["boxGeo"].get();
	diamondRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamondRitem4->IndexCount = diamondRitem4->Geo->DrawArgs["diamond"].IndexCount;
	diamondRitem4->StartIndexLocation = diamondRitem4->Geo->DrawArgs["diamond"].StartIndexLocation;
	diamondRitem4->BaseVertexLocation = diamondRitem4->Geo->DrawArgs["diamond"].BaseVertexLocation;
	mAllRitems.push_back(std::move(diamondRitem4));

	// wall 1
	auto boxRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem1->World, XMMatrixScaling(2.0f, 0.7f, 0.2f) * XMMatrixTranslation(1.0f, 1.8f, 10.2f));
	XMStoreFloat4x4(&boxRitem1->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem1->ObjCBIndex = 13;
	boxRitem1->Mat = mMaterials["bricks"].get();
	boxRitem1->Geo = mGeometries["boxGeo"].get();
	boxRitem1->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem1->IndexCount = boxRitem1->Geo->DrawArgs["box"].IndexCount;
	boxRitem1->StartIndexLocation = boxRitem1->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem1->BaseVertexLocation = boxRitem1->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem1));

	// wall 2
	auto boxRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem2->World, XMMatrixScaling(0.2f, 0.7f, 2.5f) * XMMatrixTranslation(9.5f, 1.8f, 1.0f));
	XMStoreFloat4x4(&boxRitem2->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem2->ObjCBIndex = 14;
	boxRitem2->Mat = mMaterials["bricks"].get();
	boxRitem2->Geo = mGeometries["boxGeo"].get();
	boxRitem2->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["box"].IndexCount;
	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem2));

	// wall 3
	auto boxRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem3->World, XMMatrixScaling(0.2f, 0.7f, 2.5f) * XMMatrixTranslation(-7.8f, 1.8f, 1.0f));
	XMStoreFloat4x4(&boxRitem3->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem3->ObjCBIndex = 15;
	boxRitem3->Mat = mMaterials["bricks"].get();
	boxRitem3->Geo = mGeometries["boxGeo"].get();
	boxRitem3->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem3->IndexCount = boxRitem3->Geo->DrawArgs["box"].IndexCount;
	boxRitem3->StartIndexLocation = boxRitem3->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem3->BaseVertexLocation = boxRitem3->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem3));

	// wall 4
	auto boxRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem4->World, XMMatrixScaling(0.8f, 0.7f, 0.2f) * XMMatrixTranslation(7.1f, 1.8f, -9.8f));
	XMStoreFloat4x4(&boxRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem4->ObjCBIndex = 16;
	boxRitem4->Mat = mMaterials["bricks"].get();
	boxRitem4->Geo = mGeometries["boxGeo"].get();
	boxRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem4->IndexCount = boxRitem4->Geo->DrawArgs["box"].IndexCount;
	boxRitem4->StartIndexLocation = boxRitem4->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem4->BaseVertexLocation = boxRitem4->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem4));

	// wall 5
	auto boxRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem5->World, XMMatrixScaling(0.8f, 0.7f, 0.2f) * XMMatrixTranslation(-5.4f, 1.8f, -9.8f));
	XMStoreFloat4x4(&boxRitem5->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	boxRitem5->ObjCBIndex = 17;
	boxRitem5->Mat = mMaterials["bricks"].get();
	boxRitem5->Geo = mGeometries["boxGeo"].get();
	boxRitem5->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem5->IndexCount = boxRitem5 ->Geo->DrawArgs["box"].IndexCount;
	boxRitem5->StartIndexLocation = boxRitem5->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem5->BaseVertexLocation = boxRitem5->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem5));

	// outer tower 1
	auto CylRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem4->World, XMMatrixScaling(1.0f, 1.3f, 1.0f) * XMMatrixTranslation(10.0f, 4.2f, 10.5f));
	XMStoreFloat4x4(&CylRitem4->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem4->ObjCBIndex = 18;
	CylRitem4->Mat = mMaterials["woodCrate"].get();
	CylRitem4->Geo = mGeometries["boxGeo"].get();
	CylRitem4->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem4->IndexCount = CylRitem4->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem4->StartIndexLocation = CylRitem4->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem4->BaseVertexLocation = CylRitem4->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem4));

	// outer tower 2
	auto CylRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem5->World, XMMatrixScaling(1.0f, 1.3f, 1.0f) * XMMatrixTranslation(-8.0f, 4.2f, 10.5f));
	XMStoreFloat4x4(&CylRitem5->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem5->ObjCBIndex = 19;
	CylRitem5->Mat = mMaterials["woodCrate"].get();
	CylRitem5->Geo = mGeometries["boxGeo"].get();
	CylRitem5->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem5->IndexCount = CylRitem5->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem5->StartIndexLocation = CylRitem5->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem5->BaseVertexLocation = CylRitem5->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem5));

	// outer tower 3
	auto CylRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem6->World, XMMatrixScaling(0.8f, 1.0f, 0.8f) * XMMatrixTranslation(-2.0f, 3.0f, -9.9f));
	XMStoreFloat4x4(&CylRitem6->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem6->ObjCBIndex = 20;
	CylRitem6->Mat = mMaterials["woodCrate"].get();
	CylRitem6->Geo = mGeometries["boxGeo"].get();
	CylRitem6->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem6->IndexCount = CylRitem6->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem6->StartIndexLocation = CylRitem6->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem6->BaseVertexLocation = CylRitem6->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem6));

	// outer tower 4
	auto CylRitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem7->World, XMMatrixScaling(0.8f, 1.0f, 0.8f) * XMMatrixTranslation(3.5f, 3.0f, -9.9f));
	XMStoreFloat4x4(&CylRitem7->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	CylRitem7->ObjCBIndex = 21;
	CylRitem7->Mat = mMaterials["woodCrate"].get();
	CylRitem7->Geo = mGeometries["boxGeo"].get();
	CylRitem7->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem7->IndexCount = CylRitem7->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem7->StartIndexLocation = CylRitem7->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem7->BaseVertexLocation = CylRitem7->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem7));

	// outer tower top 1
	auto sphereRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem->World, XMMatrixTranslation(3.5f, 7.6f, -9.9f));
	XMStoreFloat4x4(&sphereRitem->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	sphereRitem->ObjCBIndex = 22;
	sphereRitem->Mat = mMaterials["marble"].get();
	sphereRitem->Geo = mGeometries["boxGeo"].get();
	sphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem));

	// outer tower top 2
	auto sphereRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem1->World, XMMatrixTranslation(-2.0f, 7.6f, -9.9f));
	XMStoreFloat4x4(&sphereRitem1->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	sphereRitem1->ObjCBIndex = 23;
	sphereRitem1->Mat = mMaterials["marble"].get();
	sphereRitem1->Geo = mGeometries["boxGeo"].get();
	sphereRitem1->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem1->IndexCount = sphereRitem1->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem1->StartIndexLocation = sphereRitem1->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem1->BaseVertexLocation = sphereRitem1->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem1));

	// outer tower top 3
	auto coneRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem5->World, XMMatrixTranslation(10.0f, 10.1f, 10.5f));
	XMStoreFloat4x4(&coneRitem5->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem5->ObjCBIndex = 24;
	coneRitem5->Mat = mMaterials["marble"].get();
	coneRitem5->Geo = mGeometries["boxGeo"].get();
	coneRitem5->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem5->IndexCount = coneRitem5->Geo->DrawArgs["cone"].IndexCount;
	coneRitem5->StartIndexLocation = coneRitem5->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem5->BaseVertexLocation = coneRitem5->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem5));

	// outer tower top 4
	auto coneRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem6->World, XMMatrixTranslation(-8.0f, 10.1f, 10.5f));
	XMStoreFloat4x4(&coneRitem6->TexTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	coneRitem6->ObjCBIndex = 25;
	coneRitem6->Mat = mMaterials["marble"].get();
	coneRitem6->Geo = mGeometries["boxGeo"].get();
	coneRitem6->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem6->IndexCount = coneRitem6->Geo->DrawArgs["cone"].IndexCount;
	coneRitem6->StartIndexLocation = coneRitem6->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem6->BaseVertexLocation = coneRitem6->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem6));


	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = 26;
	treeSpritesRitem->Mat = mMaterials["treeSprites"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	//step2
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;

	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());

	// All the render items are opaque.
	for(auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
	mAllRitems.push_back(std::move(treeSpritesRitem));
}

void CrateApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
    UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    UINT matCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(MaterialConstants));
 
	auto objectCB = mCurrFrameResource->ObjectCB->Resource();
	auto matCB = mCurrFrameResource->MaterialCB->Resource();

    // For each render item...
    for(size_t i = 0; i < ritems.size(); ++i)
    {
        auto ri = ritems[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		//step18
		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex*objCBByteSize;
		D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex*matCBByteSize;

		//step19: assuming the root signature has been defined to expect a table of shader
		//resource views to be bound to the 0th slot parameter
		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }
}

//step21
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> CrateApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return { 
		pointWrap, pointClamp,
		linearWrap, linearClamp, 
		anisotropicWrap, anisotropicClamp };
}









