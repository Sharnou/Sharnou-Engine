#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <avif/avif.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cwctype>
#include <filesystem>
#include <functional>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "user32.lib")

namespace shn {
using Entity = std::uint32_t;
constexpr Entity NullEntity = 0;
constexpr std::uint32_t MaxEntities = 262144;
using Microsoft::WRL::ComPtr;

static_assert(sizeof(void*) == 8, "Sharnou Engine is x64 Windows-only.");

struct Vec3 { float x{}, y{}, z{}; };
struct Transform { Vec3 position{}, rotation{}, scale{1.f,1.f,1.f}; };

template<class T>
class ComponentPool {
    static constexpr std::uint32_t Empty = std::numeric_limits<std::uint32_t>::max();
    std::vector<Entity> denseEntities_;
    std::vector<T> denseData_;
    std::vector<std::uint32_t> sparse_;
public:
    ComponentPool() : sparse_(MaxEntities, Empty) { denseEntities_.reserve(4096); denseData_.reserve(4096); }
    bool has(Entity e) const noexcept { return e && e < MaxEntities && sparse_[e] != Empty; }
    T* get(Entity e) noexcept { return has(e) ? &denseData_[sparse_[e]] : nullptr; }
    const T* get(Entity e) const noexcept { return has(e) ? &denseData_[sparse_[e]] : nullptr; }
    T& add(Entity e, T value = {}) {
        if (has(e)) return denseData_[sparse_[e]];
        const auto i = static_cast<std::uint32_t>(denseData_.size());
        sparse_[e] = i; denseEntities_.push_back(e); denseData_.push_back(std::move(value)); return denseData_.back();
    }
    void remove(Entity e) noexcept {
        if (!has(e)) return;
        const auto i = sparse_[e], last = static_cast<std::uint32_t>(denseData_.size() - 1);
        if (i != last) { denseData_[i] = std::move(denseData_[last]); denseEntities_[i] = denseEntities_[last]; sparse_[denseEntities_[i]] = i; }
        denseData_.pop_back(); denseEntities_.pop_back(); sparse_[e] = Empty;
    }
    std::size_t size() const noexcept { return denseData_.size(); }
    T* data() noexcept { return denseData_.data(); }
};

class World {
    std::vector<Entity> free_;
    Entity next_{1};
public:
    ComponentPool<Transform> transforms;
    ComponentPool<Vec3> velocities;
    Entity create() {
        if (!free_.empty()) { const Entity e = free_.back(); free_.pop_back(); return e; }
        return next_ <= MaxEntities - 1 ? next_++ : NullEntity;
    }
    void destroy(Entity e) { transforms.remove(e); velocities.remove(e); if (e) free_.push_back(e); }
};

template<class T, std::size_t Capacity>
class MpmcQueue {
    struct Cell { std::atomic<std::size_t> sequence{0}; T data{}; };
    std::array<Cell, Capacity> cells_{};
    std::atomic<std::size_t> enqueue_{0}, dequeue_{0};
public:
    MpmcQueue() { for (std::size_t i=0;i<Capacity;++i) cells_[i].sequence.store(i,std::memory_order_relaxed); }
    bool try_push(T value) noexcept {
        Cell* c; auto pos = enqueue_.load(std::memory_order_relaxed);
        for (;;) {
            c = &cells_[pos % Capacity]; const auto seq = c->sequence.load(std::memory_order_acquire);
            const auto dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos);
            if (dif == 0) { if (enqueue_.compare_exchange_weak(pos,pos+1,std::memory_order_relaxed)) break; }
            else if (dif < 0) return false; else pos = enqueue_.load(std::memory_order_relaxed);
        }
        c->data = std::move(value); c->sequence.store(pos+1,std::memory_order_release); return true;
    }
    bool try_pop(T& value) noexcept {
        Cell* c; auto pos = dequeue_.load(std::memory_order_relaxed);
        for (;;) {
            c = &cells_[pos % Capacity]; const auto seq = c->sequence.load(std::memory_order_acquire);
            const auto dif = static_cast<std::intptr_t>(seq) - static_cast<std::intptr_t>(pos+1);
            if (dif == 0) { if (dequeue_.compare_exchange_weak(pos,pos+1,std::memory_order_relaxed)) break; }
            else if (dif < 0) return false; else pos = dequeue_.load(std::memory_order_relaxed);
        }
        value = std::move(c->data); c->sequence.store(pos+Capacity,std::memory_order_release); return true;
    }
};

