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
#include <cinttypes>
#include "skse64_common/BranchTrampoline.h"
#include "../xbyak/xbyak.h"
#include <unordered_set>

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

typedef void(*_TESWorldSpace_LoadRNAM)(RE::TESWorldSpace::LargeReferenceData * largeRefrData, ModInfo * modInfo);
RelocAddr<_TESWorldSpace_LoadRNAM> TESWorldSpace_LoadRNAM(0x002B0CE0);
_TESWorldSpace_LoadRNAM orig_TESWorldSpace_LoadRNAM;

RelocAddr<uintptr_t> RNAMHook_Enter = 0x00238D08;

PluginHandle					g_pluginHandle = kPluginHandle_Invalid;

typedef std::unordered_map<UInt32, std::unordered_set<UInt32>> cellFormIDMap;
typedef std::unordered_map<UInt32, UInt32> formIDCellMap;

std::unordered_map<RE::TESWorldSpace *, cellFormIDMap> worldSpaceRNAMMap;
std::unordered_map<RE::TESWorldSpace *, formIDCellMap> worldSpaceFormIDCellMap;

bool hook_TESObjectREFR_LoadForm(RE::TESObjectREFR * refr, ModInfo * modInfo)
{
	bool retVal = orig_TESObjectREFR_LoadForm(refr, modInfo);
	if (retVal)
		_MESSAGE("TESObjectREFR_LoadForm called, formid %d, large ref %d", refr->formID, TESObjectREFR_IsValidLargeRef(refr));
	return retVal;
}

bool hook_TESWorldSpace_LoadForm(RE::TESWorldSpace * worldSpace, ModInfo * modInfo)
{
	_MESSAGE("TESWorldSpace LoadForm called with Worldspace name %s (ptr 0x%016" PRIXPTR ") and plugin %s", dynamic_cast<RE::TESForm*>(worldSpace)->GetName(), worldSpace, modInfo->name);
	
	if (worldSpaceRNAMMap.find(worldSpace) == worldSpaceRNAMMap.end())
	{
		worldSpaceRNAMMap.insert(make_pair(worldSpace, cellFormIDMap()));
		worldSpaceFormIDCellMap.insert(make_pair(worldSpace, formIDCellMap()));
	}

	bool retVal = orig_TESWorldSpace_LoadForm(worldSpace, modInfo);


	//worldSpace->largeReferenceData.cellFormIDMap.clear();
	//worldSpace->largeReferenceData.FormIDCellMap.clear();
	//worldSpace->largeReferenceData.cellFormIDMapFiltered.clear();
	/*_MESSAGE("largeReferenceData.cellFormIDMap count %d largeReferenceData.FormIDCellMap count %d largeReferenceData.cellFormIDMapFiltered count %d", worldSpace->largeReferenceData.cellFormIDMap.size(),
		worldSpace->largeReferenceData.FormIDCellMap.size(), worldSpace->largeReferenceData.cellFormIDMapFiltered.size());*/
	/*
	if (strncmp("Tamriel", dynamic_cast<RE::TESForm*>(worldSpace)->GetName(), 7) == 0)
	{
		int count = 0;
		for (int i = 0; i < worldSpace->largeReferenceData.cellFormIDMap.max_size(); i++)
		{
			auto entry = worldSpace->largeReferenceData.cellFormIDMap._entries[i];

			if (entry.next == nullptr)
				continue;

			_MESSAGE("%d, %d, %d", entry.GetKey().p.x, entry.GetKey().p.y, *entry.GetValue());

			count += *entry.GetValue();
		}
		_MESSAGE("largeReferenceData.cellFormIDMap total count %d", count);
		count = 0;
		for (int i = 0; i < worldSpace->largeReferenceData.cellFormIDMapFiltered.max_size(); i++)
		{
			auto entry = worldSpace->largeReferenceData.cellFormIDMapFiltered._entries[i];

			if (entry.next == nullptr)
				continue;

			count += *entry.GetValue();
		}
		_MESSAGE("largeReferenceData.cellFormIDMapFiltered total count %d", count);
	}*/	

	return retVal;
}

bool hook_TESWorldSpace_LoadBuffer(RE::TESWorldSpace * worldSpace, RE::BGSLoadFormBuffer * buffer)
{
	bool retVal = orig_TESWorldSpace_LoadBuffer(worldSpace, buffer);

	_MESSAGE("TESWorldSpace LoadBuffer called with Worldspace name %s", dynamic_cast<RE::TESForm*>(worldSpace)->GetName());

	return retVal;
}

