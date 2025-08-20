#include <Mods.h>
#include <Injector.h>
#include <stddef.h>

#pragma warning( disable : 6260)
#define MAX_HANDLER_SIZE(HandlerType) (sizeof(HandlerType) + ((1 << 8 * sizeof(HandlerType::count)) * sizeof(HandlerType::handlers[0])))

#define MSG_PEEK	2

struct KeyHandlers
{
    MOD_KEY_CALLBACK callback;
    int key;
    u32 lastCalled;
};

struct KeyCallbackContext
{
    u8 count;
    KeyHandlers handlers[0];
};

struct NetHandlers
{
    union
    {
        MOD_SEND_HOOK sendHandler;
        MOD_RECV_HOOK recvHandler;
    };
    bool sendNotRecv;
};

struct NetHookContext
{
    ModContext mod;
    KeyCallbackContext* key;
    NetContext net;
    u8 recvBuf[0x1000];
    bool recvSpoof;
    u8 count;
    NetHandlers handlers[0];
};

static void CheckKeys(ModContext* Mod, KeyCallbackContext* Key)
{
    void* window = Mod->getActive();

    for (u8 i = 0; i < Key->count; i++)
    {
        u32 time = Mod->getTimeMs();

        CONTINUE_IF(!Mod->getAsyncKeyState(Key->handlers[i].key));
        CONTINUE_IF(Mod->getForeground() != window);
        CONTINUE_IF(time < Key->handlers[i].lastCalled + 250);

        Key->handlers[i].lastCalled = time;
        Key->handlers[i].callback(Mod, Key->handlers[i].key);
    }

    // also print hello message if we haven't
    if (!Mod->haveSaidHello)
    {
        char hello[] = {'M', 'o', 'd', 's', ' ', 'I', 'n', 'i', 't', 'i', 'a', 'l', 'i', 'z', 'e', 'd', '.', '\0'};
        Mod->haveSaidHello = WriteToChatWindow(hello);
    }
}

static int __stdcall HookedSend(u32 s, char* buf, int len, int flags)
{
    auto volatile ctx = (NetHookContext*)FIXUP_VALUE;
    auto thisPtr = &(*ctx->mod.ae.gPtr)[GLOBAL_PTR_KEY_OFFSET];
    auto packet = (ClientPacket*)buf;
    auto castSpell = packet;
    ContainerObject* rune = NULL;
    bool cancelSend = false;
    
    if (0 == *GLOBAL_MANAGER)
    {
        // a character isn't logged in!
        return ctx->net.send(s, buf, len, flags);
    }

    CheckKeys(&ctx->mod, ctx->key);

    // ignore this packet if we our cached key isn't correct
    // this may happen on the first send we read (our cache is null) or when switching servers (key change?)
    if (len == sizeof(ClientPacket::header) + packet->header.size && UndoXorBuffer(&ctx->mod.ae, &ctx->net, packet))
    {
        for (u8 i = 0; i < ctx->count; i++)
        {
            CONTINUE_IF(!ctx->handlers[i].sendNotRecv);

            ctx->handlers[i].sendHandler(&ctx->mod, &ctx->net, packet);
            cancelSend = !packet->header.size;

            BREAK_IF(cancelSend);
        }

        if (!cancelSend)
        {
            packet->header.checksum = CalcSendChecksum(packet);
            XorBuffer(&ctx->mod.ae, &ctx->net, packet, false);
        }
    }

    if (!cancelSend)
    {
        len = ctx->net.send(s, buf, sizeof(ClientPacket::header) + packet->header.size, flags);
    }

    // save the xor key so we can read the next send
    ctx->net.lastSendKey = *(int32_t*)(thisPtr + ctx->net.xorKeyOffset);

    return len;
}

static int __stdcall HookedRecv(u32 s, char* buf, int len, int flags)
{
    auto volatile ctx = (NetHookContext*)FIXUP_VALUE;
    bool cancelRecv = false;
    int ret = 0;
    int dataRet;

    if (0 == *GLOBAL_MANAGER)
    {
        // a character isn't logged in!
        return ctx->net.recv(s, buf, len, flags);
    }

    if (ctx->recvSpoof && MSG_PEEK != flags)
    {
        memcpy(buf, ctx->recvBuf, len);
        ctx->recvSpoof = false;
        ret = len;
    }
    else
    {
        do
        {
            CheckKeys(&ctx->mod, ctx->key);

            cancelRecv = false;

            ret = ctx->net.recv(s, buf, len, flags);

            BREAK_IF(len != ret || MSG_PEEK == flags);

            auto header = (AeHeader*)buf;

            if (len == 4 && header->size && header->size < sizeof(ctx->recvBuf))
            {
                u16 bytesRead = 0;

                dataRet = ctx->net.recv(s, (char*)ctx->recvBuf, header->size, flags);

                if (-1 != dataRet)
                {
                    while (bytesRead < header->size)
                    {
                        auto packet = (ServerPacket*)&ctx->recvBuf[bytesRead];
                        auto originalPacketSize = packet->size;
                        bool cancelPacket = false;

                        for (u8 i = 0; i < ctx->count; i++)
                        {
                            CONTINUE_IF(ctx->handlers[i].sendNotRecv);

                            ctx->handlers[i].recvHandler(&ctx->mod, &ctx->net, packet);
                            if (!packet->size)
                            {
                                cancelPacket = true;
                                header->size -= originalPacketSize;

                                // safe with same buffer because destination is before source (see memmove)
                                memcpy(&ctx->recvBuf[bytesRead], &ctx->recvBuf[bytesRead + originalPacketSize], header->size - bytesRead);
                                break;
                            }
                        }

                        if (!cancelPacket) // do not touch packet pointer since we may have changed the data!
                        {
                            bytesRead += originalPacketSize;
                        }
                    }

                    if (header->size)
                    {
                        ctx->recvSpoof = true;

                        // fix checksum in case we dropped a packet
                        header->checksum = 0;
                        for (u16 i = 0; i < header->size; i++)
                        {
                            header->checksum += ctx->recvBuf[i];
                        }
                    }
                    else
                    {
                        cancelRecv = true;
                    }
                }
            }
        } while (cancelRecv); // recv is blocking so just call recv again if we cancel it
    }

    return ret;
}