class JobSystem {
    using Task = std::function<void()>;
    MpmcQueue<Task, 2048> queue_;
    std::vector<std::jthread> workers_;
    std::mutex wakeMutex_;
    std::condition_variable_any wake_;
    std::atomic<bool> stopping_{false};
public:
    JobSystem() {
        const auto count = std::max(1u, std::thread::hardware_concurrency() > 1 ? std::thread::hardware_concurrency()-1 : 1u);
        workers_.reserve(count);
        for (unsigned i=0;i<count;++i) workers_.emplace_back([this](std::stop_token st) {
            Task task;
            while (!st.stop_requested() && !stopping_.load(std::memory_order_acquire)) {
                if (queue_.try_pop(task)) { task(); task = {}; continue; }
                std::unique_lock lock(wakeMutex_);
                wake_.wait_for(lock, std::chrono::milliseconds(2));
            }
        });
    }
    ~JobSystem() {
        stopping_.store(true, std::memory_order_release); wake_.notify_all();
        for (auto& w: workers_) w.request_stop();
    }
    bool submit(Task task) { const bool ok = queue_.try_push(std::move(task)); if (ok) wake_.notify_one(); return ok; }
};

struct AvifImage {
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> rgba;
};

class AvifCodec {
    static std::string utf8(std::filesystem::path p) {
        auto u = p.u8string(); return std::string(reinterpret_cast<const char*>(u.data()), u.size());
    }
public:
    static bool decode(const std::filesystem::path& path, AvifImage& out, std::string& error) {
        auto ext = path.extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c){ return static_cast<wchar_t>(std::towlower(c)); });
        if (ext != L".avif") { error = "Texture rejected: only .avif is allowed."; return false; }
        std::error_code ec;
        const auto fileBytes = std::filesystem::file_size(path, ec);
        if (ec || fileBytes > 256ull * 1024ull * 1024ull) { error = "Texture rejected: missing or larger than 256 MiB."; return false; }
        avifDecoder* decoder = avifDecoderCreate();
        if (!decoder) { error = "libavif allocation failed."; return false; }
        avifRGBImage rgb{};
        const auto input = utf8(path);
        auto result = avifDecoderSetIOFile(decoder, input.c_str());
        if (result == AVIF_RESULT_OK) result = avifDecoderParse(decoder);
        if (result == AVIF_RESULT_OK) result = avifDecoderNextImage(decoder);
        if (result != AVIF_RESULT_OK) { error = avifResultToString(result); avifDecoderDestroy(decoder); return false; }
        out.width = decoder->image->width; out.height = decoder->image->height;
        if (static_cast<std::uint64_t>(out.width) * out.height > 8192ull * 8192ull) {
            error = "Texture rejected: decoded surface exceeds the 8192x8192 memory guard.";
            avifDecoderDestroy(decoder);
            return false;
        }
        avifRGBImageSetDefaults(&rgb, decoder->image); rgb.format = AVIF_RGB_FORMAT_RGBA; rgb.depth = 8;
        result = avifRGBImageAllocatePixels(&rgb);
        if (result == AVIF_RESULT_OK) result = avifImageYUVToRGB(decoder->image, &rgb);
        if (result != AVIF_RESULT_OK) { error = avifResultToString(result); avifRGBImageFreePixels(&rgb); avifDecoderDestroy(decoder); return false; }
        out.rgba.resize(static_cast<std::size_t>(out.width)*out.height*4);
        for (std::uint32_t y=0;y<out.height;++y) std::copy_n(rgb.pixels + static_cast<std::size_t>(y)*rgb.rowBytes, static_cast<std::size_t>(out.width)*4, out.rgba.data()+static_cast<std::size_t>(y)*out.width*4);
        avifRGBImageFreePixels(&rgb); avifDecoderDestroy(decoder); return true;
    }
};