SKSEMessagingInterface			* g_messaging = nullptr;



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

typedef void(*_ResolveLoadOrderFormID)(UInt32 * formID, ModInfo * modInfo);
RelocAddr<_ResolveLoadOrderFormID> ResolveLoadOrderFormID = 0x001959D0;

constexpr UInt32 rnam_deleted = 0x7FFF7FFF; // 32767, 32767

UInt32 GetRNAMCount(RE::BSTHashMap<RE::TESWorldSpace::XYPlane, UInt32 *> * map)
{
	int count = 0;
	for (int i = 0; i < map->max_size(); i++)
	{
		auto entry = map->_entries[i];

		if (entry.next == nullptr)
			continue;

		count += *entry.GetValue();
	}

	return count;
}
void UpdateWorldspaceRNAM()
{
	_MESSAGE("Load completed, updating all worldspace RNAM data");

	for (auto const& it : worldSpaceRNAMMap)
	{
		RE::TESWorldSpace * ws = it.first;

		auto worldSpaceFormIDCellMapIt = worldSpaceFormIDCellMap.find(ws);

		cellFormIDMap cellMap = it.second;
		formIDCellMap formMap = worldSpaceFormIDCellMapIt->second;

		_MESSAGE("worldspace %s current RNAM cell count %d current filtered RNAM count %d", dynamic_cast<RE::TESForm*>(ws)->GetName(), GetRNAMCount(&ws->largeReferenceData.cellFormIDMap), GetRNAMCount(&ws->largeReferenceData.cellFormIDMapFiltered));
		ws->largeReferenceData.cellFormIDMap.clear();
		ws->largeReferenceData.formIDCellMap.clear();
		ws->largeReferenceData.cellFormIDMapFiltered.clear();
		
		// form id map
		for (auto const& formIt : formMap)
		{
			RE::TESWorldSpace::XYPlane formIDCellPlane = *reinterpret_cast<RE::TESWorldSpace::XYPlane const *>(formIt.second);

			ws->largeReferenceData.formIDCellMap.insert(formIt.first, formIDCellPlane);
		}

		// RNAM maps
		for (auto const& cellIt : cellMap)
		{
			std::unordered_set<UInt32> formIDSet = cellIt.second;
			RE::TESWorldSpace::XYPlane cellPlane = *reinterpret_cast<RE::TESWorldSpace::XYPlane const *>(&cellIt.first);

			UInt32 * formIDArray = static_cast<UInt32 *>(Heap_Allocate((formIDSet.size() + 1) * sizeof(UInt32)));

			formIDArray[0] = formIDSet.size();

			auto i = 1;

			for (auto formID : formIDSet)
			{
				formIDArray[i] = formID;
				i++;
			}

			ws->largeReferenceData.cellFormIDMap.insert(cellPlane, formIDArray);
			ws->largeReferenceData.cellFormIDMapFiltered.insert(cellPlane, formIDArray);
		}

		_MESSAGE("new RNAM count %d", GetRNAMCount(&ws->largeReferenceData.cellFormIDMapFiltered));

	}

	_MESSAGE("finished all worldspaces");
	worldSpaceRNAMMap.clear();
	worldSpaceFormIDCellMap.clear();
}

void SKSEMessageHandler(SKSEMessagingInterface::Message * message)
{
	switch (message->type)
	{
	case SKSEMessagingInterface::kMessage_DataLoaded:
	{
		UpdateWorldspaceRNAM();
	}
	break;
	default:
		break;
	}
}

/*
 * logic
 * 
 * parse RNAM data in plugin in load order
 * RNAM entry:
 *	cell y,x -> refr form ID + refr cell y,x
 *	
 * if cell y,x == refr cell y,x
 *   add to list unless duplicate
 *   if form ID exists in another cell already
 *     remove from other cell, as this reference has been moved
 * if refr cell y,x == 32767, 32767 (max)
 *   refr considered deleted, will be removed from cell it exists in
 * otherwise
 *   discard RNAM since it is not for the correct cell
 */

