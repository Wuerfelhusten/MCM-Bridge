#include "MCMBridge/Framework/FlickApi.h"
#include "FUCK_API.h"
#include "MCMBridge/Core/FrameworkMenuPath.h"
#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Framework/FrontendWindow.h"
#include "MCMBridge/Framework/RenderContext.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/BrowserWindow.h"
#include "MCMBridge/UI/FlickRichTextRenderer.h"
#include "MCMBridge/UI/FlickSettingsWindow.h"
#include "MCMBridge/UI/KeybindSelector.h"
#include "MCMBridge/UI/MCMWindow.h"
#include "MCMBridge/UI/SettingsWindow.h"

#include <limits>

namespace
{
	using namespace MCMBridge;
	class Tool final : public FUCK::ITool
	{
	public:
		enum class Role
		{
			kMCM,
			kSettings,
			kBrowser,
			kFlickSettings
		};
		struct Presentation
		{
			std::string name;
			std::string group;
		};
		explicit Tool(std::string a_id, Role a_role = Role::kMCM) : id(std::move(a_id)), role(a_role) {}
		const char* PluginName() const override { return "MCMBridge"; }
		const char* Name() const override
		{
			const auto data = presentation.load();
			return data ? data->name.c_str() : "Unavailable MCM";
		}
		const char* Group() const override
		{
			const auto data = presentation.load();
			return data && !data->group.empty() ? data->group.c_str() : nullptr;
		}
		bool ShowInSidebar() const override
		{
			const auto& frontend = FrameworkApi::GetSingleton();
			return presentation.load() && frontend.CanRender(Frontend::kFlick) &&
			       (role != Role::kFlickSettings || FlickSettingsVisible(frontend.Active(), BridgeSettingsService::GetSingleton().Get().preferFlick));
		}
		void Update(std::string a_name, std::string a_group)
		{
			const auto current = presentation.load();
			if (current && current->name == a_name && current->group == a_group)
				return;
			// The host retains raw tool/string pointers. Retired labels remain alive
			// until process shutdown; registration never owns a temporary snapshot.
			history.push_back(std::make_unique<Presentation>(Presentation{ std::move(a_name), std::move(a_group) }));
			presentation.store(history.back().get());
		}
		void Hide() { presentation.store(nullptr); }
		void OnOpen() override
		{
			const std::scoped_lock lock(FrameworkApi::RenderMutex());
			if (ShowInSidebar())
				BridgeController::GetSingleton().OpenFrameworkView();
		}
		void OnClose() override
		{
			const std::scoped_lock lock(FrameworkApi::RenderMutex());
			if (FrameworkApi::GetSingleton().CanRender(Frontend::kFlick))
				BridgeController::GetSingleton().CloseFrameworkView();
			KeybindSelector::CancelAll();
		}
		bool OnAsyncInput(const void* a_event) override { return ShowInSidebar() && FUCK::GetInterface()->IsMenuOpen() && KeybindSelector::HandleFlickInput(a_event); }
		void Draw() override
		{
			const std::scoped_lock lock(FrameworkApi::RenderMutex());
			if (!ShowInSidebar() || FrontendWindow::PrimaryOpen())
				return;
			RenderContext context(Frontend::kFlick, FlickRichTextRenderer::Draw);
			auto&         controller = BridgeController::GetSingleton();
			controller.BeginFrameworkFrame();
			KeybindSelector::BeginFrame();
			switch (role) {
			case Role::kSettings:
				SettingsWindow::Render();
				break;
			case Role::kBrowser:
				BrowserWindow::Render();
				break;
			case Role::kFlickSettings:
				FlickSettingsWindow::Render();
				break;
			case Role::kMCM:
				{
					const auto snapshot = controller.Snapshot();
					const auto mod = std::ranges::find(snapshot->mods, id, &MCMMod::stableID);
					if (mod != snapshot->mods.end()) {
						FlickApi::RenderPages(*mod, page);
					}
					break;
				}
			}
			KeybindSelector::EndFrame();
			controller.EndFrameworkFrame();
		}
		std::string id;

	private:
		std::atomic<const Presentation*>           presentation{};
		std::vector<std::unique_ptr<Presentation>> history;
		std::string                                page;
		Role                                       role;
	};