class Renderer {
    struct Vertex { DirectX::XMFLOAT3 pos, normal; DirectX::XMFLOAT2 uv; };
    struct Constants { DirectX::XMFLOAT4X4 wvp; float tint[4]{1.f,1.f,1.f,1.f}; float useTexture[4]{}; };
    ComPtr<ID3D11Device> device_; ComPtr<ID3D11DeviceContext> context_; ComPtr<IDXGISwapChain> swap_;
    ComPtr<ID3D11RenderTargetView> target_; ComPtr<ID3D11DepthStencilView> depth_;
    ComPtr<ID3D11VertexShader> vs_; ComPtr<ID3D11PixelShader> ps_; ComPtr<ID3D11InputLayout> layout_; ComPtr<ID3D11Buffer> cb_;
    ComPtr<ID3D11Buffer> vb_, ib_; ComPtr<ID3D11SamplerState> sampler_; ComPtr<ID3D11ShaderResourceView> texture_;
    std::uint32_t indexCount_{};
    float useTexture_{};
    float angle_{};
    static constexpr char Shader[] = R"(
cbuffer Frame : register(b0) { row_major float4x4 WVP; float4 Tint; float4 UseTexture; };
Texture2D Albedo : register(t0); SamplerState Samp : register(s0);
struct VSIn { float3 p:POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; };
struct PSIn { float4 p:SV_POSITION; float3 n:NORMAL; float2 uv:TEXCOORD0; };
PSIn VS(VSIn i) { PSIn o; o.p=mul(float4(i.p,1),WVP); o.n=i.n; o.uv=i.uv; return o; }
float4 PS(PSIn i):SV_TARGET { float3 L=normalize(float3(.35,.8,-.45)); float d=max(dot(normalize(i.n),L),.18); float4 a=Tint; if (UseTexture.x > .5) a*=Albedo.Sample(Samp,i.uv); return float4(a.rgb*d,a.a); }
)";
    bool createTargets(std::uint32_t w, std::uint32_t h) {
        ComPtr<ID3D11Texture2D> back; if (FAILED(swap_->GetBuffer(0, IID_PPV_ARGS(&back)))) return false;
        if (FAILED(device_->CreateRenderTargetView(back.Get(),nullptr,&target_))) return false;
        D3D11_TEXTURE2D_DESC d{}; d.Width=w; d.Height=h; d.MipLevels=1; d.ArraySize=1; d.Format=DXGI_FORMAT_D24_UNORM_S8_UINT; d.SampleDesc.Count=1; d.BindFlags=D3D11_BIND_DEPTH_STENCIL;
        ComPtr<ID3D11Texture2D> z; return SUCCEEDED(device_->CreateTexture2D(&d,nullptr,&z)) && SUCCEEDED(device_->CreateDepthStencilView(z.Get(),nullptr,&depth_));
    }
    bool createPipeline() {
        ComPtr<ID3DBlob> v,p,e; if (FAILED(D3DCompile(Shader,sizeof(Shader)-1,nullptr,nullptr,nullptr,"VS","vs_5_0",0,0,&v,&e))) return false;
        if (FAILED(D3DCompile(Shader,sizeof(Shader)-1,nullptr,nullptr,nullptr,"PS","ps_5_0",0,0,&p,&e))) return false;
        if (FAILED(device_->CreateVertexShader(v->GetBufferPointer(),v->GetBufferSize(),nullptr,&vs_))) return false;
        if (FAILED(device_->CreatePixelShader(p->GetBufferPointer(),p->GetBufferSize(),nullptr,&ps_))) return false;
        D3D11_INPUT_ELEMENT_DESC elems[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D11_INPUT_PER_VERTEX_DATA,0},{"NORMAL",0,DXGI_FORMAT_R32G32B32_FLOAT,0,12,D3D11_INPUT_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,24,D3D11_INPUT_PER_VERTEX_DATA,0}};
        if (FAILED(device_->CreateInputLayout(elems,3,v->GetBufferPointer(),v->GetBufferSize(),&layout_))) return false;
        D3D11_BUFFER_DESC cb{}; cb.ByteWidth=sizeof(Constants); cb.Usage=D3D11_USAGE_DYNAMIC; cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER; cb.CPUAccessFlags=D3D11_CPU_ACCESS_WRITE;
        if (FAILED(device_->CreateBuffer(&cb,nullptr,&cb_))) return false;
        D3D11_SAMPLER_DESC s{}; s.Filter=D3D11_FILTER_ANISOTROPIC; s.MaxAnisotropy=8; s.AddressU=s.AddressV=s.AddressW=D3D11_TEXTURE_ADDRESS_WRAP;
        return SUCCEEDED(device_->CreateSamplerState(&s,&sampler_));
    }
    bool createCube() {
        const Vertex v[]={
            {{-1,-1,-1},{0,0,-1},{0,1}},{{1,-1,-1},{0,0,-1},{1,1}},{{1,1,-1},{0,0,-1},{1,0}},{{-1,1,-1},{0,0,-1},{0,0}},
            {{-1,-1,1},{0,0,1},{1,1}},{{-1,1,1},{0,0,1},{1,0}},{{1,1,1},{0,0,1},{0,0}},{{1,-1,1},{0,0,1},{0,1}},
            {{-1,1,-1},{0,1,0},{0,1}},{{1,1,-1},{0,1,0},{1,1}},{{1,1,1},{0,1,0},{1,0}},{{-1,1,1},{0,1,0},{0,0}},
            {{-1,-1,-1},{0,-1,0},{0,0}},{{-1,-1,1},{0,-1,0},{0,1}},{{1,-1,1},{0,-1,0},{1,1}},{{1,-1,-1},{0,-1,0},{1,0}},
            {{-1,-1,-1},{-1,0,0},{0,1}},{{-1,1,-1},{-1,0,0},{0,0}},{{-1,1,1},{-1,0,0},{1,0}},{{-1,-1,1},{-1,0,0},{1,1}},
            {{1,-1,1},{1,0,0},{0,1}},{{1,1,1},{1,0,0},{0,0}},{{1,1,-1},{1,0,0},{1,0}},{{1,-1,-1},{1,0,0},{1,1}}
        };
        const std::uint32_t idx[]={0,1,2,0,2,3,4,5,6,4,6,7,8,9,10,8,10,11,12,13,14,12,14,15,16,17,18,16,18,19,20,21,22,20,22,23};
        D3D11_BUFFER_DESC b{}; b.Usage=D3D11_USAGE_DEFAULT; b.ByteWidth=sizeof(v); b.BindFlags=D3D11_BIND_VERTEX_BUFFER; D3D11_SUBRESOURCE_DATA sd{v};
        if (FAILED(device_->CreateBuffer(&b,&sd,&vb_))) return false;
        b.ByteWidth=sizeof(idx); b.BindFlags=D3D11_BIND_INDEX_BUFFER; sd.pSysMem=idx; if (FAILED(device_->CreateBuffer(&b,&sd,&ib_))) return false; indexCount_=std::size(idx); return true;
    }
