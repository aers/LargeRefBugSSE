#include "../skse64/PluginAPI.h"
#include "../skse64_common/skse_version.h"
#include "../skse64_common/Relocation.h"
#include "../skse64_common/SafeWrite.h"
#include "../skse64/GameData.h"
#include <ShlObj.h>
#include <intrin.h>
#include "RE/TESObjectREFR.h"
#include "RE/TESWorldSpace.h"
#include "RE/TESForm.h"
#include "../../../../Program Files (x86)/Microsoft Visual Studio 14.0/VC/include/cinttypes"
#include "skse64_common/BranchTrampoline.h"

#pragma intrinsic(_ReturnAddress)

constexpr uintptr_t vtbl_TESWorldSpace = 0x015883E8;
constexpr uintptr_t vtbl_TESObjectREFR = 0x01583AE0;

typedef bool(*_TESWorldSpace_LoadForm)(RE::TESWorldSpace * worldSpace, ModInfo * modInfo);
RelocPtr<_TESWorldSpace_LoadForm> vtbl_TESWorldSpace_LoadForm(vtbl_TESWorldSpace + 0x6 * 0x8); // ::LoadForm = vtable[6] in TESForm derived classes
_TESWorldSpace_LoadForm orig_TESWorldSpace_LoadForm;

typedef bool(*_TESWorldSpace_LoadBuffer)(RE::TESWorldSpace * worldSpace, RE::BGSLoadFormBuffer * modInfo);
RelocPtr<_TESWorldSpace_LoadBuffer> vtbl_TESWorldSpace_LoadBuffer(vtbl_TESWorldSpace + 0x9 * 0x8); // ::LoadBuffer = vtable[9] in TESForm derived classes
_TESWorldSpace_LoadBuffer orig_TESWorldSpace_LoadBuffer;

typedef bool(*_TESObjectREFR_LoadForm)(RE::TESObjectREFR * ObjectREFR, ModInfo * modInfo);
RelocPtr<_TESObjectREFR_LoadForm> vtbl_TESObjectREFR_LoadForm(vtbl_TESObjectREFR + 0x6 * 0x8); // ::LoadForm = vtable[6] in TESForm derived classes
_TESObjectREFR_LoadForm orig_TESObjectREFR_LoadForm;

typedef bool(*_TESObjectREFR_IsValidLargeRef)(RE::TESObjectREFR * refr);
RelocAddr<_TESObjectREFR_IsValidLargeRef> TESObjectREFR_IsValidLargeRef(0x00239D10);

PluginHandle					g_pluginHandle = kPluginHandle_Invalid;
SKSEMessagingInterface			* g_messaging = nullptr;

void FillRNAM()
{
	_MESSAGE("data load complete, gathering valid large references");
	auto dataHandler = DataHandler::GetSingleton();
	_MESSAGE("total worldspace: %d total refr: %d", dataHandler->arrWRLD.count, dataHandler->arrREFR.count);
}


void SKSEMessageHandler(SKSEMessagingInterface::Message * message)
{
	switch (message->type)
	{
	case SKSEMessagingInterface::kMessage_DataLoaded:
	{
		FillRNAM();
	}
	break;
	default:
		break;
	}
}

bool hook_TESObjectREFR_LoadForm(RE::TESObjectREFR * refr, ModInfo * modInfo)
{
	bool retVal = orig_TESObjectREFR_LoadForm(refr, modInfo);
	if (retVal)
		_MESSAGE("TESObjectREFR_LoadForm called, formid %d, large ref %d", refr->formID, TESObjectREFR_IsValidLargeRef(refr));
	return retVal;
}

bool hook_TESWorldSpace_LoadForm(RE::TESWorldSpace * worldSpace, ModInfo * modInfo)
{
	bool retVal = orig_TESWorldSpace_LoadForm(worldSpace, modInfo);

	_MESSAGE("TESWorldSpace LoadForm called with Worldspace name %s (ptr 0x%016" PRIXPTR ") and plugin %s, unk modinfo hashmap size = %d", dynamic_cast<RE::TESForm*>(worldSpace)->GetName(), worldSpace, modInfo->name, worldSpace->unk1D0.max_size() - worldSpace->unk1D0.free_count());
	//worldSpace->unk250.clear();
	//worldSpace->unk280.clear();
	//worldSpace->unk2B0.clear();
	_MESSAGE("unk250 count %d unk280 count %d unk2B0 count %d", worldSpace->unk250.size(),
		worldSpace->unk280.size(), worldSpace->unk2B0.size());
	/*
	if (strncmp("Tamriel", dynamic_cast<RE::TESForm*>(worldSpace)->GetName(), 7) == 0)
	{
		int count = 0;
		for (int i = 0; i < worldSpace->unk250.max_size(); i++)
		{
			auto entry = worldSpace->unk250._entries[i];

			if (entry.next == nullptr)
				continue;

			_MESSAGE("%d, %d, %d", entry.GetKey().p.x, entry.GetKey().p.y, *entry.GetValue());

			count += *entry.GetValue();
		}
		_MESSAGE("unk250 total count %d", count);
		count = 0;
		for (int i = 0; i < worldSpace->unk2B0.max_size(); i++)
		{
			auto entry = worldSpace->unk2B0._entries[i];

			if (entry.next == nullptr)
				continue;

			count += *entry.GetValue();
		}
		_MESSAGE("unk2B0 total count %d", count);
	}*/	

	return retVal;
}

