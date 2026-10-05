#include "MCMBridge/Plugin/StartupCompatibility.h"

#include <CommCtrl.h>
#include <Windows.h>
#include <shellapi.h>

namespace
{
	constexpr int  exitButton = 100;
	constexpr int  ignoreButton = 101;
	constexpr auto memoryURL = L"https://www.nexusmods.com/skyrimspecialedition/mods/189722";

	struct DialogContent
	{
		std::wstring text;
		bool         recorder{};
	};

	void OpenModPage(const wchar_t* a_url)
	{
		const auto result = reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", a_url, nullptr, nullptr, SW_SHOWNORMAL));
		if (result <= 32) {
			MessageBoxW(nullptr, a_url, L"Could not open browser - MCM Bridge", MB_OK | MB_ICONERROR);
		}
	}

	HRESULT CALLBACK OnTaskDialog(HWND a_window, UINT a_notification, WPARAM, LPARAM a_lParam, LONG_PTR)
	{
		if (a_notification == TDN_HYPERLINK_CLICKED) {
			const auto* url = reinterpret_cast<const wchar_t*>(a_lParam);
			if (url && std::wstring_view(url) == memoryURL)
				OpenModPage(url);
		}
		if (a_notification == TDN_CREATED)
			SetForegroundWindow(a_window);
		return S_OK;
	}

	INT_PTR CALLBACK OnFallbackDialog(HWND a_window, UINT a_message, WPARAM a_wParam, LPARAM a_lParam)
	{
		if (a_message == WM_INITDIALOG) {
			const auto& content = *reinterpret_cast<const DialogContent*>(a_lParam);
			SetWindowLongPtrW(a_window, GWLP_USERDATA, a_lParam);
			SetDlgItemTextW(a_window, 200, content.text.c_str());
			ShowWindow(GetDlgItem(a_window, 202), content.recorder ? SW_SHOW : SW_HIDE);
			SetForegroundWindow(a_window);
			return TRUE;
		}
		if (a_message == WM_COMMAND) {
			const auto button = LOWORD(a_wParam);
			if (button == 202) {
				OpenModPage(memoryURL);
				return TRUE;
			}
			if (button == exitButton || button == ignoreButton) {
				EndDialog(a_window, button);
				return TRUE;
			}
		}
		if (a_message == WM_CLOSE) {
			EndDialog(a_window, exitButton);
			return TRUE;
		}
		return FALSE;
	}
}

namespace MCMBridge
{
	void ShowStartupIncompatibility(bool a_redone, bool a_recorder, bool a_seeded, bool a_unlocked, bool a_menuMaid, std::string_view a_scripts)
	{
		DialogContent content;
		content.recorder = a_recorder;
		content.text = L"MCM Bridge is incompatible with:\n";
		if (!a_scripts.empty()) {
			content.text = L"MCM Bridge's required PEX files are missing, unreadable or have been replaced:\n";
			content.text.append(a_scripts.begin(), a_scripts.end());
			content.text += L"\nIf you previously had MCM Unlocked installed, reinstall MCM Bridge and then make sure that MCM Bridge's PEX files are not overwritten.\nDisable MCM Unlocked completely, let MCM Bridge win script conflicts in your mod manager, and restart Skyrim.\n";
		}
		if (a_redone)
			content.text += L"MCM Menu Redone (MCMMenuRedone.dll)\n";
		if (a_seeded)
			content.text += L"MCM super SEEDED (MCM_Super_SEEDED.dll)\n";
		if (a_recorder)
			content.text += L"MCM Recorder (McmRecorder.esp)\n";
		if (a_unlocked)
			content.text += L"MCM Unlocked (MCM-Unlocked.dll)\nBoth mods replace MCM registration. Disable MCM Unlocked to use the native MCM Bridge host.\n";
		if (a_menuMaid)
			content.text += L"Menu Maid 2 (MenuMaid2.dll)\nBoth mods replace MCM registration. Disable Menu Maid 2 to use the native MCM Bridge host.\n";
		content.text +=
			L"\nMCM Bridge has completely disabled itself for this session.\n\n"
			L"Exit closes Skyrim. Ignore continues with MCM Bridge disabled.";
		auto taskText = content.text + (a_recorder ?
											   L"\n\nYou can use MCMMemory instead. Click here to <a href=\"https://www.nexusmods.com/skyrimspecialedition/mods/189722\">open on Nexus Mods</a>." :
											   L"");
		content.text += L"\nBundled native host scripts do not provide a working MCM fallback while MCM Bridge is disabled.";
		taskText += L"\nBundled native host scripts do not provide a working MCM fallback while MCM Bridge is disabled.";

		HMODULE module{};
		GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&ShowStartupIncompatibility), &module);
		ACTCTXW context{};
		context.cbSize = sizeof(context);
		context.dwFlags = ACTCTX_FLAG_RESOURCE_NAME_VALID | ACTCTX_FLAG_HMODULE_VALID;
		context.hModule = module;
		context.lpResourceName = MAKEINTRESOURCEW(101);
		const auto activation = CreateActCtxW(&context);
		ULONG_PTR  cookie{};
		const bool activated = activation != INVALID_HANDLE_VALUE && ActivateActCtx(activation, &cookie);
		const auto controls = LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
		using ShowDialog = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
		const auto              show = controls ? reinterpret_cast<ShowDialog>(GetProcAddress(controls, "TaskDialogIndirect")) : nullptr;
		const TASKDIALOG_BUTTON buttons[]{ { exitButton, L"Exit" }, { ignoreButton, L"Ignore" } };
		TASKDIALOGCONFIG        dialog{};
		dialog.cbSize = sizeof(dialog);
		dialog.pszWindowTitle = L"MCM Bridge - Incompatible mod";
		dialog.pszMainInstruction = L"MCM Bridge is disabled";
		dialog.pszContent = taskText.c_str();
		dialog.pszMainIcon = TD_ERROR_ICON;
		dialog.dwFlags = TDF_ENABLE_HYPERLINKS | TDF_SIZE_TO_CONTENT;
		dialog.cButtons = static_cast<UINT>(std::size(buttons));
		dialog.pButtons = buttons;
		dialog.nDefaultButton = exitButton;
		dialog.pfCallback = OnTaskDialog;
		int        selected = exitButton;
		const auto result = show ? show(&dialog, &selected, nullptr, nullptr) : E_NOTIMPL;
		if (controls)
			FreeLibrary(controls);
		if (activated)
			DeactivateActCtx(0, cookie);
		if (activation != INVALID_HANDLE_VALUE)
			ReleaseActCtx(activation);
		if (FAILED(result)) {
			selected = static_cast<int>(DialogBoxParamW(module, MAKEINTRESOURCEW(102), nullptr,
				OnFallbackDialog, reinterpret_cast<LPARAM>(&content)));
		}
		if (selected != ignoreButton) {
			SKSE::log::info("Exiting Skyrim from incompatibility dialog");
			TerminateProcess(GetCurrentProcess(), 0);
			ExitProcess(0);
		}
	}
}
