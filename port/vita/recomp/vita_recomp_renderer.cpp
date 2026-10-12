#include <cstdint>
#include <memory>
#include <string>

#include <psp2/kernel/processmgr.h>
#include <vitaGL.h>

#include "ultramodern/renderer_context.hpp"
#include "vita_fast3d_abi.hpp"
#include "vita_fast3d_bridge.hpp"
#include "vita_recomp_diagnostics.hpp"
#include "vita_recomp_memory.hpp"

namespace {
class VitaWindow final : public Fast::GfxWindowBackend {
public:
    void Init(const char*, const char*, bool, std::uint32_t width,
              std::uint32_t height, std::int32_t x, std::int32_t y) override {
        width_ = width; height_ = height; x_ = x; y_ = y; running_ = true;
    }
    void Close() override {}
    void SetKeyboardCallbacks(bool (*)(int), bool (*)(int), void (*)()) override {}
    void SetMouseCallbacks(bool (*)(int), bool (*)(int)) override {}
    void SetFullscreenChangedCallback(void (*)(bool)) override {}
    void SetFullscreen(bool) override {}
    void GetActiveWindowRefreshRate(std::uint32_t* rate) override { if (rate) *rate = 60; }
    void SetCursorVisibility(bool) override {}
    void SetMousePos(std::int32_t, std::int32_t) override {}
    void GetMousePos(std::int32_t* x, std::int32_t* y) override { if (x) *x = 0; if (y) *y = 0; }
    void GetMouseDelta(std::int32_t* x, std::int32_t* y) override { if (x) *x = 0; if (y) *y = 0; }
    void GetMouseWheel(float* x, float* y) override { if (x) *x = 0; if (y) *y = 0; }
    bool GetMouseState(std::uint32_t) override { return false; }
    void SetMouseCapture(bool) override {}
    bool IsMouseCaptured() override { return false; }
    void GetDimensions(std::uint32_t* width, std::uint32_t* height,
                       std::int32_t* x, std::int32_t* y) override {
        if (width) *width = width_; if (height) *height = height_;
        if (x) *x = x_; if (y) *y = y_;
    }
    void SetDimensions(std::uint32_t width, std::uint32_t height,
                       std::int32_t x, std::int32_t y) override {
        width_ = width; height_ = height; x_ = x; y_ = y;
    }
    Ship::WindowRect GetPrimaryMonitorRect() override { return {0, 0, 960, 544}; }
    void HandleEvents() override {}
    bool IsFrameReady() override { return true; }
    void SwapBuffersBegin() override {}
    void SwapBuffersEnd() override { vglSwapBuffers(GL_FALSE); }
    double GetTime() override {
        return static_cast<double>(sceKernelGetProcessTimeWide()) / 1000000.0;
    }
    int GetTargetFps() override { return 60; }
    void SetTargetFps(int) override {}
    void SetMaxFrameLatency(int) override {}
    const char* GetKeyName(int) override { return ""; }
    bool CanDisableVsync() override { return false; }
    bool IsRunning() override { return running_; }
    void Destroy() override { running_ = false; }
    bool IsFullscreen() override { return true; }
private:
    std::uint32_t width_ = 960, height_ = 544;
    std::int32_t x_ = 0, y_ = 0;
    bool running_ = true;
};

class VitaRenderer final : public ultramodern::renderer::RendererContext {
public:
    explicit VitaRenderer(std::uint8_t* rdram)
        : rdram_(rdram), bridge_(rdram, btga::vita::kRdramSize32),
          interpreter_(std::make_shared<Fast::Interpreter>()) {
        btga_vita_log("renderer object allocated");
        Fast::GfxSetInstance(interpreter_);
        btga_vita_log("Fast3D instance installed");
        interpreter_->Init(&window_, &api_, "VitaTanx GA", true,
                           960, 544, 0, 0);
        btga_vita_log("Fast3D backend initialized");
        interpreter_->SetRdramMemory(rdram_, btga::vita::kRdramSize32);
        btga_vita_log("Fast3D RDRAM bound");
        interpreter_->SetNativeDimensions(320.0F, 240.0F);
        interpreter_->SetDisplayConfiguration(960, 544, 0, 0,
                                              (4.0F / 3.0F) / (960.0F / 544.0F));
        btga_vita_log("Fast3D display configured");
        bridge_.SetInterpreter(interpreter_.get());
        Fast::gfx_set_target_ucode(ucode_f3dex2);
        btga_vita_log("renderer ready");
    }
    ~VitaRenderer() override = default;
    bool valid() override { return interpreter_ != nullptr; }
    bool update_config(const ultramodern::renderer::GraphicsConfig&,
                       const ultramodern::renderer::GraphicsConfig&) override { return true; }
    void enable_instant_present() override {}
    void send_dl(const OSTask* task) override {
        if (!frame_started_) {
            bridge_.BeginFrame();
            interpreter_->StartFrame();
            frame_started_ = true;
        }
        const std::uint32_t address = static_cast<std::uint32_t>(task->t.data_ptr);
        void* list = bridge_.BuildDisplayList(address, GBI_2);
        btga_vita_note_display_list(list != nullptr);
        if (list != nullptr) interpreter_->Run(static_cast<Gfx*>(list), matrices_);
    }
    void send_dummy_workload(std::uint32_t) override {}
    void update_screen() override {
        if (frame_started_) {
            interpreter_->EndFrame();
            frame_started_ = false;
            btga_vita_note_present();
        }
    }
    void shutdown() override {
        if (frame_started_) update_screen();
        interpreter_->Destroy();
        Fast::GfxSetInstance({});
    }
    std::uint32_t get_display_framerate() const override { return 60; }
    float get_resolution_scale() const override { return 1.0F; }
private:
    std::uint8_t* rdram_;
    VitaWindow window_;
    Fast::GfxRenderingAPIOGL api_;
    Fast3DMemoryBridge bridge_;
    std::shared_ptr<Fast::Interpreter> interpreter_;
    robin_hood::unordered_map<Mtx*, MtxF> matrices_;
    bool frame_started_ = false;
};
} // namespace

