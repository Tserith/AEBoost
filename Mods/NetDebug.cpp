#include <Mods.h>

// this mod prints all networking traffic to the debugger
// the game will crash if this is enabled while selecting a server if a debugger is not attached

static void NetDebugSend(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    char sizeFormat[] =
    {
        '[', 'S', 'E', 'N', 'D', ']', ' ', 'S', 'i', 'z', 'e', ':', ' ', '0', 'x', '%', '0', '2', 'x', ' ', '|', ' ', '\0'
    };
    char byteFormat[] = { '%', '0', '2', 'x', ' ', '\0' };
    char newlineFormat[] = { '\n', '\0' };
    
    DebugPrint(sizeFormat, Packet->header.size);

    for (int i = 0; i < Packet->header.size; i++)
    {
        DebugPrint(byteFormat, ((unsigned char*)Packet + sizeof(AeHeader))[i]);
    }

    DebugPrint(newlineFormat);
}

static void NetDebugRecv(ModContext* ModCtx, NetContext* NetCtx, ServerPacket* Packet)
{
    char sizeFormat[] =
    {
        '[', 'R', 'E', 'C', 'V', ']', ' ', 'S', 'i', 'z', 'e', ':', ' ', '0', 'x', '%', '0', '2', 'x', ' ', '|', ' ', '\0'
    };
    char byteFormat[] = { '%', '0', '2', 'x', ' ', '\0' };
    char newlineFormat[] = { '\n', '\0' };
    
    DebugPrint(sizeFormat, Packet->size);

    for (int i = 0; i < Packet->size; i++)
    {
        DebugPrint(byteFormat, Packet->data[i]);
    }

    DebugPrint(newlineFormat);
}

END_INJECT_CODE;

void InitNetDebug()
{
    RegisterForSendHook(NetDebugSend, 0);
    RegisterForRecvHook(NetDebugRecv, 0);
}