public:
    bool init(HWND hwnd, std::uint32_t w, std::uint32_t h) {
        DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount=2; sd.BufferDesc.Width=w; sd.BufferDesc.Height=h; sd.BufferDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow=hwnd; sd.SampleDesc.Count=1; sd.Windowed=TRUE; sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        const D3D_FEATURE_LEVEL levels[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0,D3D_FEATURE_LEVEL_10_1}; D3D_FEATURE_LEVEL got{};
        UINT flags=0;
#ifdef _DEBUG
        flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,flags,levels,3,D3D11_SDK_VERSION,&sd,&swap_,&device_,&got,&context_))) return false;
        D3D11_VIEWPORT vp{}; vp.Width=static_cast<float>(w); vp.Height=static_cast<float>(h); vp.MinDepth=0; vp.MaxDepth=1; context_->RSSetViewports(1,&vp);
        return createTargets(w,h)&&createPipeline()&&createCube();
    }
    bool loadTexture(const std::filesystem::path& file, std::string& error) {
        AvifImage image; if (!AvifCodec::decode(file,image,error)) return false;
        D3D11_TEXTURE2D_DESC d{}; d.Width=image.width; d.Height=image.height; d.MipLevels=1; d.ArraySize=1; d.Format=DXGI_FORMAT_R8G8B8A8_UNORM; d.SampleDesc.Count=1; d.Usage=D3D11_USAGE_IMMUTABLE; d.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA init{image.rgba.data(), image.width*4,0}; ComPtr<ID3D11Texture2D> tex;
        if (FAILED(device_->CreateTexture2D(&d,&init,&tex))) { error="D3D11 texture upload failed."; return false; }
        if (FAILED(device_->CreateShaderResourceView(tex.Get(),nullptr,&texture_))) { error="D3D11 shader-resource view creation failed."; return false; }
        useTexture_=1.f;
        return true;
    }
    void frame(float dt, std::uint32_t w, std::uint32_t h) {
        angle_ += dt*.8f; const float clear[]={.015f,.02f,.03f,1}; context_->ClearRenderTargetView(target_.Get(),clear); context_->ClearDepthStencilView(depth_.Get(),D3D11_CLEAR_DEPTH|D3D11_CLEAR_STENCIL,1,0);
        context_->OMSetRenderTargets(1,target_.GetAddressOf(),depth_.Get()); UINT stride=sizeof(Vertex), off=0; ID3D11Buffer* vb=vb_.Get(); context_->IASetInputLayout(layout_.Get()); context_->IASetVertexBuffers(0,1,&vb,&stride,&off); context_->IASetIndexBuffer(ib_.Get(),DXGI_FORMAT_R32_UINT,0); context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST); context_->VSSetShader(vs_.Get(),nullptr,0); context_->PSSetShader(ps_.Get(),nullptr,0); context_->PSSetSamplers(0,1,sampler_.GetAddressOf());
        auto world=DirectX::XMMatrixRotationRollPitchYaw(.15f,angle_,0);
        auto view=DirectX::XMMatrixLookAtLH(DirectX::XMVectorSet(0,1.4f,-4.5f,1),DirectX::XMVectorZero(),DirectX::XMVectorSet(0,1,0,0));
        auto proj=DirectX::XMMatrixPerspectiveFovLH(DirectX::XM_PIDIV4,float(w)/float(std::max(1u,h)),.1f,100.f);
        Constants c{}; DirectX::XMStoreFloat4x4(&c.wvp,DirectX::XMMatrixTranspose(world*view*proj)); c.tint[0]=.65f; c.tint[1]=.8f; c.tint[2]=1.f; c.useTexture[0]=useTexture_;
        D3D11_MAPPED_SUBRESOURCE m{}; if (SUCCEEDED(context_->Map(cb_.Get(),0,D3D11_MAP_WRITE_DISCARD,0,&m))) { *static_cast<Constants*>(m.pData)=c; context_->Unmap(cb_.Get(),0); }
        context_->VSSetConstantBuffers(0,1,cb_.GetAddressOf()); if (texture_) context_->PSSetShaderResources(0,1,texture_.GetAddressOf()); context_->DrawIndexed(indexCount_,0,0); swap_->Present(1,0);
    }
};

