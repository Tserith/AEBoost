#include <Mods.h>

static void TotemPetAssist(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    u32 petId = 0;

    if (Packet->header.size >= 4 && Packet->body.action == 0x5b) // what even is this?
    {
        memcpy(&petId, (u8*)&Packet->body + 1, 3);
        SetPetMode(&ModCtx->ae, NetCtx, PetMode::Assist, petId);

        if (Packet->header.size >= 8 && *((u8*)&Packet->body.action + 4) == 0x5b)
        {
            memcpy(&petId, (u8*)&Packet->body + 5, 3);
            SetPetMode(&ModCtx->ae, NetCtx, PetMode::Assist, petId);
        }
    }
}


END_INJECT_CODE;

void InitTotemPetAssist()
{
    RegisterForSendHook(TotemPetAssist, 0);
}