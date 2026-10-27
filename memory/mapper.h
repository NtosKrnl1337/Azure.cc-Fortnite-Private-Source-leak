#pragma once
#include <windows.h>
#include <string>
#include <src/memory/DigiMapperApi.hpp>
#include <src/memory/driver_bytes.h>

inline bool MapDriverLocally() {
	if (azure_driver_size == 0)
		return false;

	DigiMapperApi::MapperConfig config{};
	NTSTATUS exit_code = 0;
	const ULONG64 result = DigiMapperApi::MapFromBytes(
		azure_driver,
		azure_driver_size,
		config,
		&exit_code
	);

	return result != 0 && exit_code == 0;
}

inline bool HvciOn() {
	HKEY h = NULL;
	DWORD v = 0, sz = sizeof(v), t = 0;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
		"SYSTEM\\CurrentControlSet\\Control\\DeviceGuard\\Scenarios\\HypervisorEnforcedCodeIntegrity",
		0, KEY_READ, &h) == ERROR_SUCCESS) {
		DWORD r = RegQueryValueExA(h, "Enabled", NULL, &t, (LPBYTE)&v, &sz);
		RegCloseKey(h);
		if (r == ERROR_SUCCESS && v == 1) return true;
	}
	return false;
}

inline bool IsElevated() {
	BOOL admin = FALSE;
	SID_IDENTIFIER_AUTHORITY nt = SECURITY_NT_AUTHORITY;
	PSID g = NULL;
	if (AllocateAndInitializeSid(&nt, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
		0, 0, 0, 0, 0, 0, &g)) {
		CheckTokenMembership(NULL, g, &admin);
		FreeSid(g);
	}
	return admin != FALSE;
}

inline std::string OsBuild() {
	HKEY h = NULL;
	char b[64] = { 0 };
	DWORD sz = sizeof(b), t = 0;
	if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
		0, KEY_READ, &h) == ERROR_SUCCESS) {
		if (RegQueryValueExA(h, "CurrentBuild", NULL, &t, (LPBYTE)b, &sz) == ERROR_SUCCESS) {
			RegCloseKey(h);
			return b;
		}
		RegCloseKey(h);
	}
	return "?";
}

inline bool LoadDriver() {
	return MapDriverLocally();
}