class Engine {
    HWND window_{}; HINSTANCE instance_{}; World world_; JobSystem jobs_; Renderer renderer_; bool running_{};
    std::atomic<std::uint64_t> backgroundTicks_{};
    std::uint32_t width_{1280}, height_{720}; std::chrono::steady_clock::time_point previous_{};
    static LRESULT CALLBACK wndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        auto* e=reinterpret_cast<Engine*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if (m==WM_NCCREATE) { auto* cs=reinterpret_cast<CREATESTRUCTW*>(l); e=static_cast<Engine*>(cs->lpCreateParams); SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(e)); }
        if (e && m==WM_KEYDOWN && w==VK_ESCAPE) DestroyWindow(h);
        if (m==WM_DESTROY) { PostQuitMessage(0); return 0; } return DefWindowProcW(h,m,w,l);
    }
public:
    explicit Engine(HINSTANCE h):instance_(h) {}
    bool init() {
        WNDCLASSEXW wc{sizeof(wc),CS_OWNDC,wndProc,0,0,instance_,LoadIconW(nullptr,IDI_APPLICATION),LoadCursorW(nullptr,IDC_ARROW),nullptr,L"SHN_ENGINE",nullptr};
        if (!RegisterClassExW(&wc)) return false; RECT r{0,0,static_cast<LONG>(width_),static_cast<LONG>(height_)}; AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
        window_=CreateWindowExW(0,wc.lpszClassName,L"Sharnou Engine | Windows 10 | C++23 | AVIF-only textures",WS_OVERLAPPEDWINDOW|WS_VISIBLE,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance_,this);
        if (!window_ || !renderer_.init(window_,width_,height_)) return false;
        const std::filesystem::path textureRoot = L"assets/textures";
        if (std::filesystem::exists(textureRoot)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(textureRoot)) {
                if (!entry.is_regular_file()) continue;
                std::string error;
                if (renderer_.loadTexture(entry.path(),error)) break;
                OutputDebugStringA(("[SharnouEngine] " + error + "\n").c_str());
            }
        }
        for (int i=0;i<2000;++i) { Entity e=world_.create(); world_.transforms.add(e,Transform{{float(i%50)*2.3f,0,float(i/50)*2.3f}}); world_.velocities.add(e,Vec3{}); }
        previous_=std::chrono::steady_clock::now(); running_=true; return true;
    }
    int run() {
        MSG msg{}; std::uint64_t frames=0; double acc=0;
        while (running_) {
            while (PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)) { if (msg.message==WM_QUIT) running_=false; TranslateMessage(&msg); DispatchMessageW(&msg); }
            const auto now=std::chrono::steady_clock::now(); const float dt=std::min(.1f,std::chrono::duration<float>(now-previous_).count()); previous_=now; acc+=dt;
            if ((frames & 3u) == 0u) jobs_.submit([this]{ backgroundTicks_.fetch_add(1,std::memory_order_relaxed); });
            if (acc>=1.0) { frames=0; acc=0; }
            renderer_.frame(dt,width_,height_); ++frames;
        }
        return 0;
    }
};
}
int WINAPI wWinMain(HINSTANCE h,HINSTANCE, PWSTR,int) { shn::Engine engine(h); return engine.init()?engine.run():-1; }
