#include "skse64/PluginAPI.h"
#include "skse64_common/skse_version.h"
#include "skse64_common/Relocation.h"
#include "skse64_common/SafeWrite.h"
#include "skse64_common/BranchTrampoline.h"
#include "skse64/GameReferences.h"
#include "skse64/GameInput.h"
#include "skse64/NiExtraData.h"
#include "xbyak/xbyak.h"
#include <shlobj.h>
#include <time.h>
#include <Psapi.h>
#include "PatternScanner.h"

#pragma comment(lib, "Winmm.lib")
#pragma comment(lib, "Psapi.lib")

IDebugLog				gLog;
PluginHandle			g_pluginHandle = kPluginHandle_Invalid;
void					* g_moduleHandle = nullptr;

uintptr_t g_playerAddr = 0;

MMRESULT g_timerID = 0;
UINT g_resolution = 30;
INT32 g_timer = 0;
double g_step = 3;
bool g_run = false;

void MainGetAddresses()
{
	const std::array<BYTE, 8> pattern = { 0x8B, 0x40, 0x10, 0xC1, 0xE8, 0x17, 0xA8, 0x01 };
	g_playerAddr = (uintptr_t)scan_memory_data(pattern, 0x23, true, 0x3, 0x7);
}

void UpdateExpression(PlayerCharacter * player, float value)
{
	BSFaceGenAnimationData * faceAnimData = nullptr;
	faceAnimData = player->GetFaceGenAnimationData();
	if (faceAnimData)
	{
		BSFaceGenKeyframeMultiple * keyframe = nullptr;
		keyframe = &faceAnimData->keyFrames[7];
		if (keyframe && keyframe->count == 17)
		{
			keyframe->values[0] = value;
			keyframe->values[1] = value;
		}
	}
}

void PlayerBlink(UINT uID)
{
	PlayerCharacter * player = *(PlayerCharacter**)g_playerAddr;
	if (player && player->loadedState)
	{
		INT32 timer = g_timer;
		timer--;
		g_timer = timer;
		if (timer < 0)
		{
			timer = ((rand() % 50) * 4) + 100;
			g_timer = timer;
		}
		if (timer >= 0 && timer < 5)
		{
			float time = g_timer;
			if (timer <= 3)
			{
				float value = time / g_step;

				UpdateExpression(player, value);
			}
			else
			{
				float step = g_step;
				float value = 1 - ((time - step) / step);

				UpdateExpression(player, value);
			}
		}
	}
}

void CALLBACK TimeProc(
	UINT uID, UINT uMsg, DWORD_PTR dwUser,
	DWORD_PTR dw1, DWORD_PTR dw2)
{
	PlayerBlink(uID);
}

bool Start()
{
	bool ret = false;
	if (g_timerID == 0)
	{
		UINT delay = 30;
		timeBeginPeriod(g_resolution);
		g_timerID = timeSetEvent(delay, g_resolution, TimeProc, 0, TIME_PERIODIC);
		if (g_timerID != 0)
			ret = true;
	}

	return ret;
}

bool Stop()
{
	bool ret = false;
	if (g_timerID != 0)
	{
		if (timeKillEvent(g_timerID) == TIMERR_NOERROR)
		{
			g_timerID = 0;
			ret = true;
		}
		timeEndPeriod(g_resolution);
	}
		
	PlayerCharacter * player = *(PlayerCharacter**)g_playerAddr;
	if (player && player->loadedState)
		UpdateExpression(player, 0);

	return ret;
}

extern "C"
{

	bool SKSEPlugin_Query(const SKSEInterface * skse, PluginInfo * info)
	{

		gLog.OpenRelative(CSIDL_MYDOCUMENTS, "\\My Games\\Skyrim Special Edition\\SKSE\\PlayerBlink.log");

		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = "Player Blink plugin";
		info->version = 1;

		g_pluginHandle = skse->GetPluginHandle();

		if (skse->isEditor)
		{
			_MESSAGE("loaded in editor, marking as incompatible");
			return false;
		}

		return true;
	}

	bool SKSEPlugin_Load(const SKSEInterface * skse)
	{
		_MESSAGE("Load");

		MainGetAddresses();

		srand(time(0));
		Start();

		return true;
	}

}