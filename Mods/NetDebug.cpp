#include <Mods.h>

// this mod prints all networking traffic to the debugger
// the game may crash if this is enabled while selecting a server if a debugger is not attached

// note that enabling this mod last will prevent it from displaying sends from other mods

static void NetDebugSend(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    char sendFormat[] =
    {
        '[', 'S', 'E', 'N', 'D', ']', ' ', 'S', 'i', 'z', 'e', ':', ' ', '0', 'x', '%', '0', '2', 'x', ' ',
        '|', ' ', '[', '0', 'x', '%', '0', '2', 'x', ']',  ' ', '\0'
    };
    char byteFormat[] = { '%', '0', '2', 'x', ' ', '\0' };
    char newlineFormat[] = { '\n', '\0' };
    
    DebugPrint(sendFormat, Packet->header.size, Packet->body.action);

    // send action is in the body while recv action is in header
    for (int i = 1; i < Packet->header.size; i++)
    {
        DebugPrint(byteFormat, ((unsigned char*)Packet + sizeof(AeHeader))[i]);
    }

    DebugPrint(newlineFormat);
}

static void NetDebugRecv(ModContext* ModCtx, NetContext* NetCtx, ServerPacket* Packet)
{
    char recvFormat[] =
    {
        '[', 'R', 'E', 'C', 'V', ']', ' ', 'S', 'i', 'z', 'e', ':', ' ', '0', 'x', '%', '0', '2', 'x', ' ',
        '|', ' ', '[', '0', 'x', '%', '0', '2', 'x', ']',  ' ', '\0'
    };
    char byteFormat[] = { '%', '0', '2', 'x', ' ', '\0' };
    char newlineFormat[] = { '\n', '\0' };
    
    DebugPrint(recvFormat, Packet->size, Packet->action);

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