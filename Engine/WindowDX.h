#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <cstdint>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <string>
#include <wrl.h>
#include <dcomp.h>
#pragma comment(lib, "dcomp.lib")

// ★追加: 時間計測用
#include <chrono>
#include <thread>
#include <functional>

extern std::string g_CharInputBuffer;
extern std::string g_ImeCompositionString; // ★追加: IME変換中の文字列

namespace Engine {

class WindowDX final {
public:
	static constexpr uint32_t kW = 1920;
	static constexpr uint32_t kH = 1080;
	static constexpr uint32_t kBackBufferCount = 2;

	// 現在のドロップ先ディレクトリを保持する静的変数
	static std::string s_DropDirectory;

public:
	bool Initialize(HINSTANCE hInst, int cmdShow, HWND& outHwnd);
	void Shutdown();

	void BeginFrame();
	void EndFrame();

	void WaitGPU(); // nullptr安全
	void WaitIdle() { WaitGPU(); }

	// ---- getters ----
	ID3D12Device* Dev() const { return dev_.Get(); }
	ID3D12GraphicsCommandList* List() const { return list_.Get(); }
	ID3D12CommandQueue* Queue() const { return que_.Get(); }

	ID3D12DescriptorHeap* SRV() const { return srvH_.Get(); }
	ID3D12DescriptorHeap* SRV_CPU_Heap() const { return srvH_CPU_.Get(); } // ★追加
	UINT SrvInc() const { return srvInc_; }
	UINT FrameIndex() const { return fi_; }

	ID3D12Resource* GetCurrentBackBufferResource() const;
	D3D12_CPU_DESCRIPTOR_HANDLE GetCurrentRTV() const;

	D3D12_CPU_DESCRIPTOR_HANDLE RTV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE DSV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE SRV_CPU(int offset) const;
	D3D12_CPU_DESCRIPTOR_HANDLE SRV_CPU_Master(int offset) const; // ★追加
	D3D12_GPU_DESCRIPTOR_HANDLE SRV_GPU(int offset) const;
	HWND GetHwnd() const { return hwnd_; }
	void SetSourceSize(uint32_t w, uint32_t h) { if (swap_) swap_->SetSourceSize(w, h); }

	static HWND GetHWND() { return s_hwnd; }
	static void SetHWND(HWND h) { s_hwnd = h; }

	// アクティブウィンドウかどうかの判定用
	static bool IsActiveWindow() { return s_isActiveWindow; }
    
	// 現在のOSワークエリア取得（タスクバーを除いた有効領域）
	static RECT GetWorkArea();

	// AppBarとして確保した矩形領域を正確に取得する
	static RECT GetAppBarRect();

	// ★追加: AppBarとして領域を確保する（0: なし, 1: 下, 2: 右）
	static void SetAppBarMode(int mode);

private:
	bool InitWindow_(HINSTANCE hInst, int cmdShow, HWND& outHwnd);
	bool InitDX_();
	bool CreateSwapchain_();
	bool CreateRTVDSV_();
	bool CreateCommand_();
	bool CreateFence_();
	static void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd, LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

private:
	static bool s_isActiveWindow;
	static HWND s_hwnd;

	HWND hwnd_ = nullptr;
	HWINEVENTHOOK winEventHook_ = nullptr; // ★追加: ウィンドウ切り替えイベント監視用
	HINSTANCE hInst_ = nullptr;
	WNDCLASSEX wc_{};

	Microsoft::WRL::ComPtr<IDXGIFactory7> factory_;
	Microsoft::WRL::ComPtr<ID3D12Device> dev_;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> que_;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> alloc_;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list_;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> swap_;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvH_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvH_CPU_; // ★追加: 非ShaderVisible

	Microsoft::WRL::ComPtr<IDCompositionDevice> dcompDevice_;
	Microsoft::WRL::ComPtr<IDCompositionTarget> dcompTarget_;
	Microsoft::WRL::ComPtr<IDCompositionVisual> dcompVisual_;

	Microsoft::WRL::ComPtr<ID3D12Resource> back_[kBackBufferCount];
	Microsoft::WRL::ComPtr<ID3D12Resource> depth_;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;
	UINT64 fenceVal_ = 0;
	HANDLE fev_ = nullptr;

	UINT rtvInc_ = 0;
	UINT dsvInc_ = 0;
	UINT srvInc_ = 0;

	UINT fi_ = 0;
	D3D12_VIEWPORT vp_{};
	D3D12_RECT sc_{};

	// ★追加: FPS制御用の変数
	std::chrono::steady_clock::time_point lastFrameTime_;
};

} // namespace Engine