inline void GatherRNAMs(std::unordered_set<UInt32> * formIDs, UInt32 * rnam, cellFormIDMap * wsCFIDMap, formIDCellMap * wsFIDCMap, ModInfo * modInfo)
{
	const UInt32 currentCell = rnam[0];

	for (UInt32 i = 0; i < rnam[1]; i++)
	{
		UInt32 formID = rnam[i * 2 + 2];
		UInt32 loadOrderFormID = rnam[i * 2 + 2];
		ResolveLoadOrderFormID(&loadOrderFormID, modInfo);
		UInt32 refrCell = rnam[i * 2 + 2 + 1];
		
		RE::TESWorldSpace::XYPlane refrCellPlane = *reinterpret_cast<RE::TESWorldSpace::XYPlane *>(&refrCell);

		const bool inCell = refrCell == currentCell;
		const bool deleted = refrCell == rnam_deleted;
		bool duplicate = false;
		bool moved = false;
		UInt32 oldCell;

		if (inCell && !deleted)
		{
			const auto ret = formIDs->insert(loadOrderFormID);
			if (!ret.second)
				duplicate = true;
			else
			{
				auto formMapIt = wsFIDCMap->find(loadOrderFormID);

				if (formMapIt != wsFIDCMap->end())
				{
					if (formMapIt->second != currentCell)
					{
						moved = true;
						oldCell = formMapIt->second;
						auto wsIt = wsCFIDMap->find(oldCell);
						if (wsIt != wsCFIDMap->end())
						{
							wsIt->second.erase(loadOrderFormID);
						}
						formMapIt->second = currentCell;
					}
				}
				else
				{
					wsFIDCMap->insert(std::make_pair(loadOrderFormID, currentCell));
				}
			}
		}

		if (deleted)
		{
			formIDs->erase(loadOrderFormID);
			wsFIDCMap->erase(loadOrderFormID);

			_MESSAGE("x %d y %d load order formid 0x%08x local formid 0x%08x DELETED", refrCellPlane.y, refrCellPlane.x, loadOrderFormID, formID);
		}
		else if(duplicate)
		{
			_MESSAGE("x %d y %d load order formid 0x%08x local formid 0x%08x DUPLICATE", refrCellPlane.y, refrCellPlane.x, loadOrderFormID, formID);
		}
		else if (!inCell)
		{
			_MESSAGE("x %d y %d load order formid 0x%08x local formid 0x%08x NOT IN CELL", refrCellPlane.y, refrCellPlane.x, loadOrderFormID, formID);
		}
		else if (moved)
		{
			RE::TESWorldSpace::XYPlane oldCellPlane = *reinterpret_cast<RE::TESWorldSpace::XYPlane *>(&oldCell);
			_MESSAGE("x %d y %d load order formid 0x%08x local formid 0x%08x VALID and MOVED from previous cell x %d y %d", refrCellPlane.y, refrCellPlane.x, loadOrderFormID, formID, oldCellPlane.y, oldCellPlane.x);
		}
		else
		{
			_MESSAGE("x %d y %d load order formid 0x%08x local formid 0x%08x VALID", refrCellPlane.y, refrCellPlane.x, loadOrderFormID, formID);
		}
	}
	_MESSAGE("gathered RNAM count in cell %d", formIDs->size());
}

