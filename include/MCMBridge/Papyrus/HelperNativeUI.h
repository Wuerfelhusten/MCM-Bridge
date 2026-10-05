#pragma once

namespace MCMBridge
{
	// Startup only, before native host admission. Unknown or modified binaries
	// remain unpatched. This installs menu capture, not the complete Helper host.
	bool InstallHelperMenuCapture();
	bool InstallHelperHostCapture();
	bool IsHelperHostReady();
}
