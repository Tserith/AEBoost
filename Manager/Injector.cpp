#include <WMI.h>
#include <Injector.h>
#include <Mods.h>
#include <windows.h>
#include <tlhelp32.h>
#pragma warning( disable : 4530)
#include <vector>
#undef DrawTextEx
#undef DrawText
namespace raylib
{
    extern "C"
    {
#include <raylib.h>
#define RAYGUI_IMPLEMENTATION
#include <raygui.h>
    }
}

using namespace std;

// declare x and z as code sections so that the linker
// does not give them their own .text section
#pragma code_seg(".text$x")
#pragma code_seg(".text$z")

// mark start and end of code to inject
__declspec(allocate(".text$x")) u8 CodeInjectStart = 0;
__declspec(allocate(".text$z")) u8 CodeInjectEnd = 0;

const u32 CodeInjectSize = &CodeInjectEnd - &CodeInjectStart;

// the code in this file should not be injected
END_INJECT_CODE;

#define EXPAND2(x) #x
#define EXPAND(x) EXPAND2(x)
#define UNEXPECTED_ERROR g_ErrorMessage = "Unexpected error on line " EXPAND(__LINE__) " \nin " __FUNCTION__

HANDLE g_Process = INVALID_HANDLE_VALUE;
MODULEENTRY32W g_TargetModule = {0};
u8* g_InjectedCode = nullptr;
char* g_ErrorMessage = nullptr;

bool g_Mods[] = {
#define MOD(Name) false, 
        #include <ModList.txt>
#undef MOD
};

// remember to close handle if not null (mod not boosting)
static HANDLE CheckModNotEnabled(int ModId)
{
    char temp[100];
    HANDLE mutex = nullptr;

    snprintf(temp, sizeof(temp), "aeb%i", ModId);

    mutex = CreateMutex(nullptr, false, temp);

    if (nullptr != mutex && ERROR_ALREADY_EXISTS == GetLastError())
    {
        CloseHandle(mutex);
        mutex = nullptr;
    }

    return mutex;
}

bool MarkModEnabled(int ModId)
{
    HANDLE mutex = CheckModNotEnabled(ModId);

    if (nullptr != mutex)
    {
        DuplicateHandle(GetCurrentProcess(), mutex, g_Process, nullptr, 0, false, DUPLICATE_SAME_ACCESS);
        CloseHandle(mutex);
        return false;
    }

    return true;
}

static u32 DoInjection(LONG Pid, wchar_t* Module, bool* Mods)
{
    HANDLE snapshot;
    MODULEENTRY32W module;
    SIZE_T bytesWritten = 0;
    bool modsToEnable = false;
    HANDLE mutex = nullptr;
    u32 i = 0;

#define MOD(Name) if (g_Mods[i] && (mutex = CheckModNotEnabled(i))) {CloseHandle(mutex); modsToEnable = true;} i++;
#include <ModList.txt>
#undef MOD
    if (!modsToEnable)
    {
        return 0;
    }

    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, Pid);
    if (INVALID_HANDLE_VALUE != snapshot)
    {
        module.dwSize = sizeof(module);

        Module32FirstW(snapshot, &module);

        do
        {
            if (!wcscmp(module.szModule, Module))
            {
                memcpy(&g_TargetModule, &module, sizeof(module));
                break;
            }
        } while (Module32NextW(snapshot, &module));

        CloseHandle(snapshot);
    }
    else
    {
        UNEXPECTED_ERROR;
    }

    if (0 == g_TargetModule.modBaseSize)
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    auto folderIndex = wcslen(g_TargetModule.szExePath) - (sizeof(L"AshenEmpires.exe") / sizeof(wchar_t));
    g_TargetModule.szExePath[folderIndex + 1] = L'\0';

    if (wcscat_s(
        g_TargetModule.szExePath,
        sizeof(g_TargetModule.szExePath) / sizeof(wchar_t),
        L"build-number.txt"))
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    auto f = _wfopen(g_TargetModule.szExePath, L"r");
    if (nullptr == f)
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    if (fseek(f, 16, SEEK_SET))
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    char buildNumber[4] = { '\0' };
    if (3 != fread(buildNumber, 1, 3, f))
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    if (AE_BUILD != atoi(buildNumber))
    {
        g_ErrorMessage = "This version of AEBoost \nis out of date";
        return -1;
    }

    g_Process = OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | PROCESS_DUP_HANDLE, FALSE, Pid);
    if (INVALID_HANDLE_VALUE == g_Process)
    {
        UNEXPECTED_ERROR;
        return -1;
    }

    g_InjectedCode = (u8*)VirtualAllocEx(g_Process, nullptr, CodeInjectSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!g_InjectedCode)
    {
        UNEXPECTED_ERROR;
        goto fail;
    }

    if (!WriteProcessMemory(g_Process, g_InjectedCode, &CodeInjectStart, CodeInjectSize, &bytesWritten) || bytesWritten != CodeInjectSize)
    {
        VirtualFreeEx(g_Process, g_InjectedCode, 0, MEM_RELEASE);
        UNEXPECTED_ERROR;
        goto fail;
    }

    if (!FlushInstructionCache(g_Process, g_InjectedCode, CodeInjectSize))
    {
        UNEXPECTED_ERROR;
        goto fail;
    }

    InitMods(Mods);

    return 0;
