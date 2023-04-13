/** @file Week4-1-BoxUsingFrameResources.cpp
 *  @brief Draw a box using FrameResources.
 *
 *   With frame resources, we modify our render loop so that we
 *   do not have to flush the command queue every frame; this improves CPU and GPU utilization.
 *   Because the CPU only needs to modify constant buffers in this demo, the frame
 *   resource class only contains constant buffers
 *   Then we create a render item and we divide up our constant data based on update frequency.
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

#include <iostream>
#include <string>

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

//step3: Our application class instantiates a vector of three frame resources, 
const int gNumFrameResources = 3;

// Step10: Lightweight structure stores parameters to draw a shape.  This will vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;

	// World matrix of the shape that describes the object's local space
	// relative to the world space, which defines the position, orientation,
	// and scale of the object in the world.
	XMFLOAT4X4 World = MathHelper::Identity4x4();

	// Dirty flag indicating the object data has changed and we need to update the constant buffer.
	// Because we have an object cbuffer for each FrameResource, we have to apply the
	// update to each FrameResource.  Thus, when we modify object data, we should set 
	// NumFramesDirty = gNumFrameResources so that each frame resource gets the update.
	int NumFramesDirty = gNumFrameResources;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1;

	// Geometry associated with this render-item. Note that multiple
	// render-items can share the same geometry.
	MeshGeometry* Geo = nullptr;

	// Primitive topology.
	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// DrawIndexedInstanced parameters.
	UINT IndexCount = 0; //Number of indices read from the index buffer for each instance.
	UINT StartIndexLocation = 0; //The location of the first index read by the GPU from the index buffer.
	int BaseVertexLocation = 0; //A value added to each index before reading a vertex from the vertex buffer.
};

class ShapesApp : public D3DApp
{
public:
	ShapesApp(HINSTANCE hInstance);
	ShapesApp(const ShapesApp& rhs) = delete;
	ShapesApp& operator=(const ShapesApp& rhs) = delete;
	~ShapesApp();

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
	void UpdateObjectCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

	void BuildDescriptorHeaps();
	void BuildConstantBufferViews();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildShapeGeometry();
	void BuildPSOs();

	//step5
	void BuildFrameResources();

	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

private:

	//step4: keep member variables to track the current frame resource :
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;

	//step11: Our application will maintain lists of render items based on how they need to be
	//drawn; that is, render items that need different PSOs will be kept in different lists.

	// Render items divided by PSO.
	std::vector<RenderItem*> mOpaqueRitems;

	//std::vector<RenderItem*> mTransparentRitems;  //we could have render items for transparant items



	//step12: this mMainPassCB stores constant data that is fixed over a given
	//rendering pass such as the eye position, the view and projection matrices, and information
	//about the screen(render target) dimensions; it also includes game timing information,
	//which is useful data to have access to in shader programs.

	PassConstants mMainPassCB;

	UINT mPassCbvOffset = 0;

	bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	float mTheta = 1.5f * XM_PI;
	float mPhi = 0.2f * XM_PI;
	float mRadius = 15.0f;

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
		ShapesApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

ShapesApp::ShapesApp(HINSTANCE hInstance)
	: D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
	if (md3dDevice != nullptr)
		FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildShapeGeometry();
	BuildRenderItems();
	BuildFrameResources();
	BuildDescriptorHeaps();
	BuildConstantBufferViews();
	BuildPSOs();

	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}

void ShapesApp::OnResize()
{
	D3DApp::OnResize();

	// The window resized, so update the aspect ratio and recompute the projection matrix.
	XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
	XMStoreFloat4x4(&mProj, P);
}

//step7: for CPU frame n, the algorithm
//1. Cycle through the circular frame resource array.
//2. Wait until the GPU has completed commands up to this fence point.
//3. Update resources in mCurrFrameResource (like cbuffers).

void ShapesApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);
	UpdateCamera(gt);

	//! Cycle through the circular frame resource array.
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	//! this section is really what D3DApp::FlushCommandQueue() used to do for us at the end of each draw() function!
	//! Has the GPU finished processing the commands of the current frame resource. 
	//! If not, wait until the GPU has completed commands up to this fence point.
	if (mCurrFrameResource->Fence != 0 && mFence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		std::wstring text2 = L"GPU Completed " + std::to_wstring(mFence->GetCompletedValue()) + L" but current fence is " + std::to_wstring(mCurrFrameResource->Fence) + L"\n";
		OutputDebugString(text2.c_str());

		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, false, EVENT_ALL_ACCESS);
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}

	//The idea of these changes is to group constants based on update frequency. The per
	//pass constants only need to be updated once per rendering pass, and the object constants
	//only need to change when an object’s world matrix changes.
	UpdateObjectCBs(gt);
	UpdateMainPassCB(gt);
}

void ShapesApp::Draw(const GameTimer& gt)
{
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(cmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	if (mIsWireframe)
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque_wireframe"].Get()));
	}
	else
	{
		ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));
	}

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

	ID3D12DescriptorHeap* descriptorHeaps[] = { mCbvHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	int passCbvIndex = mPassCbvOffset + mCurrFrameResourceIndex;
	auto passCbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
	passCbvHandle.Offset(passCbvIndex, mCbvSrvUavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(1, passCbvHandle);

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


	//Step1:  we have been calling D3DApp::FlushCommandQueue at the end of every
	//frame to ensure the GPU has finished executing all the commands for the frame.This solution works but is inefficient
	//For every frame, the CPU and GPU are idling at some point.
	//	FlushCommandQueue();


	//step9:  Advance the fence value to mark commands up to this fence point.
	mCurrFrameResource->Fence = ++mCurrentFence;

	// Add an instruction to the command queue to set a new fence point. 
	// Because we are on the GPU timeline, the new fence point won't be 
	// set until the GPU finishes processing all the commands prior to this Signal().
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);

	// Note that GPU could still be working on commands from previous
	// frames, but that is okay, because we are not touching any frame
	// resources associated with those frames.

	std::wstring text = L"Current Frame Resource Index= " + std::to_wstring(mCurrFrameResourceIndex) + L"\n";
	text += L"Current Fence = " + std::to_wstring(mCurrentFence) + L"\n";

	// Has the GPU finished processing the commands of the current frame resource?
	// If not, wait until the GPU has completed commands up to this fence point.
	text += L"CPU has added commands up to this Fence Number for current frame resource = " + std::to_wstring(mCurrFrameResource->Fence) + L"\n";
	text += L"GPU has completed commands up to Fence Number = " + std::to_wstring(mFence->GetCompletedValue()) + L"\n";
	OutputDebugString(text.c_str());

}

void ShapesApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void ShapesApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void ShapesApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

		// Update angles based on input to orbit camera around box.
		mTheta += dx;
		mPhi += dy;

		// Restrict the angle mPhi.
		mPhi = MathHelper::Clamp(mPhi, 0.1f, MathHelper::Pi - 0.1f);
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		// Make each pixel correspond to 0.2 unit in the scene.
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);

		// Update the camera radius based on input.
		mRadius += dx - dy;

		// Restrict the radius.
		mRadius = MathHelper::Clamp(mRadius, 5.0f, 150.0f);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	//! Determines whether a key is up or down at the time the function is called, and whether the key was pressed after a previous call to GetAsyncKeyState.
	//! If the function succeeds, the return value specifies whether the key was pressed since the last call to GetAsyncKeyState, 
	//! and whether the key is currently up or down. If the most significant bit is set, the key is down, and if the least significant bit is set, the key was pressed after the previous call to GetAsyncKeyState.
	//! if (GetAsyncKeyState('1') & 0x8000) 

	short key = GetAsyncKeyState('1');
	//! you can use the virtual key code (0x31) for '1' as well
	//! https://docs.microsoft.com/en-us/windows/win32/inputdev/virtual-key-codes
	//! short key = GetAsyncKeyState(0x31); 

	if (key & 0x8000)  //if one is pressed, 0x8000 = 32767 , key = -32767 = FFFFFFFFFFFF8001
		mIsWireframe = true;
	else
		mIsWireframe = false;
}

void ShapesApp::UpdateCamera(const GameTimer& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

//step8: Update resources (cbuffers) in mCurrFrameResource
void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));

			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;
		}
	}
}

//CBVs will be set at different frequencies—the per pass CBV only needs to be set once per
//rendering pass while the per object CBV needs to be set per render item
void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
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

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::BuildDescriptorHeaps()
{
	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource,
	// +1 for the perPass CBV for each frame resource.
	UINT numDescriptors = (objCount + 1) * gNumFrameResources;

	// Save an offset to the start of the pass CBVs.  These are the last 3 descriptors.
	mPassCbvOffset = objCount * gNumFrameResources;

	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc;
	cbvHeapDesc.NumDescriptors = numDescriptors;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	cbvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&cbvHeapDesc,
		IID_PPV_ARGS(&mCbvHeap)));
}

void ShapesApp::BuildConstantBufferViews()
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	UINT objCount = (UINT)mOpaqueRitems.size();

	// Need a CBV descriptor for each object for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto objectCB = mFrameResources[frameIndex]->ObjectCB->Resource();
		for (UINT i = 0; i < objCount; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCB->GetGPUVirtualAddress();

			// Offset to the ith object constant buffer in the buffer.
			cbAddress += i * objCBByteSize;

			// Offset to the object cbv in the descriptor heap.
			int heapIndex = frameIndex * objCount + i;
			auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
			handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = cbAddress;
			cbvDesc.SizeInBytes = objCBByteSize;

			md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
		}
	}

	UINT passCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

	// Last three descriptors are the pass CBVs for each frame resource.
	for (int frameIndex = 0; frameIndex < gNumFrameResources; ++frameIndex)
	{
		auto passCB = mFrameResources[frameIndex]->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCB->GetGPUVirtualAddress();

		// Offset to the pass cbv in the descriptor heap.
		int heapIndex = mPassCbvOffset + frameIndex;
		auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mCbvHeap->GetCPUDescriptorHandleForHeapStart());
		handle.Offset(heapIndex, mCbvSrvUavDescriptorSize);

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = cbAddress;
		cbvDesc.SizeInBytes = passCBByteSize;

		md3dDevice->CreateConstantBufferView(&cbvDesc, handle);
	}
}

void ShapesApp::BuildRootSignature()
{
	//step 15
	//The resources that our shaders expect have changed; therefore, we need to update the
	//root signature accordingly to take two descriptor tables(we need two tables because the
	//CBVs will be set at different frequencies—the per pass CBV only needs to be set once per
	//rendering pass while the per object CBV needs to be set per render item) :

	CD3DX12_DESCRIPTOR_RANGE cbvTable0;
	cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

	CD3DX12_DESCRIPTOR_RANGE cbvTable1;
	cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];

	// Create root CBVs.
	slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
	slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
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

void ShapesApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\VS.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\PS.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
}

//step16
void ShapesApp::BuildShapeGeometry()
{
	//GeometryGenerator is a utility class for generating simple geometric shapes like grids, sphere, cylinders, and boxes
	GeometryGenerator geoGen;
	//The MeshData structure is a simple structure nested inside GeometryGenerator that stores a vertex and index list

	GeometryGenerator::MeshData grid = geoGen.CreateGrid(30.0f, 30.0f, 15, 15);
	GeometryGenerator::MeshData box = geoGen.CreateBox(8.0f, 8.0f, 8.0f, 2);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(1.5f, 1.0f, 8.0f, 10, 5);
	GeometryGenerator::MeshData pyramid = geoGen.CreateSquarePyramid(5.0f, 7.0f, 7.0f, 2);
	GeometryGenerator::MeshData cone = geoGen.CreateCone(1.5f, 1.5f, 6.0f, 3.0f);
	GeometryGenerator::MeshData torus = geoGen.CreateTorus(1.5f, 0.3f, 8, 6);
	GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(2.0f, 2.0f, 1.0f, 1);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(1.0f, 6.0f, 6.0f);

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT gridVertexOffset = 0;
	UINT boxVertexOffset = grid.Vertices.size();
	UINT cylinderVertexOffset = boxVertexOffset + box.Vertices.size();
	UINT pyramidVertexOffset = cylinderVertexOffset + cylinder.Vertices.size();
	UINT coneVertexOffset = pyramidVertexOffset + pyramid.Vertices.size();
	UINT torusVertexOffset = coneVertexOffset + cone.Vertices.size();
	UINT diamondVertexOffset = torusVertexOffset + torus.Vertices.size();
	UINT sphereVertexOffset = diamondVertexOffset + diamond.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT gridIndexOffset = 0;
	UINT boxIndexOffset = grid.Indices32.size();
	UINT cylinderIndexOffset = boxIndexOffset + box.Indices32.size();
	UINT pyramidIndexOffset = cylinderIndexOffset + cylinder.Indices32.size();
	UINT coneIndexOffset = pyramidIndexOffset + pyramid.Indices32.size();
	UINT torusIndexOffset = coneIndexOffset + cone.Indices32.size();
	UINT diamondIndexOffset = torusIndexOffset + torus.Indices32.size();
	UINT sphereIndexOffset = diamondIndexOffset + diamond.Indices32.size();

	// Define the SubmeshGeometry that cover different 
	// regions of the vertex/index buffers.

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


	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	//auto totalVertexCount = box.Vertices.size();
	auto totalVertexCount = grid.Vertices.size() + box.Vertices.size() + cylinder.Vertices.size() + pyramid.Vertices.size() + cone.Vertices.size() + torus.Vertices.size() + diamond.Vertices.size() + sphere.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::RosyBrown);
	}
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::LightYellow);
	}
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::LightGoldenrodYellow);
	}
	for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = pyramid.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Goldenrod);
	}
	for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Goldenrod);
	}
	for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = torus.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Yellow);
	}
	for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Gold);
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Color = XMFLOAT4(DirectX::Colors::Gold);
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
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->Name = "shapeGeo";

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

	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildPSOs()
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
	opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));

	//
	// PSO for opaque wireframe objects.
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaqueWireframePsoDesc = opaquePsoDesc;
	opaqueWireframePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaqueWireframePsoDesc, IID_PPV_ARGS(&mPSOs["opaque_wireframe"])));
}

//step6: build three frame resources
//FrameResource constructor:     FrameResource(ID3D12Device* device, UINT passCount, UINT objectCount);

void ShapesApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
			1, (UINT)mAllRitems.size()));
	}
}

void ShapesApp::BuildRenderItems()
{

	// floor
	auto gridRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&gridRitem->World, XMMatrixTranslation(1.0f, -1.0f, 1.0f));
	gridRitem->ObjCBIndex = 0;
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;  //36
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation; //0
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation; //0
	mAllRitems.push_back(std::move(gridRitem));

	// main building
	auto boxRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem->World, XMMatrixTranslation(1.0f, 3.0f, 1.0f));
	boxRitem->ObjCBIndex = 1;
	boxRitem->Geo = mGeometries["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem));

	// tower 1
	auto CylRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem->World, XMMatrixTranslation(5.5f, 3.0f, 5.5f));
	CylRitem->ObjCBIndex = 2;
	CylRitem->Geo = mGeometries["shapeGeo"].get();
	CylRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem->IndexCount = CylRitem->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem->StartIndexLocation = CylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem->BaseVertexLocation = CylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem));

	// tower 2
	auto CylRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem1->World, XMMatrixTranslation(-3.5f, 3.0f, 5.5f));
	CylRitem1->ObjCBIndex = 3;
	CylRitem1->Geo = mGeometries["shapeGeo"].get();
	CylRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem1->IndexCount = CylRitem1->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem1->StartIndexLocation = CylRitem1->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem1->BaseVertexLocation = CylRitem1->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem1));

	// tower 3
	auto CylRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem2->World, XMMatrixTranslation(-3.5f, 3.0f, -3.5f));
	CylRitem2->ObjCBIndex = 4;
	CylRitem2->Geo = mGeometries["shapeGeo"].get();
	CylRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem2->IndexCount = CylRitem2->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem2->StartIndexLocation = CylRitem2->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem2->BaseVertexLocation = CylRitem2->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem2));

	// tower 4
	auto CylRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem3->World, XMMatrixTranslation(5.5f, 3.0f, -3.5f));
	CylRitem3->ObjCBIndex = 5;
	CylRitem3->Geo = mGeometries["shapeGeo"].get();
	CylRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem3->IndexCount = CylRitem3->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem3->StartIndexLocation = CylRitem3->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem3->BaseVertexLocation = CylRitem3->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem3));

	// building top
	auto pyramidRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&pyramidRitem->World, XMMatrixTranslation(1.0f, 7.0f, 1.0f));
	pyramidRitem->ObjCBIndex = 6;
	pyramidRitem->Geo = mGeometries["shapeGeo"].get();
	pyramidRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	pyramidRitem->IndexCount = pyramidRitem->Geo->DrawArgs["pyramid"].IndexCount;
	pyramidRitem->StartIndexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].StartIndexLocation;
	pyramidRitem->BaseVertexLocation = pyramidRitem->Geo->DrawArgs["pyramid"].BaseVertexLocation;
	mAllRitems.push_back(std::move(pyramidRitem));
	// tower top 1
	auto coneRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem->World, XMMatrixTranslation(5.5f, 7.7f, 5.5f));
	coneRitem->ObjCBIndex = 7;
	coneRitem->Geo = mGeometries["shapeGeo"].get();
	coneRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem->IndexCount = coneRitem->Geo->DrawArgs["cone"].IndexCount;
	coneRitem->StartIndexLocation = coneRitem->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem->BaseVertexLocation = coneRitem->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem));

	// tower top 2
	auto coneRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem2->World, XMMatrixTranslation(-3.5f, 7.7f, 5.5f));
	coneRitem2->ObjCBIndex = 8;
	coneRitem2->Geo = mGeometries["shapeGeo"].get();
	coneRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem2->IndexCount = coneRitem2->Geo->DrawArgs["cone"].IndexCount;
	coneRitem2->StartIndexLocation = coneRitem2->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem2->BaseVertexLocation = coneRitem2->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem2));

	// tower top 3
	auto coneRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem3->World, XMMatrixTranslation(-3.5f, 7.7f, -3.5f));
	coneRitem3->ObjCBIndex = 9;
	coneRitem3->Geo = mGeometries["shapeGeo"].get();
	coneRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem3->IndexCount = coneRitem3->Geo->DrawArgs["cone"].IndexCount;
	coneRitem3->StartIndexLocation = coneRitem3->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem3->BaseVertexLocation = coneRitem3->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem3));

	//  tower top 3
	auto coneRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem4->World, XMMatrixTranslation(5.5f, 7.7f, -3.5f));
	coneRitem4->ObjCBIndex = 10;
	coneRitem4->Geo = mGeometries["shapeGeo"].get();
	coneRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem4->IndexCount = coneRitem4->Geo->DrawArgs["cone"].IndexCount;
	coneRitem4->StartIndexLocation = coneRitem4->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem4->BaseVertexLocation = coneRitem4->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem4));

	// building loop 
	auto torusRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&torusRitem4->World, XMMatrixTranslation(1.0f, 5.0f, -3.2f));
	torusRitem4->ObjCBIndex = 11;
	torusRitem4->Geo = mGeometries["shapeGeo"].get();
	torusRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	torusRitem4->IndexCount = torusRitem4->Geo->DrawArgs["torus"].IndexCount;
	torusRitem4->StartIndexLocation = torusRitem4->Geo->DrawArgs["torus"].StartIndexLocation;
	torusRitem4->BaseVertexLocation = torusRitem4->Geo->DrawArgs["torus"].BaseVertexLocation;
	mAllRitems.push_back(std::move(torusRitem4));

	auto diamondRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&diamondRitem4->World, XMMatrixTranslation(1.0f, 5.0f, -3.4f));
	diamondRitem4->ObjCBIndex = 12;
	diamondRitem4->Geo = mGeometries["shapeGeo"].get();
	diamondRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	diamondRitem4->IndexCount = diamondRitem4->Geo->DrawArgs["diamond"].IndexCount;
	diamondRitem4->StartIndexLocation = diamondRitem4->Geo->DrawArgs["diamond"].StartIndexLocation;
	diamondRitem4->BaseVertexLocation = diamondRitem4->Geo->DrawArgs["diamond"].BaseVertexLocation;
	mAllRitems.push_back(std::move(diamondRitem4));

	// wall 1
	auto boxRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem1->World, XMMatrixScaling(2.0f, 0.7f, 0.2f) * XMMatrixTranslation(1.0f, 1.8f, 10.2f));
	boxRitem1->ObjCBIndex = 13;
	boxRitem1->Geo = mGeometries["shapeGeo"].get();
	boxRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem1->IndexCount = boxRitem1->Geo->DrawArgs["box"].IndexCount;
	boxRitem1->StartIndexLocation = boxRitem1->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem1->BaseVertexLocation = boxRitem1->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem1));

	// wall 2
	auto boxRitem2 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem2->World, XMMatrixScaling(0.2f, 0.7f, 2.5f)* XMMatrixTranslation(9.5f, 1.8f, 1.0f));
	boxRitem2->ObjCBIndex = 14;
	boxRitem2->Geo = mGeometries["shapeGeo"].get();
	boxRitem2->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem2->IndexCount = boxRitem2->Geo->DrawArgs["box"].IndexCount;
	boxRitem2->StartIndexLocation = boxRitem2->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem2->BaseVertexLocation = boxRitem2->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem2));

	// wall 3
	auto boxRitem3 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem3->World, XMMatrixScaling(0.2f, 0.7f, 2.5f)* XMMatrixTranslation(-7.8f, 1.8f, 1.0f));
	boxRitem3->ObjCBIndex = 15;
	boxRitem3->Geo = mGeometries["shapeGeo"].get();
	boxRitem3->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem3->IndexCount = boxRitem3->Geo->DrawArgs["box"].IndexCount;
	boxRitem3->StartIndexLocation = boxRitem3->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem3->BaseVertexLocation = boxRitem3->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem3));

	// wall 4
	auto boxRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem4->World, XMMatrixScaling(0.8f, 0.7f, 0.2f)* XMMatrixTranslation(7.1f, 1.8f, -9.8f));
	boxRitem4->ObjCBIndex = 16;
	boxRitem4->Geo = mGeometries["shapeGeo"].get();
	boxRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem4->IndexCount = boxRitem4->Geo->DrawArgs["box"].IndexCount;
	boxRitem4->StartIndexLocation = boxRitem4->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem4->BaseVertexLocation = boxRitem4->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem4));

	// wall 5
	auto boxRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxRitem5->World, XMMatrixScaling(0.8f, 0.7f, 0.2f) * XMMatrixTranslation(-5.4f, 1.8f, -9.8f));
	boxRitem5->ObjCBIndex = 17;
	boxRitem5->Geo = mGeometries["shapeGeo"].get();
	boxRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem5->IndexCount = boxRitem5->Geo->DrawArgs["box"].IndexCount;
	boxRitem5->StartIndexLocation = boxRitem5->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem5->BaseVertexLocation = boxRitem5->Geo->DrawArgs["box"].BaseVertexLocation;
	mAllRitems.push_back(std::move(boxRitem5));

	// outer tower 1
	auto CylRitem4 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem4->World, XMMatrixScaling(1.0f, 1.3f, 1.0f) * XMMatrixTranslation(10.0f, 4.2f, 10.5f));
	CylRitem4->ObjCBIndex = 18;
	CylRitem4->Geo = mGeometries["shapeGeo"].get();
	CylRitem4->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem4->IndexCount = CylRitem4->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem4->StartIndexLocation = CylRitem4->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem4->BaseVertexLocation = CylRitem4->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem4));

	// outer tower 2
	auto CylRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem5->World, XMMatrixScaling(1.0f, 1.3f, 1.0f) * XMMatrixTranslation(-8.0f, 4.2f, 10.5f));
	CylRitem5->ObjCBIndex = 19;
	CylRitem5->Geo = mGeometries["shapeGeo"].get();
	CylRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem5->IndexCount = CylRitem5->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem5->StartIndexLocation = CylRitem5->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem5->BaseVertexLocation = CylRitem5->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem5));

	// outer tower 3
	auto CylRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem6->World, XMMatrixScaling(0.8f, 1.0f, 0.8f) * XMMatrixTranslation(-2.0f, 3.0f, -9.9f));
	CylRitem6->ObjCBIndex = 20;
	CylRitem6->Geo = mGeometries["shapeGeo"].get();
	CylRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem6->IndexCount = CylRitem6->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem6->StartIndexLocation = CylRitem6->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem6->BaseVertexLocation = CylRitem6->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem6));

	// outer tower 3
	auto CylRitem7 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&CylRitem7->World, XMMatrixScaling(0.8f, 1.0f, 0.8f) * XMMatrixTranslation(3.5f, 3.0f, -9.9f));
	CylRitem7->ObjCBIndex = 21;
	CylRitem7->Geo = mGeometries["shapeGeo"].get();
	CylRitem7->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	CylRitem7->IndexCount = CylRitem7->Geo->DrawArgs["cylinder"].IndexCount;
	CylRitem7->StartIndexLocation = CylRitem7->Geo->DrawArgs["cylinder"].StartIndexLocation;
	CylRitem7->BaseVertexLocation = CylRitem7->Geo->DrawArgs["cylinder"].BaseVertexLocation;
	mAllRitems.push_back(std::move(CylRitem7));

	// outer tower top 1
	auto sphereRitem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem->World, XMMatrixTranslation(3.5f, 7.6f, -9.9f));
	sphereRitem->ObjCBIndex = 22;
	sphereRitem->Geo = mGeometries["shapeGeo"].get();
	sphereRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem));

	// outer tower top 2
	auto sphereRitem1 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&sphereRitem1->World, XMMatrixTranslation(-2.0f, 7.6f, -9.9f));
	sphereRitem1->ObjCBIndex = 23;
	sphereRitem1->Geo = mGeometries["shapeGeo"].get();
	sphereRitem1->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	sphereRitem1->IndexCount = sphereRitem1->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem1->StartIndexLocation = sphereRitem1->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem1->BaseVertexLocation = sphereRitem1->Geo->DrawArgs["sphere"].BaseVertexLocation;
	mAllRitems.push_back(std::move(sphereRitem1));

	// outer tower top 3
	auto coneRitem5 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem5->World, XMMatrixTranslation(10.0f, 10.1f, 10.5f));
	coneRitem5->ObjCBIndex = 24;
	coneRitem5->Geo = mGeometries["shapeGeo"].get();
	coneRitem5->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem5->IndexCount = coneRitem5->Geo->DrawArgs["cone"].IndexCount;
	coneRitem5->StartIndexLocation = coneRitem5->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem5->BaseVertexLocation = coneRitem5->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem5));

	// outer tower top 4
	auto coneRitem6 = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&coneRitem6->World, XMMatrixTranslation(-8.0f, 10.1f, 10.5f));
	coneRitem6->ObjCBIndex = 25;
	coneRitem6->Geo = mGeometries["shapeGeo"].get();
	coneRitem6->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	coneRitem6->IndexCount = coneRitem6->Geo->DrawArgs["cone"].IndexCount;
	coneRitem6->StartIndexLocation = coneRitem6->Geo->DrawArgs["cone"].StartIndexLocation;
	coneRitem6->BaseVertexLocation = coneRitem6->Geo->DrawArgs["cone"].BaseVertexLocation;
	mAllRitems.push_back(std::move(coneRitem6));

	// All the render items are opaque.
	//Our application will maintain lists of render items based on how they need to be
	//drawn; that is, render items that need different PSOs will be kept in different lists.
	for (auto& e : mAllRitems)
		mOpaqueRitems.push_back(e.get());
}

void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
{
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

	auto objectCB = mCurrFrameResource->ObjectCB->Resource();

	// For each render item...
	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		// Offset to the CBV in the descriptor heap for this object and for this frame resource.
		UINT cbvIndex = mCurrFrameResourceIndex * (UINT)mOpaqueRitems.size() + ri->ObjCBIndex;
		auto cbvHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(mCbvHeap->GetGPUDescriptorHandleForHeapStart());
		cbvHandle.Offset(cbvIndex, mCbvSrvUavDescriptorSize);

		cmdList->SetGraphicsRootDescriptorTable(0, cbvHandle);

		cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
	}
}


