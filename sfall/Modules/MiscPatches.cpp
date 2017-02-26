/*
*    sfall
*    Copyright (C) 2008-2016  The sfall team
*
*    This program is free software: you can redistribute it and/or modify
*    it under the terms of the GNU General Public License as published by
*    the Free Software Foundation, either version 3 of the License, or
*    (at your option) any later version.
*
*    This program is distributed in the hope that it will be useful,
*    but WITHOUT ANY WARRANTY; without even the implied warranty of
*    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*    GNU General Public License for more details.
*
*    You should have received a copy of the GNU General Public License
*    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <math.h>
#include <stdio.h>

#include "..\main.h"
#include "..\FalloutEngine\Fallout2.h"
#include "..\SimplePatch.h"
#include "Graphics.h"
#include "ScriptExtender.h"

#include "MiscPatches.h"

// TODO: split this into smaller files

static char mapName[65];
static char configName[65];
static char patchName[65];
static char versionString[65];

static const char* debugLog = "LOG";
static const char* debugGnw = "GNW";

static int* scriptDialog = nullptr;

bool NpcAutoLevelEnabled;

static const DWORD PutAwayWeapon[] = {
	0x411EA2, // action_climb_ladder_
	0x412046, // action_use_an_item_on_object_
	0x41224A, // action_get_an_object_
	0x4606A5, // intface_change_fid_animate_
	0x472996, // invenWieldFunc_
};

static const DWORD dinputPos = 0x50FB70;
static const DWORD DDrawPos = 0x50FB5C;

static const DWORD FastShotFixF1[] = {
	0x478BB8, 0x478BC7, 0x478BD6, 0x478BEA, 0x478BF9, 0x478C08, 0x478C2F,
};

static const DWORD script_dialog_msgs[] = {
	0x4A50C2, 0x4A5169, 0x4A52FA, 0x4A5302, 0x4A6B86, 0x4A6BE0, 0x4A6C37,
};


static double FadeMulti;
static __declspec(naked) void FadeHook() {
	__asm {
		pushf;
		push ebx;
		fild[esp];
		fmul FadeMulti;
		fistp[esp];
		pop ebx;
		popf;
		call FuncOffs::fadeSystemPalette_;
		retn;
	}
}

static void __declspec(naked) Combat_p_procFix() {
	__asm {
		push eax;

		mov eax, dword ptr ds : [VARPTR_combat_state];
		cmp eax, 3;
		jnz end_cppf;

		push esi;
		push ebx;
		push edx;

		mov esi, VARPTR_main_ctd;
		mov eax, [esi];
		mov ebx, [esi + 0x20];
		xor edx, edx;
		mov eax, [eax + 0x78];
		call FuncOffs::scr_set_objs_;
		mov eax, [esi];

		cmp dword ptr ds : [esi + 0x2c], +0x0;
		jng jmp1;

		test byte ptr ds : [esi + 0x15], 0x1;
		jz jmp1;
		mov edx, 0x2;
		jmp jmp2;
jmp1:
		mov edx, 0x1;
jmp2:
		mov eax, [eax + 0x78];
		call FuncOffs::scr_set_ext_param_;
		mov eax, [esi];
		mov edx, 0xd;
		mov eax, [eax + 0x78];
		call FuncOffs::exec_script_proc_;
		pop edx;
		pop ebx;
		pop esi;

end_cppf:
		pop eax;
		call FuncOffs::stat_level_;

		retn;
	}
}

static void __declspec(naked) WeaponAnimHook() {
	__asm {
		cmp edx, 11;
		je c11;
		cmp edx, 15;
		je c15;
		jmp FuncOffs::art_get_code_;
c11:
		mov edx, 16;
		jmp FuncOffs::art_get_code_;
c15:
		mov edx, 17;
		jmp FuncOffs::art_get_code_;
	}
}
static void __declspec(naked) intface_rotate_numbers_hack() {
	__asm {
		push edi
		push ebp
		sub  esp, 0x54
		// ebx=old value, ecx=new value
		cmp  ebx, ecx
		je   end
		mov  ebx, ecx
		jg   decrease
		dec  ebx
		jmp  end
decrease:
		test ecx, ecx
		jl   negative
		inc  ebx
		jmp  end
negative:
		xor  ebx, ebx
end:
		push 0x460BA6
		retn
	}
}

static void __declspec(naked) register_object_take_out_hack() {
	__asm {
		push ecx
		push eax
		mov  ecx, edx                             // ID1
		mov  edx, [eax + 0x1C]                      // cur_rot
		inc  edx
		push edx                                  // ID3
		xor  ebx, ebx                             // ID2
		mov  edx, [eax + 0x20]                      // fid
		and edx, 0xFFF                           // Index
		xor eax, eax
		inc  eax                                  // Obj_Type
		call FuncOffs::art_id_
		xor  ebx, ebx
		dec  ebx
		xchg edx, eax
		pop  eax
		call FuncOffs::register_object_change_fid_
		pop  ecx
		xor  eax, eax
		retn
	}
}

static void __declspec(naked) gdAddOptionStr_hack() {
	__asm {
		mov  ecx, ds:[VARPTR_gdNumOptions]
		add  ecx, '1'
		push ecx
		push 0x4458FA
		retn
	}
}

static void __declspec(naked) ScienceCritterCheckHook() {
	__asm {
		cmp esi, ds:[VARPTR_obj_dude];
		jne end;
		mov eax, 10;
		retn;
end:
		jmp FuncOffs::critter_kill_count_type_;
	}
}

static const DWORD FastShotTraitFixEnd1 = 0x478E7F;
static const DWORD FastShotTraitFixEnd2 = 0x478E7B;
static void __declspec(naked) FastShotTraitFix() {
	__asm {
		test eax, eax;				// does player have Fast Shot trait?
		je ajmp;				// skip ahead if no
		mov edx, ecx;				// argument for item_w_range_: hit_mode
		mov eax, ebx;				// argument for item_w_range_: pointer to source_obj (always dude_obj due to code path)
		call FuncOffs::item_w_range_;			// get weapon's range
		cmp eax, 0x2;				// is weapon range less than or equal 2 (i.e. melee/unarmed attack)?
		jle ajmp;				// skip ahead if yes
		xor eax, eax;				// otherwise, disallow called shot attempt
		jmp bjmp;
ajmp:
		jmp FastShotTraitFixEnd1;		// continue processing called shot attempt
bjmp:
		jmp FastShotTraitFixEnd2;		// clean up and exit function item_w_called_shot
	}
}

static void __declspec(naked) ReloadHook() {
	__asm {
		push eax;
		push ebx;
		push edx;
		mov eax, dword ptr ds:[VARPTR_obj_dude];
		call FuncOffs::register_clear_;
		xor eax, eax;
		inc eax;
		call FuncOffs::register_begin_;
		xor edx, edx;
		xor ebx, ebx;
		mov eax, dword ptr ds:[VARPTR_obj_dude];
		dec ebx;
		call FuncOffs::register_object_animate_;
		call FuncOffs::register_end_;
		pop edx;
		pop ebx;
		pop eax;
		jmp FuncOffs::gsound_play_sfx_file_;
	}
}


static const DWORD CorpseHitFix2_continue_loop1 = 0x48B99B;
static void __declspec(naked) CorpseHitFix2() {
	__asm {
		push eax;
		mov eax, [eax];
		call FuncOffs::critter_is_dead_; // found some object, check if it's a dead critter
		test eax, eax;
		pop eax;
		jz really_end; // if not, allow breaking the loop (will return this object)
		jmp CorpseHitFix2_continue_loop1; // otherwise continue searching

really_end:
		mov     eax, [eax];
		pop     ebp;
		pop     edi;
		pop     esi;
		pop     ecx;
		retn;
	}
}

static const DWORD CorpseHitFix2_continue_loop2 = 0x48BA0B;
// same logic as above, for different loop
static void __declspec(naked) CorpseHitFix2b() {
	__asm {
		mov eax, [edx];
		call FuncOffs::critter_is_dead_;
		test eax, eax;
		jz really_end;
		jmp CorpseHitFix2_continue_loop2;

really_end:
		mov     eax, [edx];
		pop     ebp;
		pop     edi;
		pop     esi;
		pop     ecx;
		retn;
	}
}

static DWORD RetryCombatLastAP;
static DWORD RetryCombatMinAP;
static void __declspec(naked) RetryCombatHook() {
	__asm {
		mov RetryCombatLastAP, 0;
retry:
		mov eax, esi;
		xor edx, edx;
		call FuncOffs::combat_ai_;
process:
		cmp dword ptr ds:[VARPTR_combat_turn_running], 0;
		jle next;
		call FuncOffs::process_bk_;
		jmp process;
next:
		mov eax, [esi+0x40];
		cmp eax, RetryCombatMinAP;
		jl end;
		cmp eax, RetryCombatLastAP;
		je end;
		mov RetryCombatLastAP, eax;
		jmp retry;
end:
		retn;
	}
}

static const DWORD NPCStage6Fix1End = 0x493D16;
static const DWORD NPCStage6Fix2End = 0x49423A;
static void __declspec(naked) NPCStage6Fix1() {
	__asm {
		mov eax,0xcc;				// set record size to 204 bytes
		imul eax,edx;				// multiply by number of NPC records in party.txt
		call FuncOffs::mem_malloc_;			// malloc the necessary memory
		mov edx,dword ptr ds:[VARPTR_partyMemberMaxCount];	// retrieve number of NPC records in party.txt
		mov ebx,0xcc;				// set record size to 204 bytes
		imul ebx,edx;				// multiply by number of NPC records in party.txt
		jmp NPCStage6Fix1End;			// call memset to set all malloc'ed memory to 0
	}
}

static void __declspec(naked) NPCStage6Fix2() {
	__asm {
		mov eax,0xcc;				// record size is 204 bytes
		imul edx,eax;				// multiply by NPC number as listed in party.txt
		mov eax,dword ptr ds:[VARPTR_partyMemberAIOptions];	// get starting offset of internal NPC table
		jmp NPCStage6Fix2End;			// eax+edx = offset of specific NPC record
	}
}

static const DWORD ScannerHookRet=0x41BC1D;
static const DWORD ScannerHookFail=0x41BC65;
static void __declspec(naked) ScannerAutomapHook() {
	__asm {
		mov eax, ds:[VARPTR_obj_dude];
		mov edx, PID_MOTION_SENSOR;
		call FuncOffs::inven_pid_is_carried_ptr_;
		test eax, eax;
		jz fail;
		mov edx, eax;
		jmp ScannerHookRet;
fail:
		jmp ScannerHookFail;
	}
}

static void __declspec(naked) objCanSeeObj_ShootThru_Fix() {//(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags)
	__asm {
		push esi
		push edi

		push FuncOffs::obj_shoot_blocking_at_ //arg3 check hex objects func pointer
		mov esi, 0x20//arg2 flags, 0x20 = check shootthru
		push esi
		mov edi, dword ptr ss : [esp + 0x14] //arg1 **ret_objStruct
		push edi
		call FuncOffs::make_straight_path_func_;//(EAX *objStruct, EDX hexNum1, EBX hexNum2, ECX ?, stack1 **ret_objStruct, stack2 flags, stack3 *check_hex_objs_func)

		pop edi
		pop esi
		ret 0x8
	}
}

static const DWORD EncounterTableSize[] = {
	0x4BD1A3, 0x4BD1D9, 0x4BD270, 0x4BD604, 0x4BDA14, 0x4BDA44, 0x4BE707,
	0x4C0815, 0x4C0D4A, 0x4C0FD4,
};

void ApplyInputPatch() {
	//if(GetPrivateProfileIntA("Input", "Enable", 0, ini)) {
		dlog("Applying input patch.", DL_INIT);
		SafeWriteStr(dinputPos, "ddraw.dll");
		AvailableGlobalScriptTypes |= 1;
		dlogr(" Done", DL_INIT);
	//}
}

void ApplyGraphicsPatch() {
	DWORD fadeMulti;
	DWORD GraphicsMode = GetPrivateProfileIntA("Graphics", "Mode", 0, ini);
	if (GraphicsMode != 4 && GraphicsMode != 5) {
		GraphicsMode = 0;
	}
	if (GraphicsMode == 4 || GraphicsMode == 5) {
		dlog("Applying dx9 graphics patch.", DL_INIT);
#ifdef WIN2K
#define _DLL_NAME "d3dx9_42.dll"
#else
#define _DLL_NAME "d3dx9_43.dll"
#endif
		HMODULE h = LoadLibraryEx(_DLL_NAME, 0, LOAD_LIBRARY_AS_DATAFILE);
		if (!h) {
			MessageBoxA(0, "You have selected graphics mode 4 or 5, but " _DLL_NAME " is missing\nSwitch back to mode 0, or install an up to date version of DirectX", "Error", 0);
			ExitProcess(-1);
		} else {
			FreeLibrary(h);
		}
		SafeWrite8(0x0050FB6B, '2');
		dlogr(" Done", DL_INIT);
#undef _DLL_NAME
	}
	fadeMulti = GetPrivateProfileIntA("Graphics", "FadeMultiplier", 100, ini);
	if (fadeMulti != 100) {
		dlog("Applying fade patch.", DL_INIT);
		SafeWrite32(0x00493B17, ((DWORD)&FadeHook) - 0x00493B1b);
		FadeMulti = ((double)fadeMulti) / 100.0;
		dlogr(" Done", DL_INIT);
	}
}

void ApplyDebugModePatch() {
	if (IsDebug) {
		DWORD dbgMode = GetPrivateProfileIntA("Debugging", "DebugMode", 0, ".\\ddraw.ini");
		if (dbgMode) {
			dlog("Applying debugmode patch.", DL_INIT);
			//If the player is using an exe with the debug patch already applied, just skip this block without erroring
			if (*((DWORD*)0x00444A64) != 0x082327e8) {
				SafeWrite32(0x00444A64, 0x082327e8);
				SafeWrite32(0x00444A68, 0x0120e900);
				SafeWrite8(0x00444A6D, 0);
				SafeWrite32(0x00444A6E, 0x90909090);
			}
			SafeWrite8(0x004C6D9B, 0xb8);
			if (dbgMode == 1) {
				SafeWrite32(0x004C6D9C, (DWORD)debugGnw);
			}
			else {
				SafeWrite32(0x004C6D9C, (DWORD)debugLog);
			}
			dlogr(" Done", DL_INIT);
		}
	}
}

void ApplyNPCAutoLevelPatch() {
	NpcAutoLevelEnabled = GetPrivateProfileIntA("Misc", "NPCAutoLevel", 0, ini) != 0;
	if (NpcAutoLevelEnabled) {
		dlog("Applying npc autolevel patch.", DL_INIT);
		SafeWrite16(0x00495D22, 0x9090);
		SafeWrite32(0x00495D24, 0x90909090);
		dlogr(" Done", DL_INIT);
	}

	if (GetPrivateProfileIntA("Misc", "SingleCore", 1, ini)) {
		dlog("Applying single core patch.", DL_INIT);
		HANDLE process = GetCurrentProcess();
		SetProcessAffinityMask(process, 1);
		CloseHandle(process);
		dlogr(" Done", DL_INIT);
	}

	if (GetPrivateProfileIntA("Misc", "OverrideArtCacheSize", 0, ini)) {
		dlog("Applying override art cache size patch.", DL_INIT);
		SafeWrite32(0x00418867, 0x90909090);
		SafeWrite32(0x00418872, 256);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyAdditionalWeaponAnimsPatch() {
	if (GetPrivateProfileIntA("Misc", "AdditionalWeaponAnims", 0, ini)) {
		dlog("Applying additional weapon animations patch.", DL_INIT);
		SafeWrite8(0x419320, 0x12);
		HookCall(0x4194CC, WeaponAnimHook);
		HookCall(0x451648, WeaponAnimHook);
		HookCall(0x451671, WeaponAnimHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplySkilldexImagesPatch() {
	DWORD tmp;
	dlog("Checking for changed skilldex images.", DL_INIT);
	tmp = GetPrivateProfileIntA("Misc", "Lockpick", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D54, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Steal", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D58, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Traps", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D5C, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "FirstAid", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D4C, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Doctor", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D50, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Science", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D60, tmp);
	}
	tmp = GetPrivateProfileIntA("Misc", "Repair", 293, ini);
	if (tmp != 293) {
		SafeWrite32(0x00518D64, tmp);
	}
	dlogr(" Done", DL_INIT);
}

void ApplySpeedInterfaceCounterAnimsPatch() {
	switch (GetPrivateProfileIntA("Misc", "SpeedInterfaceCounterAnims", 0, ini)) {
	case 1:
		dlog("Applying SpeedInterfaceCounterAnims patch.", DL_INIT);
		MakeCall(0x460BA1, &intface_rotate_numbers_hack, true);
		dlogr(" Done", DL_INIT);
		break;
	case 2:
		dlog("Applying SpeedInterfaceCounterAnims patch. (Instant)", DL_INIT);
		SafeWrite32(0x460BB6, 0x90DB3190);
		dlogr(" Done", DL_INIT);
		break;
	}
}

void ApplyScienceOnCrittersPatch() {
	switch (GetPrivateProfileIntA("Misc", "ScienceOnCritters", 0, ini)) {
	case 1:
		HookCall(0x41276E, ScienceCritterCheckHook);
		break;
	case 2:
		SafeWrite8(0x41276A, 0xeb);
		break;
	}
}

void ApplyFashShotTraitFix() {
	switch (GetPrivateProfileIntA("Misc", "FastShotFix", 1, ini)) {
	case 1:
		dlog("Applying Fast Shot Trait Fix.", DL_INIT);
		MakeCall(0x478E75, &FastShotTraitFix, true);
		dlogr(" Done", DL_INIT);
		break;
	case 2:
		dlog("Applying Fast Shot Trait Fix. (Fallout 1 version)", DL_INIT);
		SafeWrite16(0x478C9F, 0x9090);
		for (int i = 0; i < sizeof(FastShotFixF1) / 4; i++) {
			HookCall(FastShotFixF1[i], (void*)0x478C7D);
		}
		dlogr(" Done", DL_INIT);
		break;
	}
}

void ApplyBoostScriptDialogLimitPatch() {
	if (GetPrivateProfileIntA("Misc", "BoostScriptDialogLimit", 0, ini)) {
		const int scriptDialogCount = 10000;
		dlog("Applying script dialog limit patch.", DL_INIT);
		scriptDialog = new int[scriptDialogCount * 2]; // Because the msg structure is 8 bytes, not 4.
		SafeWrite32(0x4A50E3, scriptDialogCount); // scr_init
		SafeWrite32(0x4A519F, scriptDialogCount); // scr_game_init
		SafeWrite32(0x4A534F, scriptDialogCount * 8); // scr_message_free
		for (int i = 0; i < sizeof(script_dialog_msgs) / 4; i++) {
			SafeWrite32(script_dialog_msgs[i], (DWORD)scriptDialog); // scr_get_dialog_msg_file
		}
		dlogr(" Done", DL_INIT);
	}
}

void ApplyNumbersInDialoguePatch() {
	if (GetPrivateProfileIntA("Misc", "NumbersInDialogue", 0, ini)) {
		dlog("Applying numbers in dialogue patch.", DL_INIT);
		SafeWrite32(0x502C32, 0x2000202E);
		SafeWrite8(0x446F3B, 0x35);
		SafeWrite32(0x5029E2, 0x7325202E);
		SafeWrite32(0x446F03, 0x2424448B);        // mov  eax, [esp+0x24]
		SafeWrite8(0x446F07, 0x50);               // push eax
		SafeWrite32(0x446FE0, 0x2824448B);        // mov  eax, [esp+0x28]
		SafeWrite8(0x446FE4, 0x50);               // push eax
		MakeCall(0x4458F5, &gdAddOptionStr_hack, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyInstantWeaponEquipPatch() {
	if (GetPrivateProfileIntA("Misc", "InstantWeaponEquip", 0, ini)) {
		//Skip weapon equip/unequip animations
		dlog("Applying instant weapon equip patch.", DL_INIT);
		for (int i = 0; i < sizeof(PutAwayWeapon) / 4; i++) {
			SafeWrite8(PutAwayWeapon[i], 0xEB);   // jmps
		}
		BlockCall(0x472AD5);                      //
		BlockCall(0x472AE0);                      // invenUnwieldFunc_
		BlockCall(0x472AF0);                      //
		MakeCall(0x415238, &register_object_take_out_hack, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyCombatProcFix() {
	//Ray's combat_p_proc fix
		SafeWrite32(0x0425253, ((DWORD)&Combat_p_procFix) - 0x0425257);
		SafeWrite8(0x0424dbc, 0xE9);
		SafeWrite32(0x0424dbd, 0x00000034);
		dlogr(" Done", DL_INIT);
	//}
}

void ApplyMultiPatchesPatch() {
	if (GetPrivateProfileIntA("Misc", "MultiPatches", 0, ini)) {
		dlog("Applying load multiple patches patch.", DL_INIT);
		SafeWrite8(0x444354, 0x90); // Change step from 2 to 1
		SafeWrite8(0x44435C, 0xC4); // Disable check
		dlogr(" Done", DL_INIT);
	}
}

void ApplyPlayIdleAnimOnReloadPatch() {
	if (GetPrivateProfileInt("Misc", "PlayIdleAnimOnReload", 0, ini)) {
		dlog("Applying idle anim on reload patch.", DL_INIT);
		HookCall(0x460B8C, ReloadHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyCorpseLineOfFireFix() {
	if (GetPrivateProfileInt("Misc", "CorpseLineOfFireFix", 0, ini)) {
		dlog("Applying corpse line of fire patch.", DL_INIT);
		MakeCall(0x48B994, CorpseHitFix2, true);
		MakeCall(0x48BA04, CorpseHitFix2b, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyNpcExtraApPatch() {
	RetryCombatMinAP = GetPrivateProfileIntA("Misc", "NPCsTryToSpendExtraAP", 0, ini);
	if (RetryCombatMinAP > 0) {
		dlog("Applying retry combat patch.", DL_INIT);
		HookCall(0x422B94, &RetryCombatHook);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyNpcStage6Fix() {
	if (GetPrivateProfileIntA("Misc", "NPCStage6Fix", 0, ini)) {
		dlog("Applying NPC Stage 6 Fix.", DL_INIT);
		MakeCall(0x493CE9, &NPCStage6Fix1, true);
		SafeWrite8(0x494063, 0x06);		// loop should look for a potential 6th stage
		SafeWrite8(0x4940BB, 0xCC);		// move pointer by 204 bytes instead of 200
		MakeCall(0x494224, &NPCStage6Fix2, true);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyMotionScannerFlagsPatch() {
	DWORD flags;
	if (flags = GetPrivateProfileIntA("Misc", "MotionScannerFlags", 1, ini)) {
		dlog("Applying MotionScannerFlags patch.", DL_INIT);
		if (flags & 1) MakeCall(0x41BBE9, &ScannerAutomapHook, true);
		if (flags & 2) BlockCall(0x41BC3C);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyEncounterTableSizePatch() {
	DWORD tableSize = GetPrivateProfileIntA("Misc", "EncounterTableSize", 0, ini);
	if (tableSize > 40 && tableSize <= 127) {
		dlog("Applying EncounterTableSize patch.", DL_INIT);
		SafeWrite8(0x4BDB17, (BYTE)tableSize);
		DWORD nsize = (tableSize + 1) * 180 + 0x50;
		for (int i = 0; i < sizeof(EncounterTableSize) / 4; i++) {
			SafeWrite32(EncounterTableSize[i], nsize);
		}
		SafeWrite8(0x4BDB17, (BYTE)tableSize);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyObjCanSeeShootThroughPatch() {
	if (GetPrivateProfileIntA("Misc", "ObjCanSeeObj_ShootThru_Fix", 0, ini)) {
		dlog("Applying ObjCanSeeObj ShootThru Fix.", DL_INIT);
		SafeWrite32(0x456BC7, (DWORD)&objCanSeeObj_ShootThru_Fix - 0x456BCB);
		dlogr(" Done", DL_INIT);
	}
}

static const char* musicOverridePath = "data\\sound\\music\\";
void ApplyOverrideMusicDirPatch() {
	DWORD overrideMode;
	if (overrideMode = GetPrivateProfileIntA("Sound", "OverrideMusicDir", 0, ini)) {
		SafeWrite32(0x4449C2, (DWORD)musicOverridePath);
		SafeWrite32(0x4449DB, (DWORD)musicOverridePath);
		if (overrideMode == 2) {
			SafeWrite32(0x518E78, (DWORD)musicOverridePath);
			SafeWrite32(0x518E7C, (DWORD)musicOverridePath);
		}
	}
}

void ApplyDialogueFix() {
	if (GetPrivateProfileIntA("Misc", "DialogueFix", 1, ini)) {
		dlog("Applying dialogue patch.", DL_INIT);
		SafeWrite8(0x00446848, 0x31);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyDontDeleteProtosPatch() {	
	if (IsDebug && GetPrivateProfileIntA("Debugging", "DontDeleteProtos", 0, ".\\ddraw.ini")) {
		dlog("Applying permanent protos patch.", DL_INIT);
		SafeWrite8(0x48007E, 0xeb);
		dlogr(" Done", DL_INIT);
	}
}

void ApplyAlwaysReloadMsgs() {
	if (GetPrivateProfileInt("Misc", "AlwaysReloadMsgs", 0, ini)) {
		dlog("Applying always reload messages patch.", DL_INIT);
		SafeWrite8(0x4A6B8A, 0xff);
		SafeWrite32(0x4A6B8B, 0x02eb0074);
		dlogr(" Done", DL_INIT);
	}
}

void SkipOpeningMoviesPatch() {	
	if (GetPrivateProfileIntA("Misc", "SkipOpeningMovies", 0, ini)) {
		dlog("Blocking opening movies.", DL_INIT);
		BlockCall(0x4809CB);
		BlockCall(0x4809D4);
		BlockCall(0x4809E0);
		dlogr(" Done", DL_INIT);
	}
}

void RemoveWindowRoundingPatch() {
	if(GetPrivateProfileIntA("Misc", "RemoveWindowRounding", 0, ini)) {
		SafeWrite32(0x4B8090, 0x90909090);
		SafeWrite16(0x4B8094, 0x9090);
	}
}

void InventoryCharacterRotationSpeedPatch() {
	DWORD setting = GetPrivateProfileIntA("Misc", "SpeedInventoryPCRotation", 166, ini);
	if (setting != 166 && setting <= 1000) {
		dlog("Applying SpeedInventoryPCRotation patch.", DL_INIT);
		SafeWrite32(0x47066B, setting);
		dlogr(" Done", DL_INIT);
	}
}

void UIAnimationSpeedPatch() {
	DWORD addrs[2] = {0x45F9DE, 0x45FB33};
	SimplePatch<WORD>(addrs, 2, "Misc", "CombatPanelAnimDelay", 1000, 0, 65535);
	addrs[0] = 0x447DF4; addrs[1] = 0x447EB6;
	SimplePatch<BYTE>(addrs, 2, "Misc", "DialogPanelAnimDelay", 33, 0, 255);
	addrs[0] = 0x499B99; addrs[1] = 0x499DA8;
	SimplePatch<BYTE>(addrs, 2, "Misc", "PipboyTimeAnimDelay", 50, 0, 127);
}

void MusicInDialoguePatch() {
	if (GetPrivateProfileIntA("Misc", "EnableMusicInDialogue", 0, ini)) {
		dlog("Applying music in dialogue patch.", DL_INIT);
		SafeWrite8(0x44525B, 0x0);
		//BlockCall(0x450627);
		dlogr(" Done", DL_INIT);
	}
}

void MiscPatches::init() {
	mapName[64] = 0;
	if (GetPrivateProfileString("Misc", "StartingMap", "", mapName, 64, ini)) {
		dlog("Applying starting map patch.", DL_INIT);
		SafeWrite32(0x00480AAA, (DWORD)&mapName);
		dlogr(" Done", DL_INIT);
	}

	versionString[64] = 0;
	if (GetPrivateProfileString("Misc", "VersionString", "", versionString, 64, ini)) {
		dlog("Applying version string patch.", DL_INIT);
		SafeWrite32(0x004B4588, (DWORD)&versionString);
		dlogr(" Done", DL_INIT);
	}

	configName[64] = 0;
	if (GetPrivateProfileString("Misc", "ConfigFile", "", configName, 64, ini)) {
		dlog("Applying config file patch.", DL_INIT);
		SafeWrite32(0x00444BA5, (DWORD)&configName);
		SafeWrite32(0x00444BCA, (DWORD)&configName);
		dlogr(" Done", DL_INIT);
	}

	patchName[64] = 0;
	if (GetPrivateProfileString("Misc", "PatchFile", "", patchName, 64, ini)) {
		dlog("Applying patch file patch.", DL_INIT);
		SafeWrite32(0x00444323, (DWORD)&patchName);
		dlogr(" Done", DL_INIT);
	}

	ApplyInputPatch();
	ApplyCombatProcFix();
	ApplyDebugModePatch();
	ApplyNPCAutoLevelPatch();
	ApplyDialogueFix();
	ApplyDontDeleteProtosPatch();
	ApplyAdditionalWeaponAnimsPatch();
	ApplyMultiPatchesPatch();
	ApplyAlwaysReloadMsgs();
	ApplyPlayIdleAnimOnReloadPatch();
	ApplyCorpseLineOfFireFix();
	SkipOpeningMoviesPatch();

	ApplyNpcExtraApPatch();

	ApplySkilldexImagesPatch();
	RemoveWindowRoundingPatch();
	
	ApplySpeedInterfaceCounterAnimsPatch();
	ApplyScienceOnCrittersPatch();
	InventoryCharacterRotationSpeedPatch();

	dlogr("Patching out ereg call.", DL_INIT);
	BlockCall(0x4425E6);

	ApplyOverrideMusicDirPatch();
	ApplyNpcStage6Fix();
	ApplyFashShotTraitFix();
	ApplyBoostScriptDialogLimitPatch();
	ApplyMotionScannerFlagsPatch();
	ApplyEncounterTableSizePatch();

	if (GetPrivateProfileIntA("Misc", "DisablePipboyAlarm", 0, ini)) {
		SafeWrite8(0x499518, 0xc3);
	}

	ApplyObjCanSeeShootThroughPatch();
	UIAnimationSpeedPatch();
	MusicInDialoguePatch();

	ApplyInstantWeaponEquipPatch();
	ApplyNumbersInDialoguePatch();
}

void MiscPatches::exit() {
	if (scriptDialog != nullptr) {
		delete[] scriptDialog;
	}
}