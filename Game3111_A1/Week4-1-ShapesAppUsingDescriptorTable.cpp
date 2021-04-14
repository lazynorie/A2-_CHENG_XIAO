//***************************************************************************************
// ShapesApp.cpp 

// Hold down '1' key to view scene in wireframe mode.
//***************************************************************************************

#include "d3dApp.h"
#include "MathHelper.h"
#include "UploadBuffer.h"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "FrameResource.h"
#include "Camera.h"

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const int gNumFrameResources = 3;


enum class RenderLayer : int
{
	Opaque = 0,
	Transparent,
	AlphaTested,
	AlphaTestedTreeSprites,
	Count
};

// Lightweight structure stores parameters to draw a shape.  This will
// vary from app-to-app.
struct RenderItem
{
	RenderItem() = default;
	RenderItem(const RenderItem& rhs) = delete;
    // World matrix of the shape that describes the object's local space
    // relative to the world space, which defines the position, orientation,
    // and scale of the object in the world.
    XMFLOAT4X4 World = MathHelper::Identity4x4();

    XMFLOAT4X4 TWorld = MathHelper::Identity4x4();

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

	std::vector<ObjectConstants> Instances;
	
    // DrawIndexedInstanced parameters.
    UINT IndexCount = 0;
    UINT StartIndexLocation = 0;
    int BaseVertexLocation = 0;
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
    void AnimateMaterials(const GameTimer& gt);
	void UpdateObjectCBs(const GameTimer& gt);
    void UpdateMaterialCBs(const GameTimer& gt);
	void UpdateMainPassCB(const GameTimer& gt);

    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();

    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
	void BuildTreeSpritesGeometry();
    void BuildPSOs();
    void BuildFrameResources();
    void BuildMaterials();
    void SetRenderItemInfo(RenderItem &Ritem, std::string itemType, XMMATRIX transform, std::string material, RenderLayer layer);
    void BuildRenderItems();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);
 
    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	float GetHillsHeight(float x, float z)const;
	XMFLOAT3 GetHillsNormal(float x, float z)const;
	XMFLOAT3 GetTreePosition(float minX, float maxX, float minZ, float maxZ, float treeHeightOffset)const;

private:

    std::vector<std::unique_ptr<FrameResource>> mFrameResources;
    FrameResource* mCurrFrameResource = nullptr;
    int mCurrFrameResourceIndex = 0;

    UINT mCbvSrvDescriptorSize = 0;

    ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
  //  ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;

	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mStdInputLayout;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mTreeSpriteInputLayout;

	RenderItem* mWavesRitem = nullptr;

	// List of all the render items.
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;


	// Render items divided by PSO.
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];

	UINT mInstanceCount = 0;
	bool mFrustumCullingEnabled = true;
    BoundingFrustum mCamFrustum;

	Camera mCamera;

    PassConstants mMainPassCB;

    UINT mPassCbvOffset = 0;

    bool mIsWireframe = false;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

    float mTheta = 1.5f*XM_PI;
    float mPhi = 0.2f*XM_PI;
    float mRadius = 65.0f;

    POINT mLastMousePos;

    UINT objCBIndex = 0;
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

ShapesApp::ShapesApp(HINSTANCE hInstance)
    : D3DApp(hInstance)
{
}

ShapesApp::~ShapesApp()
{
    if(md3dDevice != nullptr)
        FlushCommandQueue();
}

bool ShapesApp::Initialize()
{
    if(!D3DApp::Initialize())
        return false;

    // Reset the command list to prep for initialization commands.
    ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

    mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	mCamera.SetPosition(0.0f, 3.0f, -65.0f);

    LoadTextures();
    BuildRootSignature();
    BuildShadersAndInputLayout();
    BuildShapeGeometry();
	BuildTreeSpritesGeometry();
    BuildMaterials();
    BuildRenderItems();
    BuildFrameResources();
    BuildDescriptorHeaps();
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

	mCamera.SetLens(0.4f * MathHelper::Pi, AspectRatio(), 1.0f, 100.0f);

	BoundingFrustum::CreateFromMatrix(mCamFrustum, mCamera.GetProj());
    // The window resized, so update the aspect ratio and recompute the projection matrix.
    XMMATRIX P = XMMatrixPerspectiveFovLH(0.2f*MathHelper::Pi, AspectRatio(), 1.0f, 100.0f);
    XMStoreFloat4x4(&mProj, P);
}

void ShapesApp::Update(const GameTimer& gt)
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

void ShapesApp::Draw(const GameTimer& gt)
{
    auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;

    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(cmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(mCommandList->Reset(cmdListAlloc.Get(), mPSOs["opaque"].Get()));

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

	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["treeSprites"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

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
    if((btnState & MK_LBUTTON) != 0)
    {
        // Make each pixel correspond to a quarter of a degree.
        float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
        //float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		//mCamera.Pitch(dy);
		mCamera.RotateY(dx);

        // Update angles based on input to orbit camera around box.
        mTheta += dx;
        //mPhi += dy;

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
 
void ShapesApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();
	
	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * dt);

	if (GetAsyncKeyState('R') & 0x8000)
		mCamera.Pedestal(10.0f * dt);
	if (GetAsyncKeyState('F') & 0x8000)
		mCamera.Pedestal(-10.0f * dt);
    if(GetAsyncKeyState('1') & 0x8000)
        mIsWireframe = true;
    else
        mIsWireframe = false;
	mCamera.UpdateViewMatrix();
}

 
void ShapesApp::UpdateCamera(const GameTimer& gt)
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

void ShapesApp::AnimateMaterials(const GameTimer& gt)
{
	// Scroll the water material texture coordinates.
	auto waterMat = mMaterials["water0"].get();

	float& tu = waterMat->MatTransform(3, 0);
	float& tv = waterMat->MatTransform(3, 1);

	/*tu -= 0.1f * gt.DeltaTime();*/
	tv -= 0.2f * gt.DeltaTime();

	/*if (tu <= 0.0f)
		tu += 1.0f;*/

	if (tv <= 0.0f)
		tv += 1.0f;

	waterMat->MatTransform(3, 0) = tu;
	waterMat->MatTransform(3, 1) = tv;

	// Material has changed, so need to update cbuffer.
	waterMat->NumFramesDirty = gNumFrameResources;
}

void ShapesApp::UpdateObjectCBs(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);

	auto currObjectCB = mCurrFrameResource->ObjectCB.get();
	for (auto& e : mAllRitems)
	{
		// Only update the cbuffer data if the constants have changed.  
		// This needs to be tracked per frame resource.

		/*const auto& instanceData = e->Instances;

		int visibleInstanceCount = 0;

		for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			XMMATRIX world = XMLoadFloat4x4(&instanceData[i].World);
			XMMATRIX texTransform = XMLoadFloat4x4(&instanceData[i].TexTransform);

			XMMATRIX invWorld = XMMatrixInverse(&XMMatrixDeterminant(world), world);

			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

			// step3: Transform the camera frustum from view space to the object's local space.
			BoundingFrustum localSpaceFrustum;
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);
			}
		}*/
		if (e->NumFramesDirty > 0)
		{
			XMMATRIX world = XMLoadFloat4x4(&e->World);
			XMMATRIX tWorld = XMLoadFloat4x4(&e->TWorld);
			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);

			ObjectConstants objConstants;
			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
			XMStoreFloat4x4(&objConstants.TWorld, XMMatrixTranspose(MathHelper::InverseTranspose(world)));
			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));


			currObjectCB->CopyData(e->ObjCBIndex, objConstants);

			// Next FrameResource need to be updated too.
			e->NumFramesDirty--;

		}
	}
}