fail:
    CloseHandle(g_Process);
    return -2;
}

void* GetRemoteCodeAddr(void* Func)
{
    return g_InjectedCode + ((uint8_t*)Func - &CodeInjectStart);
}

void InitModContext(ModContext* Context)
{
    Context->ae.inventory = GLOBAL_INVENTORY;
    Context->ae.spells = GLOBAL_SPELLS;
    Context->ae.items = GLOBAL_ITEMS;
    Context->ae.gPtr = GLOBAL_PTR;
    Context->ae.getObjectFromId = GLOBAL_GET_OBJECT;
    Context->snprintf = (SNPRINTF)GetProcAddress(GetModuleHandleA("ntdll.dll"), "_snprintf");
    Context->debugPrint = (DEBUG_PRINT)GetProcAddress(GetModuleHandleA("kernel32.dll"), "OutputDebugStringA");
    Context->getTimeMs = (GET_TIME)GetProcAddress(GetModuleHandleA("kernel32.dll"), "GetTickCount");
    Context->getAsyncKeyState = (GET_KEY_STATE)GetProcAddress(GetModuleHandleA("user32.dll"), "GetAsyncKeyState");
    Context->haveSaidHello = false;
}

void AllocContext(void* Context, uint16_t ContextSize, u32* ContextAddr)
{
    void* remoteBuffer = nullptr;
    SIZE_T bytesWritten = 0;

    remoteBuffer = VirtualAllocEx(g_Process, nullptr, ContextSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remoteBuffer)
    {
        UNEXPECTED_ERROR;
        return;
    }

    if (!WriteProcessMemory(g_Process, remoteBuffer, Context, ContextSize, &bytesWritten) || bytesWritten != ContextSize)
    {
        UNEXPECTED_ERROR;
        VirtualFreeEx(g_Process, remoteBuffer, 0, MEM_RELEASE);
        return;
    }

    *ContextAddr = (u32)remoteBuffer;
}

void FixupCode(void* Hook, u32 FixupValue)
{
    for (u16 i = 0; i < 0x40; i++)
    {
        if (FIXUP_VALUE == *(uint32_t*)((u8*)Hook + i))
        {
            WriteProcessMemory(g_Process, (u8*)GetRemoteCodeAddr(Hook) + i, &FixupValue, 4, NULL);
            FlushInstructionCache(g_Process, (u8*)GetRemoteCodeAddr(Hook) + i, 4);

            break;
        }
    }
}