void ReadRNAM(RE::TESWorldSpace * worldSpace, ModInfo * modInfo, UInt32 * rnam)
{
	// note: x and y are swapped on rnam data, maybe all? idk
	RE::TESWorldSpace::XYPlane plane = *reinterpret_cast<RE::TESWorldSpace::XYPlane *>(rnam);

	_MESSAGE("read RNAM hook called on worldspace %s modname %s rnam cell x %d, y %d rnam count %d", dynamic_cast<RE::TESForm*>(worldSpace)->GetName(), modInfo->name, plane.y, plane.x, rnam[1]);

	auto wsRNAMMapIterator = worldSpaceRNAMMap.find(worldSpace);

	if (wsRNAMMapIterator != worldSpaceRNAMMap.end())
	{
		auto wsFormIDCellMapIterator = worldSpaceFormIDCellMap.find(worldSpace);

		if (wsFormIDCellMapIterator != worldSpaceFormIDCellMap.end())
		{
			auto cellFormIDMap = wsRNAMMapIterator->second;
			auto formIDCellMap = wsFormIDCellMapIterator->second;
			const auto cellMapIterator = cellFormIDMap.find(rnam[0]);

			if (cellMapIterator == cellFormIDMap.end())
			{
				std::unordered_set<UInt32> formIDs;
				GatherRNAMs(&formIDs, rnam, &cellFormIDMap, &formIDCellMap, modInfo);
				cellFormIDMap.insert(std::make_pair(rnam[0], formIDs));
			}
			else
			{
				GatherRNAMs(&cellMapIterator->second, rnam, &cellFormIDMap, &formIDCellMap, modInfo);
			}
		}
		else
		{
			_MESSAGE("something went horribly wrong");
		}
	}
	else
	{
		_MESSAGE("something went horribly wrong");
	}
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

		_MESSAGE("Installing Worldspace Load Hook");
		orig_TESWorldSpace_LoadForm = *vtbl_TESWorldSpace_LoadForm;
		SafeWrite64(vtbl_TESWorldSpace_LoadForm.GetUIntPtr(), GetFnAddr(hook_TESWorldSpace_LoadForm));
		//orig_TESWorldSpace_LoadBuffer = *vtbl_TESWorldSpace_LoadBuffer;
		//SafeWrite64(vtbl_TESWorldSpace_LoadBuffer.GetUIntPtr(), GetFnAddr(hook_TESWorldSpace_LoadBuffer));
		_MESSAGE("Installed");

		_MESSAGE("hooking ModInfo_IsMaster");

		constexpr uintptr_t is_master_patch = 0x0017E320;

		RelocAddr<uintptr_t> IsMasterPatch = is_master_patch;

		g_branchTrampoline.Write6Branch(IsMasterPatch.GetUIntPtr(), GetFnAddr(hook_ModInfo_IsMaster));

		_MESSAGE("done");

		//constexpr UInt64 retn_1 = 0xC300000001C0C748;
		//constexpr uintptr_t large_ref_patch = 0x00239D10;

		//RelocAddr<uintptr_t> LargeRefPatch = large_ref_patch;

		//SafeWrite64(LargeRefPatch.GetUIntPtr(), retn_1);

		//_MESSAGE("Disabling game RNAM loader");
		//RelocAddr<uintptr_t> DisableLoadRNAM = 0x00238C40;
		//SafeWrite64(DisableLoadRNAM.GetUIntPtr(), retn_1);
		//_MESSAGE("done");

		/*_MESSAGE("Installing REFR load hook");
		orig_TESObjectREFR_LoadForm = *vtbl_TESObjectREFR_LoadForm;
		SafeWrite64(vtbl_TESObjectREFR_LoadForm.GetUIntPtr(), GetFnAddr(hook_TESObjectREFR_LoadForm));
		_MESSAGE("done");*/

		_MESSAGE("RNAM accumulator hook");
		{
			struct ReadRNAMHook_Code : Xbyak::CodeGenerator
			{
				ReadRNAMHook_Code(void * buf) : Xbyak::CodeGenerator(4096, buf)
				{
					Xbyak::Label readRNAMLabel;

					// save all the caller saved registers because i'm lazy
					push(rax);
					push(rcx);
					push(rdx);
					push(r8);
					push(r9);
					push(r10);
					push(r11);
					mov(rcx, r14); // arg1: TESWorldSpace
					mov(rdx, r13); // arg2: ModInfo
					mov(r8, rdi); // arg3: rnam data
					// again too lazy to check if stack fixing is necessary for this function but it probably will be in release mode
					sub(rsp, 0x20);
					call(ptr[rip + readRNAMLabel]);
					// fix everything
					add(rsp, 0x20);
					pop(r11);
					pop(r10);
					pop(r9);
					pop(r8);
					pop(rdx);
					pop(rcx);
					pop(rax);

					// original code at 0x00238D08
					mov(r12d, ptr[rdi]);
					mov(ptr[rbp + 0x57 + 0x28], r12d);
					mov(ptr[rbp + 0x57 + 0x20], r12d);

					// jump out to 0x00238D13
					jmp(ptr[rip]);
					dq(RNAMHook_Enter.GetUIntPtr() + 0xB);

					L(readRNAMLabel);
					dq(uintptr_t(ReadRNAM));
;				}
			};

			void *codeBuf = g_localTrampoline.StartAlloc();
			ReadRNAMHook_Code code(codeBuf);
			g_localTrampoline.EndAlloc(code.getCurr());

			g_branchTrampoline.Write6Branch(RNAMHook_Enter.GetUIntPtr(), uintptr_t(code.getCode()));

		}
		_MESSAGE("done");
		return true;
	}
};