void ShapesApp::UpdateMaterialCBs(const GameTimer& gt)
{
	auto currMaterialCB = mCurrFrameResource->MaterialCB.get();
	for (auto& e : mMaterials)
	{
		// Only update the cbuffer data if the constants have changed.  If the cbuffer
		// data changes, it needs to be updated for each FrameResource.
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
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


void ShapesApp::UpdateMainPassCB(const GameTimer& gt)
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();

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
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();

    //lights
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.25f, 1.0f };
    
	mMainPassCB.Lights[0].Direction = { 0.0f, -1.0f, 0.0f };
	mMainPassCB.Lights[0].Strength = { 0.8f, 0.5, 0.3f };
   
	//Diamond light
	mMainPassCB.Lights[1].Position = { 0.0f, 6.0f, 0.0f };
	mMainPassCB.Lights[1].Strength = { 0.0f, 0.0f, 1.5f };

	//castle entry light
	mMainPassCB.Lights[2].Position = { 0.0f, 5.0f, -20.0f };
	mMainPassCB.Lights[2].Strength = { 0.0f, 1.0f, 1.0f };
	
	//four tower lights
	mMainPassCB.Lights[3].Position = { 20.0f, 5.0f, 20.0f };
	mMainPassCB.Lights[3].Strength = { 1.0f, 0.0f, 0.0f };
	
    mMainPassCB.Lights[4].Position = { 20.0f, 5.0f, -20.0f };
	mMainPassCB.Lights[4].Strength = { 0.0f, 1.0f, 0.0f };
	
	mMainPassCB.Lights[5].Position = { -20.0f, 5.0f, 20.0f };
	mMainPassCB.Lights[5].Strength = { 1.0f, 0.0f, 1.0f };

	mMainPassCB.Lights[6].Position = { -20.0f, 5.0f, -20.0f };
	mMainPassCB.Lights[6].Strength = { 0.0, 0.0f, 1.0f };

    //spotlight
    mMainPassCB.Lights[7].Position = { 0.0f, 15.0f, -60.0f };
    mMainPassCB.Lights[7].Direction = { 0.0f, -1.0f, 0.0f };
    mMainPassCB.Lights[7].SpotPower =  1.0f;
    mMainPassCB.Lights[7].Strength = { 2.1f, 2.1f, 2.1f };
    mMainPassCB.Lights[7].FalloffEnd = 20.0f;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

void ShapesApp::LoadTextures()
{
	auto bricksTex = std::make_unique<Texture>();
	bricksTex->Name = "bricksTex";
	bricksTex->Filename = L"Textures/bricks2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), bricksTex->Filename.c_str(),
		bricksTex->Resource, bricksTex->UploadHeap));

	auto stoneTex = std::make_unique<Texture>();
	stoneTex->Name = "stoneTex";
	stoneTex->Filename = L"Textures/stone.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), stoneTex->Filename.c_str(),
		stoneTex->Resource, stoneTex->UploadHeap));

	auto sandTex = std::make_unique<Texture>();
	sandTex->Name = "sandTex";
	sandTex->Filename = L"Textures/sand.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), sandTex->Filename.c_str(),
		sandTex->Resource, sandTex->UploadHeap));

    auto waterTex = std::make_unique<Texture>();
    waterTex->Name = "waterTex";
    waterTex->Filename = L"Textures/water1.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), waterTex->Filename.c_str(),
        waterTex->Resource, waterTex->UploadHeap));

    auto iceTex = std::make_unique<Texture>();
    iceTex->Name = "iceTex";
    iceTex->Filename = L"Textures/ice.dds";
    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
        mCommandList.Get(), iceTex->Filename.c_str(),
        iceTex->Resource, iceTex->UploadHeap));

	auto redBrickTex = std::make_unique<Texture>();
	redBrickTex->Name = "redBrickTex";
	redBrickTex->Filename = L"Textures/bricks3.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), redBrickTex->Filename.c_str(),
		redBrickTex->Resource, redBrickTex->UploadHeap));


	auto fenceTex = std::make_unique<Texture>();
	fenceTex->Name = "fenceTex";
	fenceTex->Filename = L"Textures/WireFence.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), fenceTex->Filename.c_str(),
		fenceTex->Resource, fenceTex->UploadHeap));

	auto treeArrayTex = std::make_unique<Texture>();
	treeArrayTex->Name = "treeArrayTex";
	treeArrayTex->Filename = L"Textures/treeArray.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeArrayTex->Filename.c_str(),
		treeArrayTex->Resource, treeArrayTex->UploadHeap));

	auto treeTex = std::make_unique<Texture>();
	treeTex->Name = "treeTex";
	treeTex->Filename = L"Textures/treeArray2.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), treeTex->Filename.c_str(),
		treeTex->Resource, treeTex->UploadHeap));

	auto grassTex = std::make_unique<Texture>();
	grassTex->Name = "grassTex";
	grassTex->Filename = L"Textures/grass.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(),
		mCommandList.Get(), grassTex->Filename.c_str(),
		grassTex->Resource, grassTex->UploadHeap));


	mTextures[bricksTex->Name] = std::move(bricksTex);
	mTextures[stoneTex->Name] = std::move(stoneTex);
	mTextures[sandTex->Name] = std::move(sandTex);
	mTextures[waterTex->Name] = std::move(waterTex);
	mTextures[iceTex->Name] = std::move(iceTex);
	mTextures[redBrickTex->Name] = std::move(redBrickTex);
	mTextures[fenceTex->Name] = std::move(fenceTex);
	mTextures[treeArrayTex->Name] = std::move(treeArrayTex);
	mTextures[treeTex->Name] = std::move(treeTex);
	mTextures[grassTex->Name] = std::move(grassTex);

	

}