void HookIat(char* IatModule, uint8_t IatEntry, void* Hook, uint32_t* HookedFunc, uint32_t FixupValue)
{
    void* remoteHookFuncAddr = GetRemoteCodeAddr(Hook);
	vector<u8> copyBuf;
	PIMAGE_NT_HEADERS32 ntHeader;
	PIMAGE_OPTIONAL_HEADER32 opHeader;
	IMAGE_DATA_DIRECTORY itable;

    // do fixups
    FixupCode(Hook, FixupValue);

	copyBuf.resize(g_TargetModule.modBaseSize);

	if (ReadProcessMemory(g_Process, g_TargetModule.modBaseAddr, copyBuf.data(), copyBuf.size(), NULL))
	{
		ntHeader = (PIMAGE_NT_HEADERS32)(copyBuf.data() + ((PIMAGE_DOS_HEADER)copyBuf.data())->e_lfanew);
		opHeader = &ntHeader->OptionalHeader;
		itable = opHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

		if (itable.Size)
		{
			for (auto i = (PIMAGE_IMPORT_DESCRIPTOR)(copyBuf.data() + itable.VirtualAddress); i->Name; i++)
			{
                CONTINUE_IF(strcmp((char*)copyBuf.data() + i->Name, IatModule));
				
                auto iatFunc = &((uint32_t*)(copyBuf.data() + i->FirstThunk))[IatEntry - 1];
                auto remoteAddr = (uint8_t*)iatFunc - copyBuf.data() + g_TargetModule.modBaseAddr;
                DWORD oldProtect;

                // write hooked func addr
                VirtualProtectEx(g_Process, HookedFunc, 4, PAGE_READWRITE, &oldProtect);
                WriteProcessMemory(g_Process, HookedFunc, iatFunc, sizeof(iatFunc), NULL);
                VirtualProtectEx(g_Process, HookedFunc, 4, oldProtect, &oldProtect);

                // write hook
                VirtualProtectEx(g_Process, remoteAddr, 4, PAGE_READWRITE, &oldProtect);
                WriteProcessMemory(g_Process, remoteAddr, &remoteHookFuncAddr, 4, NULL);
                VirtualProtectEx(g_Process, remoteAddr, 4, oldProtect, &oldProtect);

                break;
			}
		}
	}
}

