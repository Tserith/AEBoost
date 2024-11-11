#pragma once
#include <AE.h>

#define TARGET_IMAGE_NAME L"AshenEmpires.exe"

extern void InitMods(bool*, u32, bool);

struct ModContext;

void* GetRemoteCodeAddr(void* Func);
void FixupCode(void* Hook, u32 FixupValue);
void InitModContext(ModContext* Context);
void AllocContext(void* Context, uint16_t ContextSize, u32* ContextAddr);
void HookIat(char* IatModule, uint8_t IatEntry, void* Hook, uint32_t* HookedFunc, uint32_t FixupValue);
void InlineHook(u8* Pattern, u8 PatternLen, void* Hook, u32 FixupValue);
bool CheckModNotEnabled(u32 Pid, int ModId);