//If we have 3 frame resources and n render items, then we have three 3n object constant
//buffers and 3 pass constant buffers.Hence we need 3(n + 1) constant buffer views(CBVs).
//Thus we will need to modify our CBV heap to include the additional descriptors :

void ShapesApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 10;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

	auto bricksTex = mTextures["bricksTex"]->Resource;
	auto stoneTex = mTextures["stoneTex"]->Resource;
	auto sandTex = mTextures["sandTex"]->Resource;
	auto redBrickTex = mTextures["redBrickTex"]->Resource;
	auto waterTex = mTextures["waterTex"]->Resource;
	auto iceTex = mTextures["iceTex"]->Resource;
	auto grassTex = mTextures["grassTex"]->Resource;
	auto fenceTex = mTextures["fenceTex"]->Resource;
	auto treeArrayTex = mTextures["treeArrayTex"]->Resource;
	auto treeTex = mTextures["treeTex"]->Resource;
	

	



	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = bricksTex->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = bricksTex->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(bricksTex.Get(), &srvDesc, hDescriptor);

	

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = stoneTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = stoneTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(stoneTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = sandTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = sandTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(sandTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = redBrickTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = redBrickTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(redBrickTex.Get(), &srvDesc, hDescriptor);

	//// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = waterTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = waterTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(waterTex.Get(), &srvDesc, hDescriptor);

    // next descriptor
    hDescriptor.Offset(1, mCbvSrvDescriptorSize);

    srvDesc.Format = iceTex->GetDesc().Format;
    srvDesc.Texture2D.MipLevels = iceTex->GetDesc().MipLevels;
    md3dDevice->CreateShaderResourceView(iceTex.Get(), &srvDesc, hDescriptor);

	


	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = grassTex->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = grassTex->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(grassTex.Get(), &srvDesc, hDescriptor);


	

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	srvDesc.Format = fenceTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(fenceTex.Get(), &srvDesc, hDescriptor);



	

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto desc = treeArrayTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeArrayTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeArrayTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeArrayTex.Get(), &srvDesc, hDescriptor);

	// next descriptor
	hDescriptor.Offset(1, mCbvSrvDescriptorSize);

	auto treedesc = treeTex->GetDesc();
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
	srvDesc.Format = treeTex->GetDesc().Format;
	srvDesc.Texture2DArray.MostDetailedMip = 0;
	srvDesc.Texture2DArray.MipLevels = -1;
	srvDesc.Texture2DArray.FirstArraySlice = 0;
	srvDesc.Texture2DArray.ArraySize = treeTex->GetDesc().DepthOrArraySize;
	md3dDevice->CreateShaderResourceView(treeTex.Get(), &srvDesc, hDescriptor);

	// next descriptor


	

	// next descriptor
	
}


