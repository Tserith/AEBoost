#pragma once
#include <AE.h>

// this must be used in the first 0x40 bytes of the hook function
#define FIXUP_VALUE 0xAAAAAAAA

#define DebugPrint(...) \
{ \
    char temp[1000]; \
    ModCtx->snprintf(temp, sizeof(temp), __VA_ARGS__); \
    ModCtx->debugPrint(temp); \
}

#define CONTINUE_IF(condition) if (condition) {continue;}
#define BREAK_IF(condition) if (condition) {break;}
#define GOTO_FAIL_IF(condition) if (condition) {goto fail;}

typedef int(__cdecl* SNPRINTF)(char*, size_t, const char*, ...);
typedef void(__stdcall* DEBUG_PRINT)(const char*);
typedef u32(__stdcall* GET_TIME)();
typedef u16(__stdcall* GET_KEY_STATE)(int Key);

struct ModContext
{
    AeContext ae;
    SNPRINTF snprintf;
    DEBUG_PRINT debugPrint;
    GET_TIME getTimeMs;
    GET_KEY_STATE getAsyncKeyState;
    bool haveSaidHello;
};

// default code to the injection area
#pragma code_seg(".text$y")
// runtime checks are not PIC
#pragma runtime_checks("", off )
// speed optimizations can break the caller of hooked code
#pragma optimize("", off)

#define END_INJECT_CODE __pragma(code_seg(".text$a")); __pragma(optimize("", on))

typedef void(*MOD_KEY_CALLBACK)(ModContext* ModCtx, int Key);
typedef void(*MOD_SEND_HOOK)(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet);
typedef void(*MOD_RECV_HOOK)(ModContext* ModCtx, NetContext* NetCtx, ServerPacket* Packet);

void AllocContext(void* Context, uint16_t ContextSize, u32* ContextAddr);
void InlineHook(u8* Pattern, u8 PatternLen, void* Hook, u32 FixupValue);
void RegisterForSendHook(MOD_SEND_HOOK Hook, u32 FixupValue);
void RegisterForRecvHook(MOD_RECV_HOOK Hook, u32 FixupValue);
void RegisterForKeyCallback(MOD_KEY_CALLBACK Callback, int Key, u32 FixupValue);