bool hook_TESWorldSpace_LoadBuffer(RE::TESWorldSpace * worldSpace, RE::BGSLoadFormBuffer * buffer)
{
	bool retVal = orig_TESWorldSpace_LoadBuffer(worldSpace, buffer);

	_MESSAGE("TESWorldSpace LoadBuffer called with Worldspace name %s", dynamic_cast<RE::TESForm*>(worldSpace)->GetName());

	return retVal;
}

char hook_ModInfo_IsMaster(ModInfo * modInfo)
{
	if (!(modInfo->unk438 & 1))
	{
		uintptr_t returnAddr = (uintptr_t)(_ReturnAddress()) - RelocationManager::s_baseAddr;
		//_MESSAGE("hook_ModInfo_IsMaster called with return address 0x%016" PRIXPTR " for plugin %s", returnAddr, modInfo->name);
		/*returnAddr == 0x1706E8 ||
			returnAddr == 0x2B0D51 ||
			returnAddr == 0x19489E ||
			returnAddr == 0x2B0ECD ||
			returnAddr == 0x2B104B ||
			//returnAddr == 0x171694 ||
			//returnAddr == 0x2B1D74 ||
			//returnAddr == 0x2B226A ||
			//returnAddr == 0x167444 ||
			//returnAddr == 0x2857EE ||
			//returnAddr == 0x285A09 ||
			//returnAddr == 0x25FA28 ||*/
		if (returnAddr == 0x171526)
		{
			_MESSAGE("faking ESM for %s, return address 0x%016" PRIXPTR " (large reference loading)", modInfo->name, returnAddr);
			return 1;
		}
		if(returnAddr == 0x26AC05)
		{
			_MESSAGE("faking ESM for %s, return address 0x%016" PRIXPTR " (normal reference loading)", modInfo->name, returnAddr);
			return 1;
		}
	}

	return modInfo->unk438 & 1;
}


extern "C" {
	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{
		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\LargeRef.log");
		gLog.SetPrintLevel(IDebugLog::kLevel_Error);
		gLog.SetLogLevel(IDebugLog::kLevel_DebugMessage);

		_MESSAGE("LargeRefBug");

		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "large ref bug";
		info->version = 1;

		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor)
		{
			_MESSAGE("loaded in editor, marking as incompatible");
			return false;
		}
		else if (skse->runtimeVersion != RUNTIME_VERSION_1_5_62)
		{
			_MESSAGE("unsupported runtime version %08X", skse->runtimeVersion);
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 64))
		{
			_FATALERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_localTrampoline.Create(1024 * 64, nullptr))
		{
			_FATALERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		g_messaging = (SKSEMessagingInterface *)skse->QueryInterface(kInterface_Messaging);
		if (!g_messaging) {
			_ERROR("couldn't get messaging interface, disabling patches that require it");
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse) {

		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "SKSE", SKSEMessageHandler);

		//_MESSAGE("Installing Worldspace Load Hook");
		//orig_TESWorldSpace_LoadForm = *vtbl_TESWorldSpace_LoadForm;
		//SafeWrite64(vtbl_TESWorldSpace_LoadForm.GetUIntPtr(), GetFnAddr(hook_TESWorldSpace_LoadForm));
		//orig_TESWorldSpace_LoadBuffer = *vtbl_TESWorldSpace_LoadBuffer;
		//SafeWrite64(vtbl_TESWorldSpace_LoadBuffer.GetUIntPtr(), GetFnAddr(hook_TESWorldSpace_LoadBuffer));
		//_MESSAGE("Installed");

		_MESSAGE("hooking ModInfo_IsMaster");

		constexpr uintptr_t is_master_patch = 0x0017E320;

		RelocAddr<uintptr_t> IsMasterPatch = is_master_patch;

		g_branchTrampoline.Write6Branch(IsMasterPatch.GetUIntPtr(), GetFnAddr(hook_ModInfo_IsMaster));

		_MESSAGE("done");

		constexpr UInt64 retn_1 = 0xC300000001C0C748;
		//constexpr uintptr_t large_ref_patch = 0x00239D10;

		//RelocAddr<uintptr_t> LargeRefPatch = large_ref_patch;

		//SafeWrite64(LargeRefPatch.GetUIntPtr(), retn_1);

		//_MESSAGE("Disabling game RNAM loader");
		//RelocAddr<uintptr_t> DisableLoadRNAM = 0x00238C40;
		//SafeWrite64(DisableLoadRNAM.GetUIntPtr(), retn_1);
		//_MESSAGE("done");

		_MESSAGE("Installing REFR load hook");
		orig_TESObjectREFR_LoadForm = *vtbl_TESObjectREFR_LoadForm;
		SafeWrite64(vtbl_TESObjectREFR_LoadForm.GetUIntPtr(), GetFnAddr(hook_TESObjectREFR_LoadForm));
		_MESSAGE("done");
		return true;
	}
};