	class Window final : public FUCK::IWindow
	{
	public:
		explicit Window(FrontendWindow& a_window) : window(a_window) {}
		const char* Id() const override { return window.id.c_str(); }
		const char* PluginName() const override { return "MCMBridge"; }
		const char* Title() const override { return window.title.c_str(); }
		bool        IsOpen() const override
		{
			const auto& framework = FrameworkApi::GetSingleton();
			return window.IsOpen() && (window.primary ? framework.CanRender(Frontend::kFlick) : framework.CanRenderAuxiliary(Frontend::kFlick));
		}
		void SetOpen(bool a_open) override
		{
			if (a_open)
				window.SetOpen(true);
			else
				window.UserClose();
		}
		FUCK::WindowFlags GetFlags() const override
		{
			return FUCK::WindowFlags::kCloseOnEsc | (window.blocking ? FUCK::WindowFlags::kBlurBackground : FUCK::WindowFlags::kPassInputToGame) | (window.height == 0 ? FUCK::WindowFlags::kAutoResize : FUCK::WindowFlags::kNone);
		}
		ImVec2 GetDefaultSize() const override { return { window.width, window.height }; }
		ImVec2 GetDefaultPos() const override
		{
			float width, height;
			FUCK::GetInterface()->GetDisplaySize(&width, &height);
			return { (std::max)(0.0F, (width - window.width) * 0.5F), (std::max)(0.0F, (height - (window.height ? window.height : 240)) * 0.5F) };
		}
		bool OnAsyncInput(const void* a_event) override { return IsOpen() && KeybindSelector::HandleFlickInput(a_event); }
		void Draw() override
		{
			const std::scoped_lock lock(FrameworkApi::RenderMutex());
			if (!IsOpen())
				return;
			RenderContext context(Frontend::kFlick, FlickRichTextRenderer::Draw, true);
			window.Render();
		}

	private:
		FrontendWindow& window;
	};
	std::unordered_map<std::string, std::unique_ptr<Tool>> tools;
	std::vector<std::unique_ptr<Window>>                   windows;
}

namespace MCMBridge::FlickApi
{
	void Install()
	{
		for (const auto& [id, name, role] : {
				 std::tuple{ "@bridge:settings", "Settings", Tool::Role::kSettings },
				 std::tuple{ "@bridge:browser", "Browser", Tool::Role::kBrowser },
				 std::tuple{ "@bridge:flick", "FLICK settings", Tool::Role::kFlickSettings } }) {
			auto tool = std::make_unique<Tool>(id, role);
			tool->Update(name, "MCM Bridge");
			FUCK::GetInterface()->RegisterTool(tool.get());
			tools.emplace(id, std::move(tool));
		}
	}
	void RegisterWindow(FrontendWindow& a_window)
	{
		auto window = std::make_unique<Window>(a_window);
		FUCK::GetInterface()->RegisterWindow(window.get());
		windows.push_back(std::move(window));
	}
	void Synchronize(std::span<const MCMMod> a_mods)
	{
		const std::scoped_lock          lock(FrameworkApi::RenderMutex());
		const auto                      settings = BridgeSettingsService::GetSingleton().Get();
		std::unordered_set<std::string> retained{ "@bridge:settings", "@bridge:browser", "@bridge:flick" };
		std::unordered_set<std::string> names{ "Settings", "Browser", "FLICK settings" };
		for (const auto& mod : a_mods) {
			if (mod.pages.empty())
				continue;
			retained.insert(mod.stableID);
			auto name = ResolveMCMAlias(settings, mod.stableID, mod.displayName);
			if (!settings.groupMCMs)
				name = UppercaseMCMRootInitial(name);
			auto&      tool = tools[mod.stableID];
			const bool created = !tool;
			if (created)
				tool = std::make_unique<Tool>(mod.stableID);
			tool->Update(UniqueFrameworkMenuLabel(name, names), FlickMCMGroup(settings, name));
			if (created)
				FUCK::GetInterface()->RegisterTool(tool.get());
		}
		for (const auto& [id, tool] : tools)
			if (!retained.contains(id))
				tool->Hide();
	}
	void SetOpen(bool a_open) { FUCK::GetInterface()->SetMenuOpen(a_open); }
	bool IsOpen() { return FUCK::GetInterface()->IsMenuOpen(); }
}