std::unique_ptr<ultramodern::renderer::RendererContext>
btga_vita_create_renderer(std::uint8_t* rdram,
                          ultramodern::renderer::WindowHandle, bool) {
    return std::make_unique<VitaRenderer>(rdram);
}

extern "C" bool DaedalusFast3D_ShouldSuppressFramebufferClear(bool, bool) { return false; }
extern "C" std::uint32_t DaedalusFast3D_GetFramebufferOverride(int) { return 0; }
void RDRAM_MarkDirtyRange(std::uint32_t, std::uint32_t) {}

namespace Ship {
const std::string IResource::gAltAssetPrefix{};
static std::shared_ptr<Context> context = std::make_shared<Context>();
static std::shared_ptr<ConsoleVariable> variables = std::make_shared<ConsoleVariable>();
static std::shared_ptr<ResourceManager> resources = std::make_shared<ResourceManager>();
static std::shared_ptr<ArchiveManager> archives = std::make_shared<ArchiveManager>();
std::shared_ptr<ResourceInitData> IResource::GetInitData() { return mInitData; }
const std::string* ArchiveManager::HashToString(std::uint64_t) const { return nullptr; }
std::shared_ptr<ArchiveManager> ResourceManager::GetArchiveManager() { return archives; }
void* ResourceManager::GetResourceRawPointer(const char*) { return nullptr; }
void* ResourceManager::GetResourceRawPointer(std::uint64_t) { return nullptr; }
std::shared_ptr<IResource> ResourceManager::LoadResourceProcess(const std::string&, bool,
    std::shared_ptr<ResourceInitData>, std::uint64_t) { return {}; }
std::shared_ptr<IResource> ResourceManager::LoadResourceProcessFast(const char*) { return {}; }
bool ResourceManager::OtrSignatureCheck(const char*) { return false; }
float ConsoleVariable::GetFloat(const char*, float fallback) { return fallback; }
int ConsoleVariable::GetInteger(const char*, int fallback) { return fallback; }
std::shared_ptr<Context> Context::GetInstance() { return context; }
std::shared_ptr<ConsoleVariable> Context::GetConsoleVariables() const { return variables; }
std::shared_ptr<ResourceManager> Context::GetResourceManager() const { return resources; }
} // namespace Ship