//A root signature defines what resources need to be bound to the pipeline before issuing a draw call and
//how those resources get mapped to shader input registers. there is a limit of 64 DWORDs that can be put in a root signature.
void ShapesApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(
		D3D12_DESCRIPTOR_RANGE_TYPE_SRV,
		1,  // number of descriptors
		0); // register t0

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[4];

	// Performance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0); // register b0
	slotRootParameter[2].InitAsConstantBufferView(1); // register b1
	slotRootParameter[3].InitAsConstantBufferView(2); // register b2

	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
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
		const D3D_SHADER_MACRO defines[] =
	{
		"FOG", "1",
		NULL, NULL
	};

	const D3D_SHADER_MACRO alphaTestDefines[] =
	{
		"FOG", "1",
		"ALPHA_TEST", "1",
		NULL, NULL
	};
	mShaders["standardVS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", defines, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = d3dUtil::CompileShader(L"Shaders\\color.hlsl", alphaTestDefines, "PS", "ps_5_1");

	mShaders["treeSpriteVS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["treeSpriteGS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", nullptr, "GS", "gs_5_1");
	mShaders["treeSpritePS"] = d3dUtil::CompileShader(L"Shaders\\TreeSprite.hlsl", alphaTestDefines, "PS", "ps_5_1");
	
	mStdInputLayout =
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


void ShapesApp::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(90, 150 , 60 , 40);
    GeometryGenerator::MeshData sandDunes = geoGen.CreateGrid(200, 200, 60 * 4, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.5f, 2.0f, 20, 20);
	GeometryGenerator::MeshData cone = geoGen.CreateCone(0.5f, 1.0f, 20, 1);
    GeometryGenerator::MeshData triPrism = geoGen.CreateTriangularPrism(1, 1, 1);
    GeometryGenerator::MeshData diamond = geoGen.CreateDiamond(1.0f, 0.0f, 1.0f, 1.0f, 6, 1);
    GeometryGenerator::MeshData pyramid = geoGen.CreatePyramid(1, 1, 1 );
	GeometryGenerator::MeshData torus = geoGen.CreateTorus(0.1f, 1.0f, 20, 20);
	GeometryGenerator::MeshData wedge = geoGen.CreateWedge(1.0f, 1.0f, 2.0f);
    //GeometryGenerator::MeshData waterGrid = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);
    

	//
	// We are concatenating all the geometry into one big vertex/index buffer.  So
	// define the regions in the buffer each submesh covers.
	//

	// Cache the vertex offsets to each object in the concatenated vertex buffer.
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
    UINT sandDunesVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
    UINT sphereVertexOffset = sandDunesVertexOffset + (UINT)sandDunes.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
    UINT coneVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();
    UINT triPrismVertexOffset = coneVertexOffset + (UINT)cone.Vertices.size();
    UINT diamondVertexOffset = triPrismVertexOffset + (UINT)triPrism.Vertices.size();
    UINT pyramidVertexOffset = diamondVertexOffset + (UINT)diamond.Vertices.size();
    UINT torusVertexOffset = pyramidVertexOffset + (UINT)pyramid.Vertices.size();
    UINT wedgeVertexOffset = torusVertexOffset + (UINT)torus.Vertices.size();

   
    
	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sandDunesIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
    UINT sphereIndexOffset = sandDunesIndexOffset + (UINT)sandDunes.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
    UINT coneIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();
    UINT triPrismIndexOffset = coneIndexOffset + (UINT)cone.Indices32.size();
    UINT diamondIndexOffset = triPrismIndexOffset + (UINT)triPrism.Indices32.size();
    UINT pyramidIndexOffset = diamondIndexOffset + (UINT)diamond.Indices32.size();
    UINT torusIndexOffset = pyramidIndexOffset + (UINT)pyramid.Indices32.size();
    UINT wedgeIndexOffset = torusIndexOffset + (UINT)torus.Indices32.size(); 
 
    

    // Define the SubmeshGeometry that cover different 
    // regions of the vertex/index buffers.

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

    SubmeshGeometry wedgeSubmesh;
    wedgeSubmesh.IndexCount = (UINT)wedge.Indices32.size();
    wedgeSubmesh.StartIndexLocation = wedgeIndexOffset;
    wedgeSubmesh.BaseVertexLocation = wedgeVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

    SubmeshGeometry sandDunesSubmesh;
    sandDunesSubmesh.IndexCount = (UINT)sandDunes.Indices32.size();
    sandDunesSubmesh.StartIndexLocation = sandDunesIndexOffset;
    sandDunesSubmesh.BaseVertexLocation = sandDunesVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

    SubmeshGeometry coneSubmesh;
    coneSubmesh.IndexCount = (UINT)cone.Indices32.size();
    coneSubmesh.StartIndexLocation = coneIndexOffset;
    coneSubmesh.BaseVertexLocation = coneVertexOffset;

    SubmeshGeometry triPrismSubmesh;
    triPrismSubmesh.IndexCount = (UINT)triPrism.Indices32.size();
    triPrismSubmesh.StartIndexLocation = triPrismIndexOffset;
    triPrismSubmesh.BaseVertexLocation = triPrismVertexOffset;

    SubmeshGeometry diamondSubmesh;
    diamondSubmesh.IndexCount = (UINT)diamond.Indices32.size();
    diamondSubmesh.StartIndexLocation = diamondIndexOffset;
    diamondSubmesh.BaseVertexLocation = diamondVertexOffset;

    SubmeshGeometry pyramidSubmesh;
    pyramidSubmesh.IndexCount = (UINT)pyramid.Indices32.size();
    pyramidSubmesh.StartIndexLocation = pyramidIndexOffset;
    pyramidSubmesh.BaseVertexLocation = pyramidVertexOffset;  

    SubmeshGeometry torusSubmesh;
    torusSubmesh.IndexCount = (UINT)torus.Indices32.size();
    torusSubmesh.StartIndexLocation = torusIndexOffset;
    torusSubmesh.BaseVertexLocation = torusVertexOffset;



	//
	// Extract the vertex elements we are interested in and pack the
	// vertices of all the meshes into one vertex buffer.
	//

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sandDunes.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		cone.Vertices.size() +
		triPrism.Vertices.size() +
		diamond.Vertices.size() +
		pyramid.Vertices.size() +
		torus.Vertices.size() +
		wedge.Vertices.size();
	
      
	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for(size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
        vertices[k].Normal = box.Vertices[i].Normal;
        vertices[k].TexC = box.Vertices[i].TexC;
	}

	for(size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
	}
    for (size_t i = 0; i < sandDunes.Vertices.size(); ++i, ++k)
    {
		auto& p = sandDunes.Vertices[i].Position;
		vertices[k].Pos = p;
		vertices[k].Pos.y = GetHillsHeight(p.x, p.z);
		vertices[k].Normal = GetHillsNormal(p.x, p.z);
		vertices[k].TexC = sandDunes.Vertices[i].TexC;
    }

	for(size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
	}

	for(size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
	}

    for (size_t i = 0; i < cone.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = cone.Vertices[i].Position;
		vertices[k].Normal = cone.Vertices[i].Normal;
		vertices[k].TexC = cone.Vertices[i].TexC;
    }

    for (size_t i = 0; i < triPrism.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = triPrism.Vertices[i].Position;
		vertices[k].Normal = triPrism.Vertices[i].Normal;
		vertices[k].TexC = triPrism.Vertices[i].TexC;
    }

    for (size_t i = 0; i < diamond.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = diamond.Vertices[i].Position;
		vertices[k].Normal = diamond.Vertices[i].Normal;
		vertices[k].TexC = diamond.Vertices[i].TexC;
    }

    for (size_t i = 0; i < pyramid.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = pyramid.Vertices[i].Position;
		vertices[k].Normal = pyramid.Vertices[i].Normal;
		vertices[k].TexC = pyramid.Vertices[i].TexC;
    }

     for (size_t i = 0; i < torus.Vertices.size(); ++i, ++k)
    {
        vertices[k].Pos = torus.Vertices[i].Position;
		vertices[k].Normal = torus.Vertices[i].Normal;
		vertices[k].TexC = torus.Vertices[i].TexC;
    }
     for (size_t i = 0; i < wedge.Vertices.size(); ++i, ++k)
     {
         vertices[k].Pos = wedge.Vertices[i].Position;
		 vertices[k].Normal = wedge.Vertices[i].Normal;
		 vertices[k].TexC = wedge.Vertices[i].TexC;
     }



	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
    indices.insert(indices.end(), std::begin(sandDunes.GetIndices16()), std::end(sandDunes.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(cone.GetIndices16()), std::end(cone.GetIndices16()));
	indices.insert(indices.end(), std::begin(triPrism.GetIndices16()), std::end(triPrism.GetIndices16()));
	indices.insert(indices.end(), std::begin(diamond.GetIndices16()), std::end(diamond.GetIndices16()));
	indices.insert(indices.end(), std::begin(pyramid.GetIndices16()), std::end(pyramid.GetIndices16()));
	indices.insert(indices.end(), std::begin(torus.GetIndices16()), std::end(torus.GetIndices16()));
    indices.insert(indices.end(), std::begin(wedge.GetIndices16()), std::end(wedge.GetIndices16()));


    const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
    const UINT ibByteSize = (UINT)indices.size()  * sizeof(std::uint16_t);

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

	geo->DrawArgs["box"] = boxSubmesh;
	geo->DrawArgs["grid"] = gridSubmesh;
    geo->DrawArgs["sandDunes"] = sandDunesSubmesh;
	geo->DrawArgs["sphere"] = sphereSubmesh;
	geo->DrawArgs["cylinder"] = cylinderSubmesh;
	geo->DrawArgs["cone"] = coneSubmesh;
	geo->DrawArgs["prism"] = triPrismSubmesh;
	geo->DrawArgs["diamond"] = diamondSubmesh;
	geo->DrawArgs["pyramid"] = pyramidSubmesh;
	geo->DrawArgs["torus"] = torusSubmesh;
    geo->DrawArgs["wedge"] = wedgeSubmesh;

	mGeometries[geo->Name] = std::move(geo);
}

void ShapesApp::BuildTreeSpritesGeometry()
{
	
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	const float m_size = 15.0f;
	const float m_halfHeight = m_size/2.4f; 

	static const int treeCount = 30;
	std::array<TreeSpriteVertex, treeCount> vertices;
	//left side 
	for(UINT i = 0; i < treeCount*0.3; ++i)
	{
		vertices[i].Pos = GetTreePosition(-40, -30, -60, 30, m_halfHeight);
		vertices[i].Size = XMFLOAT2(m_size, m_size);
	}
	//right side
	for(UINT i = treeCount*0.3; i < treeCount * 0.6; ++i)
	{
		vertices[i].Pos = vertices[i].Pos = GetTreePosition(30, 40, -60, 30, m_halfHeight);
		vertices[i].Size = XMFLOAT2(m_size, m_size);
	}


	//front side
	for (UINT i = treeCount * 0.6; i < treeCount * 0.8; ++i)
	{
		vertices[i].Pos = vertices[i].Pos = GetTreePosition(-40, 40, -70, -80, m_halfHeight);
		vertices[i].Size = XMFLOAT2(m_size, m_size);
	}


	//back side
	for (UINT i = treeCount * 0.8; i < treeCount; ++i)
	{
		vertices[i].Pos = vertices[i].Pos = GetTreePosition(-40, 40, 40, 50, m_halfHeight);
		vertices[i].Size = XMFLOAT2(m_size, m_size);
	}
	
	

	std::array<std::uint16_t, treeCount> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15,
		16, 17, 18, 19 ,20, 21, 22, 
		23, 24, 25, 26, 27, 28, 29
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

void ShapesApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

	//
	// PSO for opaque objects.
	//
	ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	opaquePsoDesc.InputLayout = { mStdInputLayout.data(), (UINT)mStdInputLayout.size() };
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

	//there is abug with F2 key that is supposed to turn on the multisampling!
//Set4xMsaaState(true);
	//m4xMsaaState = true;

	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
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

	//transparentPsoDesc.BlendState.AlphaToCoverageEnable = true;

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

void ShapesApp::BuildFrameResources()
{
    for(int i = 0; i < gNumFrameResources; ++i)
    {
        mFrameResources.push_back(std::make_unique<FrameResource>(md3dDevice.Get(),
            1, (UINT)mAllRitems.size(), (UINT)mMaterials.size()));
    }
}

void ShapesApp::BuildMaterials()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(1.2f, 1.2f, 0.2f);
	bricks0->Roughness = 0.5f;

	

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->MatCBIndex = 1;
	stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(0.8f, 0.8f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	stone0->Roughness = 0.9f;

	auto sand0 = std::make_unique<Material>();
	sand0->Name = "sand0";
	sand0->MatCBIndex = 2;
	sand0->DiffuseSrvHeapIndex = 2;
	sand0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sand0->FresnelR0 = XMFLOAT3(0.6f, 0.6f, 0.6f);
	sand0->Roughness = 0.95f;

	auto redbrick0 = std::make_unique<Material>();
	redbrick0->Name = "redbrick0";
	redbrick0->MatCBIndex = 3;
	redbrick0->DiffuseSrvHeapIndex = 3;
	redbrick0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	redbrick0->FresnelR0 = XMFLOAT3(0.6f, 0.6f, 0.6f);
	redbrick0->Roughness = 0.3f;

	auto Water0 = std::make_unique<Material>();
	Water0->Name = "water0";
	Water0->MatCBIndex = 4;
	Water0->DiffuseSrvHeapIndex = 4;
	Water0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);
	Water0->FresnelR0 = XMFLOAT3(1.0f, 1.0f, 1.0f);
	Water0->Roughness = 0.0f;

	auto Ice0 = std::make_unique<Material>();
	Ice0->Name = "ice0";
	Ice0->MatCBIndex = 5;
	Ice0->DiffuseSrvHeapIndex = 5;
	Ice0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.8f);
	Ice0->FresnelR0 = XMFLOAT3(1.0f, 1.0f, 1.0f);
	Ice0->Roughness = 0.1f;

	auto grass0 = std::make_unique<Material>();
	grass0->Name = "grass0";
	grass0->MatCBIndex = 6;
	grass0->DiffuseSrvHeapIndex = 6;
	grass0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	grass0->FresnelR0 = XMFLOAT3(0.2f, 0.2f, 0.2f);
	grass0->Roughness = 0.7f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->MatCBIndex = 7;
	wirefence->DiffuseSrvHeapIndex = 7;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	wirefence->Roughness = 0.25f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "treeSprites";
	treeSprites->MatCBIndex = 8;
	treeSprites->DiffuseSrvHeapIndex = 8;
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	auto treeSprite = std::make_unique<Material>();
	treeSprite->Name = "treeSprite";
	treeSprite->MatCBIndex = 9;
	treeSprite->DiffuseSrvHeapIndex = 9;
	treeSprite->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprite->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprite->Roughness = 0.125f;

	

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["stone0"] = std::move(stone0);
	mMaterials["redbrick0"] = std::move(redbrick0);
	mMaterials["ice0"] = std::move(Ice0);
	mMaterials["water0"] = std::move(Water0);
	mMaterials["sand0"] = std::move(sand0);
	mMaterials["wirefence"] = std::move(wirefence);
	mMaterials["treeSprites"] = std::move(treeSprites);	
	mMaterials["treeSprite"] = std::move(treeSprite);
	mMaterials["grass0"] = std::move(grass0);

}

//makes building render items simpler, reduces repeated chunks of code
//the itemType is the key used to access the submesh
void ShapesApp::SetRenderItemInfo(RenderItem& Ritem, std::string itemType, XMMATRIX transform, std::string material, RenderLayer layer)
{
    Ritem.ObjCBIndex = objCBIndex++;
    XMStoreFloat4x4(&Ritem.World, transform);
    Ritem.Mat = mMaterials[material].get();
    Ritem.Mat->NormalSrvHeapIndex = 1;
    Ritem.Geo = mGeometries["shapeGeo"].get();
    Ritem.PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    Ritem.IndexCount = Ritem.Geo->DrawArgs[itemType].IndexCount;
    Ritem.StartIndexLocation = Ritem.Geo->DrawArgs[itemType].StartIndexLocation;
    Ritem.BaseVertexLocation = Ritem.Geo->DrawArgs[itemType].BaseVertexLocation;
    

     mRitemLayer[(int)layer].push_back(&Ritem);
   
}

void ShapesApp::BuildRenderItems()
{

	float nintydegrees = XM_2PI / 4;
	float x1, z1;  //using trig to change b/w x axis and z axix
	x1 = z1 = 20;
	float radius = sqrt(x1 * x1 + z1 * z1);

   

    auto gridRitem = std::make_unique<RenderItem>();
	XMMATRIX gridWorld = XMMatrixScaling(90.0f, 1.8f, 180.0f) * XMMatrixTranslation(0, 0, -10);
    SetRenderItemInfo(*gridRitem, "box",gridWorld, "sand0", RenderLayer::Opaque);
	XMStoreFloat4x4(&gridRitem->TexTransform, XMMatrixScaling(10.0f, 20.0f, 10.0f));

	mAllRitems.push_back(std::move(gridRitem));

   

    //towers
    for (int i = 0; i < 4; ++i)
    {
		float theta = i * nintydegrees + XM_2PI / 8; //90+45 =135
		float sRadius = radius * sinf(theta);
		float cRadius = radius * cosf(theta);

        auto towerRitem = std::make_unique<RenderItem>();
        auto ttopRitem = std::make_unique<RenderItem>();
        auto donutRitem = std::make_unique<RenderItem>();
        
       

        XMMATRIX towerWorld = XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixTranslation(cRadius, 3.5f, sRadius);
		XMStoreFloat4x4(&towerRitem->TexTransform, XMMatrixScaling(4.0f, 4.0f, 4.0f));

        XMMATRIX ttopWorld = XMMatrixScaling(5.0f, 4.0f, 5.0f) * XMMatrixTranslation(cRadius, 8.5f, sRadius);
		XMStoreFloat4x4(&ttopRitem->TexTransform, XMMatrixScaling(5.0f, 4.0f, 5.0f));

        XMMATRIX donutWorld = XMMatrixScaling(2.5f, 3.0f, 2.5f) * XMMatrixTranslation(cRadius, 7.0f, sRadius);
		XMStoreFloat4x4(&donutRitem->TexTransform, XMMatrixScaling(2.5f, 3.0f, 2.5f));


        SetRenderItemInfo(*towerRitem, "cylinder", towerWorld, "redbrick0", RenderLayer::Opaque);

        SetRenderItemInfo(*ttopRitem, "cone", ttopWorld, "redbrick0", RenderLayer::Opaque);

		SetRenderItemInfo(*donutRitem, "torus", donutWorld, "sand0", RenderLayer::Opaque);

       
   

        mAllRitems.push_back(std::move(towerRitem));
        mAllRitems.push_back(std::move(ttopRitem));
        mAllRitems.push_back(std::move(donutRitem));
        

       
    }

	//frontwall
	for (int i = 0; i < 2; i++)
	{

		float theta = i * nintydegrees;


		if (i < 2)
		{
			auto FrontWallRitem = std::make_unique<RenderItem>();
			auto walltopRitem = std::make_unique<RenderItem>();

			XMMATRIX FrontWallWorld = XMMatrixScaling(16.0f, 5.0f, 1.0f) * XMMatrixTranslation(  -12 + 24*i , 2.5f, -20.0f);
			XMStoreFloat4x4(&FrontWallRitem->TexTransform, XMMatrixScaling(8.0f, 2.0f, 1.0f));

			XMMATRIX walltopWorld = XMMatrixScaling(40.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 5.3f, -20.0f);
			XMStoreFloat4x4(&walltopRitem->TexTransform, XMMatrixScaling(20.0f, 0.5f, 1.0f));

			SetRenderItemInfo(*FrontWallRitem, "box", FrontWallWorld, "bricks0", RenderLayer::Opaque);
			SetRenderItemInfo(*walltopRitem, "prism", walltopWorld, "bricks0", RenderLayer::Opaque);

			mAllRitems.push_back(std::move(FrontWallRitem));
			mAllRitems.push_back(std::move(walltopRitem));
		}
	}


	//walls
	for (int i = 0; i < 3; i++)
	{
		float theta = i * nintydegrees;
		float sinR = x1 * sinf(theta);
		float cosR = x1 * cosf(theta);

		if (i<3)
		{
			auto wallRitem = std::make_unique<RenderItem>();
			auto walltopRitem = std::make_unique<RenderItem>();

			XMMATRIX wallWorld = XMMatrixScaling(1.0f, 5.0f, 40.0f) * XMMatrixRotationY(theta) * XMMatrixTranslation(cosR, 2.5f, sinR);
			XMStoreFloat4x4(&wallRitem->TexTransform, XMMatrixScaling(20.0f, 2.5f, 1.0f));

			XMMATRIX walltopWorld = XMMatrixScaling(2.0f, 1.0f, 40.0f) * XMMatrixRotationY(theta) * XMMatrixTranslation(cosR, 5.0f, sinR);
			XMStoreFloat4x4(&walltopRitem->TexTransform, XMMatrixScaling(20.0f, 0.5f, 1.0f));

			SetRenderItemInfo(*wallRitem, "box", wallWorld, "bricks0", RenderLayer::Opaque);
			SetRenderItemInfo(*walltopRitem, "box", walltopWorld, "bricks0", RenderLayer::Opaque);

			mAllRitems.push_back(std::move(wallRitem));
			mAllRitems.push_back(std::move(walltopRitem));

		}
	}

	//mazewalls
	auto wall2Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall2World = XMMatrixScaling(1.0f, 4.0f, 40.0f) * XMMatrixTranslation(25.0f, 2.5f, -40.0f);
	SetRenderItemInfo(*wall2Ritem, "box", wall2World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall2Ritem));
	auto wall3Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall3World = XMMatrixScaling(1.0f, 4.0f, 40.0f) * XMMatrixTranslation(-25.0f, 2.5f, -40.0f);
	SetRenderItemInfo(*wall3Ritem, "box", wall3World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall3Ritem));
	auto wall4Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall4World = XMMatrixScaling(4.0f, 4.0f, 1.0f) * XMMatrixTranslation(23.5f, 2.5f, -19.5f);
	SetRenderItemInfo(*wall4Ritem, "box", wall4World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall4Ritem));
	auto wall5Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall5World = XMMatrixScaling(4.0f, 4.0f, 1.0f) * XMMatrixTranslation(-23.5f, 2.5f, -19.5f);
	SetRenderItemInfo(*wall5Ritem, "box", wall5World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall5Ritem));
	auto wall6Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall6World = XMMatrixScaling(23.0f, 4.0f, 1.0f) * XMMatrixTranslation(-14.0f, 2.5f, -60.0f);
	SetRenderItemInfo(*wall6Ritem, "box", wall6World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall6Ritem));
	auto wall7Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall7World = XMMatrixScaling(23.0f, 4.0f, 1.0f) * XMMatrixTranslation(14.0f, 2.5f, -60.0f);
	SetRenderItemInfo(*wall7Ritem, "box", wall7World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall7Ritem));
	auto wall8Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall8World = XMMatrixScaling(40.0f, 4.0f, 1.0f) * XMMatrixTranslation(0.0f, 2.5f, -55.5f);
	SetRenderItemInfo(*wall8Ritem, "box", wall8World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall8Ritem));
	auto wall9Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall9World = XMMatrixScaling(21.0f, 4.0f, 1.0f) * XMMatrixTranslation(14.0f, 2.5f, -50.0f);
	SetRenderItemInfo(*wall9Ritem, "box", wall9World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall9Ritem));
	auto wall10Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall10World = XMMatrixScaling(24.0f, 4.0f, 1.0f) * XMMatrixTranslation(13.0f, 2.5f, -40.0f);
	SetRenderItemInfo(*wall10Ritem, "box", wall10World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall10Ritem));
	auto wall11Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall11World = XMMatrixScaling(27.0f, 4.0f, 1.0f) * XMMatrixTranslation(3.5f, 2.5f, -30.0f);
	SetRenderItemInfo(*wall11Ritem, "box", wall11World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall11Ritem));
	auto wall12Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall12World = XMMatrixScaling(10.5f, 4.0f, 1.0f) * XMMatrixTranslation(-20.0f, 2.5f, -35.0f);
	SetRenderItemInfo(*wall12Ritem, "box", wall12World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall12Ritem));
	auto wall13Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall13World = XMMatrixScaling(5.0f, 4.0f, 1.0f) * XMMatrixTranslation(-12.0f, 2.5f, -50.0f);
	SetRenderItemInfo(*wall13Ritem, "box", wall13World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall13Ritem));
	auto wall14Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall14World = XMMatrixScaling(5.5f, 4.0f, 1.0f) * XMMatrixTranslation(-22.5f, 2.5f, -47.0f);
	SetRenderItemInfo(*wall14Ritem, "box", wall14World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall14Ritem));
	auto wall15Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall15World = XMMatrixScaling(1.0f, 4.0f, 15.0f) * XMMatrixTranslation(-19.5f, 2.5f, -47.5f);
	SetRenderItemInfo(*wall15Ritem, "box", wall15World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall15Ritem));
	auto wall16Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall16World = XMMatrixScaling(1.0f, 4.0f, 16.0f) * XMMatrixTranslation(-14.5f, 2.5f, -42.5f);
	SetRenderItemInfo(*wall16Ritem, "box", wall16World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall16Ritem));
	auto wall17Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall17World = XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(-9.5f, 2.5f, -40.5f);
	SetRenderItemInfo(*wall17Ritem, "box", wall17World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall17Ritem));
	auto wall18Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall18World = XMMatrixScaling(1.0f, 4.0f, 20.0f) * XMMatrixTranslation(-3.5f, 2.5f, -40.5f);
	SetRenderItemInfo(*wall18Ritem, "box", wall18World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall18Ritem));
	auto wall19Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall19World = XMMatrixScaling(23.0f, 4.0f, 1.0f) * XMMatrixTranslation(8.5f, 2.5f, -45.0f);
	SetRenderItemInfo(*wall19Ritem, "box", wall19World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall19Ritem));
	auto wall20Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall20World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(1.5f, 2.5f, -37.5f);
	SetRenderItemInfo(*wall20Ritem, "box", wall20World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall20Ritem));
	auto wall21Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall21World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(6.5f, 2.5f, -32.5f);
	SetRenderItemInfo(*wall21Ritem, "box", wall21World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall21Ritem));
	auto wall22Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall22World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(11.5f, 2.5f, -37.5f);
	SetRenderItemInfo(*wall22Ritem, "box", wall22World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall22Ritem));
	auto wall23Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall23World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(6.5f, 2.5f, -22.5f);
	SetRenderItemInfo(*wall23Ritem, "box", wall23World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall23Ritem));
	auto wall24Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall24World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(16.5f, 2.5f, -27.5f);
	SetRenderItemInfo(*wall24Ritem, "box", wall24World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall24Ritem));
	auto wall25Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall25World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(-6.5f, 2.5f, -22.5f);
	SetRenderItemInfo(*wall25Ritem, "box", wall25World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall25Ritem));
	auto wall26Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall26World = XMMatrixScaling(10.0f, 4.0f, 1.0f) * XMMatrixTranslation(-11.0f, 2.5f, -25.0f);
	SetRenderItemInfo(*wall26Ritem, "box", wall26World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall26Ritem));
	auto wall27Ritem = std::make_unique<RenderItem>();
	XMMATRIX wall27World = XMMatrixScaling(1.0f, 4.0f, 5.0f) * XMMatrixTranslation(-16.0f, 2.5f, -27.0f);
	SetRenderItemInfo(*wall27Ritem, "box", wall27World, "grass0", RenderLayer::Opaque);
	mAllRitems.push_back(std::move(wall27Ritem));

	//battlements
	for (int i = 0; i < 21; i++)
	{
		auto brick1Ritem = std::make_unique<RenderItem>();
		auto brick2Ritem = std::make_unique<RenderItem>();
		auto brick3Ritem = std::make_unique<RenderItem>();
		auto brick4Ritem = std::make_unique<RenderItem>();

		XMMATRIX brick1World = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(-20.0f, 5.5f, 20.0f - 2 * i);
		XMMATRIX brick2World = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(20.0f, 5.5f, 20.0f - 2 * i);
		XMMATRIX brick3World = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(20.0f - 2 * i, 5.5f, 20.0f);
		XMMATRIX brick4World = XMMatrixScaling(1.0f, 1.5f, 1.0f) * XMMatrixTranslation(20.0f - 2 * i, 5.5f, -20.0f);

		SetRenderItemInfo(*brick1Ritem, "box", brick1World, "stone0", RenderLayer::Opaque);
		SetRenderItemInfo(*brick2Ritem, "box", brick2World, "stone0", RenderLayer::Opaque);
		SetRenderItemInfo(*brick3Ritem, "box", brick3World, "stone0", RenderLayer::Opaque);
		SetRenderItemInfo(*brick4Ritem, "box", brick4World, "stone0", RenderLayer::Opaque);

		mAllRitems.push_back(std::move(brick1Ritem));
		mAllRitems.push_back(std::move(brick2Ritem));
		mAllRitems.push_back(std::move(brick3Ritem));
		mAllRitems.push_back(std::move(brick4Ritem));
	}

	auto diamondRitem = std::make_unique<RenderItem>();
	XMMATRIX DiamondWorld = XMMatrixScaling(1.0f, 1.0f, 1.0f) * XMMatrixTranslation(0.0f, 4.5f, 0.0f);
	SetRenderItemInfo(*diamondRitem, "diamond", DiamondWorld, "ice0", RenderLayer::Transparent);
	mAllRitems.push_back(std::move(diamondRitem));

	auto wedgeRitem = std::make_unique<RenderItem>();
	XMMATRIX wedgeWorld = XMMatrixScaling(5.0f, 1.0f, 5.0f) * XMMatrixRotationY(-nintydegrees) * XMMatrixTranslation(0.0f, 1.2f, -23.0f);
	SetRenderItemInfo(*wedgeRitem, "wedge", wedgeWorld, "wirefence", RenderLayer::Transparent);
	mAllRitems.push_back(std::move(wedgeRitem));

	auto pyramidRitem = std::make_unique<RenderItem>();
	XMMATRIX pyramidWorld = XMMatrixScaling(4.0f, 4.0f, 4.0f) * XMMatrixRotationY(-nintydegrees) * XMMatrixTranslation(0.0f, 1.5f, 0.0f);
	SetRenderItemInfo(*pyramidRitem, "pyramid", pyramidWorld, "stone0", RenderLayer::Transparent);
	mAllRitems.push_back(std::move(pyramidRitem));


	auto waterRitem = std::make_unique<RenderItem>();
	XMMATRIX WaterWorld = XMMatrixScaling(5.0f, 5.0f, 5.0f) * XMMatrixTranslation( 1.5, -1.5 ,  1.5);
	XMStoreFloat4x4(&waterRitem->TexTransform, XMMatrixScaling(5.0f, 5.0f, 1.0f));
	SetRenderItemInfo(*waterRitem, "grid", WaterWorld, "water0", RenderLayer::Transparent);
	mAllRitems.push_back(std::move(waterRitem));

	


	auto treeSpritesRitem = std::make_unique<RenderItem>();
	treeSpritesRitem->World = MathHelper::Identity4x4();
	treeSpritesRitem->ObjCBIndex = objCBIndex++;
	treeSpritesRitem->Mat = mMaterials["treeSprite"].get();
	treeSpritesRitem->Geo = mGeometries["treeSpritesGeo"].get();
	treeSpritesRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	treeSpritesRitem->IndexCount = treeSpritesRitem->Geo->DrawArgs["points"].IndexCount;
	treeSpritesRitem->StartIndexLocation = treeSpritesRitem->Geo->DrawArgs["points"].StartIndexLocation;
	treeSpritesRitem->BaseVertexLocation = treeSpritesRitem->Geo->DrawArgs["points"].BaseVertexLocation;
	mRitemLayer[(int)RenderLayer::AlphaTestedTreeSprites].push_back(treeSpritesRitem.get());
	mAllRitems.push_back(std::move(treeSpritesRitem));



}


//The DrawRenderItems method is invoked in the main Draw call:
void ShapesApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems)
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

		CD3DX12_GPU_DESCRIPTOR_HANDLE tex(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
		tex.Offset(ri->Mat->DiffuseSrvHeapIndex, mCbvSrvDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->Mat->MatCBIndex * matCBByteSize;

		cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->IndexCount, 1, ri->StartIndexLocation, ri->BaseVertexLocation, 0);
    }

}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> ShapesApp::GetStaticSamplers()
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


float ShapesApp::GetHillsHeight(float x, float z)const
{
	return 0.1f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 ShapesApp::GetHillsNormal(float x, float z)const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.1f * cosf(0.1f * z),
		1.0f,
		-0.1f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

XMFLOAT3 ShapesApp::GetTreePosition(float minX, float maxX, float minZ, float maxZ, float treeHeightOffset)const
{
	XMFLOAT3 pos(0.0f, -1.0f, 0.0f);


		pos.x = MathHelper::RandF(minX, maxX);
		pos.z = MathHelper::RandF(minZ, maxZ);
		pos.y = 8.0f;

	return pos;
}