END_INJECT_CODE;

#define MOD(Name) extern void Init ## Name ();
#include <ModList.txt>
#undef MOD

static NetHookContext* g_NetHookContext = nullptr;
static KeyCallbackContext* g_KeyCallbackContext = nullptr;

void InitMods(bool* Mods, u32 Pid, bool AlwaysFirstInject)
{
    u32 netContextRemoteAddr = 0;
    u32 keyContextRemoteAddr = 0;

    g_NetHookContext = (NetHookContext*)malloc(MAX_HANDLER_SIZE(NetHookContext));
    GOTO_FAIL_IF(nullptr == g_NetHookContext);
    memset(g_NetHookContext, 0, MAX_HANDLER_SIZE(NetHookContext));

    g_KeyCallbackContext = (KeyCallbackContext*)malloc(MAX_HANDLER_SIZE(KeyCallbackContext));
    GOTO_FAIL_IF(nullptr == g_KeyCallbackContext);
    memset(g_KeyCallbackContext, 0, MAX_HANDLER_SIZE(KeyCallbackContext));

    InitModContext(&g_NetHookContext->mod);

    g_NetHookContext->net.xorKeyOffset = XOR_KEY_OFFSET;

    int i = 0;
#define MOD(Name) if (Mods[i] && (CheckModNotEnabled(Pid, i) || AlwaysFirstInject)) Init ## Name ## (); i++;
#include <ModList.txt>

    AllocContext(g_KeyCallbackContext, MAX_HANDLER_SIZE(KeyCallbackContext), &keyContextRemoteAddr);
    GOTO_FAIL_IF(0 == keyContextRemoteAddr);

    g_NetHookContext->key = (KeyCallbackContext*)keyContextRemoteAddr;

    AllocContext(g_NetHookContext, MAX_HANDLER_SIZE(NetHookContext), &netContextRemoteAddr);
    GOTO_FAIL_IF(0 == netContextRemoteAddr);

    HookIat("WSOCK32.dll", SEND_IAT_ENTRY, HookedSend, (u32*)&((NetHookContext*)netContextRemoteAddr)->net.send, netContextRemoteAddr);
    HookIat("WSOCK32.dll", RECV_IAT_ENTRY, HookedRecv, (u32*)&((NetHookContext*)netContextRemoteAddr)->net.recv, netContextRemoteAddr);

fail:
    free(g_NetHookContext);
    g_NetHookContext = nullptr;
    free(g_KeyCallbackContext);
    g_KeyCallbackContext = nullptr;
}

static void RegisterForNetHook(void* Hook, u32 FixupValue, bool SendNotRecv)
{
    if (SendNotRecv)
    {
        g_NetHookContext->handlers[g_NetHookContext->count].sendHandler = (MOD_SEND_HOOK)GetRemoteCodeAddr(Hook);
    }
    else
    {
        g_NetHookContext->handlers[g_NetHookContext->count].recvHandler = (MOD_RECV_HOOK)GetRemoteCodeAddr(Hook);
    }
    
    g_NetHookContext->handlers[g_NetHookContext->count].sendNotRecv = SendNotRecv;
    g_NetHookContext->count++;

    FixupCode(Hook, FixupValue);
}

void RegisterForSendHook(MOD_SEND_HOOK Hook, u32 FixupValue)
{
    RegisterForNetHook(Hook, FixupValue, true);
}

void RegisterForRecvHook(MOD_RECV_HOOK Hook, u32 FixupValue)
{
    RegisterForNetHook(Hook, FixupValue, false);
}

void RegisterForKeyCallback(MOD_KEY_CALLBACK Callback, int Key, u32 FixupValue)
{
    g_KeyCallbackContext->handlers[g_KeyCallbackContext->count].callback = (MOD_KEY_CALLBACK)GetRemoteCodeAddr(Callback);
    g_KeyCallbackContext->handlers[g_KeyCallbackContext->count].key = Key;
    g_KeyCallbackContext->count++;

    FixupCode(Callback, FixupValue);
}

// ideas:
// delay potion/totem usage until after next hit
// swap weapons before casting spell (spells always lit up)
// keybind to equip last weapon
// disable click to move
// keybinds
// swap to other gear set

// new / command for targetting players
// switch to best arrow type for target