void InlineHook(u8* Pattern, u8 PatternLen, void* Hook, u32 FixupValue)
{
    void* remoteHookFuncAddr = GetRemoteCodeAddr(Hook);
    uint32_t i = 0;
    vector<uint8_t> copyBuf;
    DWORD oldProtect;
    LPVOID funcToHook = 0;
    uint8_t jmp[5];

    // do fixups
    FixupCode(Hook, FixupValue);

    copyBuf.resize(g_TargetModule.modBaseSize);

    if (ReadProcessMemory(g_Process, g_TargetModule.modBaseAddr, copyBuf.data(), copyBuf.size(), NULL))
    {
        for (i = 0; i < copyBuf.size() - PatternLen; i++)
        {
            if (!memcmp(copyBuf.data() + i, Pattern, PatternLen))
            {
                funcToHook = g_TargetModule.modBaseAddr + i;

                break;
            }
        }
    }

    if (funcToHook)
    {
        VirtualProtectEx(g_Process, funcToHook, sizeof(jmp), PAGE_EXECUTE_READWRITE, &oldProtect);

        *jmp = 0xE9; // JMP
        *(uint32_t*)(jmp + 1) = (uint32_t)remoteHookFuncAddr - (uint32_t)funcToHook - sizeof(jmp);

        // copy hook
        WriteProcessMemory(g_Process, funcToHook, jmp, sizeof(jmp), NULL);
        VirtualProtectEx(g_Process, funcToHook, sizeof(jmp), oldProtect, &oldProtect);

        if (!FlushInstructionCache(g_Process, funcToHook, sizeof(jmp)))
        {
            UNEXPECTED_ERROR;
        }
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR lpCmdLine, int nCmdShow)
{
    COM com;
    WMI wmi(com);
    ProcessCreation newProcesses(wmi);
    HANDLE mutex;
    LONG pid;
    HANDLE snapshot;
    PROCESSENTRY32W process;

    const int windowWidth = 300;
    const int windowHeight = 50 + ((sizeof(g_Mods) + 1) / 2) * 80;
    const char* windowName = "AE Boost";

    // don't allow two open at a time
    mutex = CreateMutex(nullptr, false, "AEBoost");
    if (nullptr != mutex && ERROR_ALREADY_EXISTS == GetLastError())
    {
        CloseHandle(mutex);
        return 0;
    }

    // which mods are currently enabled?
    for (u16 i = 0; i < sizeof(g_Mods); i++)
    {
        auto mutex = CheckModNotEnabled(i);
        if (mutex)
        {
            CloseHandle(mutex);
        }
        else
        {
            g_Mods[i] = true;
        }
    }

    raylib::SetConfigFlags(raylib::FLAG_WINDOW_UNDECORATED);
    raylib::InitWindow(windowWidth, windowHeight, windowName);

    raylib::GuiSetStyle(raylib::STATUSBAR, raylib::BASE_COLOR_NORMAL, raylib::ColorToInt(raylib::BLUE));
    raylib::GuiSetStyle(raylib::STATUSBAR, raylib::TEXT_COLOR_NORMAL, raylib::ColorToInt(raylib::BLACK));

    HICON hIcon = LoadIcon(hInstance, "IDI_ICON1");
    SendMessage((HWND)raylib::GetWindowHandle(), WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    SendMessage((HWND)raylib::GetWindowHandle(), WM_SETICON, ICON_BIG, (LPARAM)hIcon);

    // General variables
    raylib::Vector2 mousePosition = { 0 };
    raylib::Vector2 windowPosition = { 800, 400 };
    raylib::Vector2 panOffset = mousePosition;
    bool dragWindow = false;
    bool exitWindow = false;

    raylib::SetWindowPosition(windowPosition.x, windowPosition.y);
    raylib::SetTargetFPS(60);

    bool boosting = false;

    while (!exitWindow && !raylib::WindowShouldClose())
    {
        // update
        mousePosition = raylib::GetMousePosition();
        
        if (raylib::IsMouseButtonPressed(raylib::MOUSE_LEFT_BUTTON))
        {
            if (raylib::CheckCollisionPointRec(mousePosition, {0, 0, windowWidth, 20}))
            {
                dragWindow = true;
                panOffset = mousePosition;
            }
        }

        if (dragWindow)
        {
            windowPosition.x += (mousePosition.x - panOffset.x);
            windowPosition.y += (mousePosition.y - panOffset.y);
            
            if (raylib::IsMouseButtonReleased(raylib::MOUSE_LEFT_BUTTON)) dragWindow = false;

            raylib::SetWindowPosition(windowPosition.x, windowPosition.y);
        }

        // draw
        raylib::BeginDrawing();

        raylib::ClearBackground(raylib::RAYWHITE);

        exitWindow = raylib::GuiWindowBox({0,0,windowWidth,windowHeight}, windowName);

        raylib::DrawText("Created by Inev", windowWidth - 87, windowHeight - 12, 10, raylib::MAGENTA);

        int i = 0;
        HANDLE mutex = nullptr;
        bool temp;
#define MOD(Name) temp = raylib::GuiCheckBox({ (i%2==1) * 135.0f + 35, (i / 2) * 50.0f + 45, 35, 35 }, ""#Name, g_Mods[i]); \
if (!boosting && (temp || (mutex = CheckModNotEnabled(i)))) \
    { if (mutex) CloseHandle(mutex); g_Mods[i] = temp; }i++; mutex = nullptr;
         #include <ModList.txt>
#undef MOD

        auto toggleText = "";

        if (boosting)
        {
            toggleText = "Pause";
        }
        else
        {
            toggleText = "Boost";
        }
        
        auto wasBoosting = boosting;
        boosting = raylib::GuiToggle({80,((i/2) + (i%2))*40.0f + 70,140,40}, toggleText, wasBoosting);

        if (!wasBoosting && boosting)
        {
            // inject into existing clients
            snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (INVALID_HANDLE_VALUE != snapshot)
            {
                process.dwSize = sizeof(process);

                Process32FirstW(snapshot, &process);

                do
                {
                    if (!wcscmp(process.szExeFile, TARGET_IMAGE_NAME))
                    {
                        DoInjection(process.th32ProcessID, TARGET_IMAGE_NAME, g_Mods);
                    }
                } while (Process32NextW(snapshot, &process));

                CloseHandle(snapshot);
            }
        }

        // inject into future clients
        if (boosting)
        {
            pid = newProcesses.CheckForImageName(TARGET_IMAGE_NAME);
            if (-1 != pid)
            {
                DoInjection(pid, TARGET_IMAGE_NAME, g_Mods);
            }
        }

        if (g_ErrorMessage)
        {
            boosting = false;

            if (-1 != raylib::GuiMessageBox({ windowWidth / 2 - 100, windowHeight / 2 - 50, 200, 100 }, "Error", g_ErrorMessage, "Ok"))
            {
                g_ErrorMessage = nullptr;
            }
        }

        raylib::EndDrawing();
    }

    raylib::CloseWindow();

    return 0;
}