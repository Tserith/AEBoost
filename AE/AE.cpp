#include <Mods.h>

ClientPacket::ClientPacket(PetMode Mode, uint32_t PetId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::SetPet);
    body.action = (uint8_t)Action::SetPetMode;

    body.SetPet.mode = (uint8_t)Mode;
    memcpy(&body.SetPet.petId, &PetId, sizeof(AeBody::SetPet.petId));

    body.SetPet.zero[0] = 0;
    body.SetPet.zero[1] = 0;
    body.SetPet.zero[2] = 0;
}
ClientPacket::ClientPacket(uint8_t RuneSlot, uint32_t RuneItemId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::ChangeRune);
    body.action = (uint8_t)Action::Magic;

    body.ChangeRune.magicType = (uint8_t)MagicType::ChangeRune;
    memcpy(&body.ChangeRune.runeItemId, &RuneItemId, sizeof(AeBody::ChangeRune.runeItemId));
    body.ChangeRune.runeSlot = RuneSlot;
}
ClientPacket::ClientPacket(uint32_t SpellId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::SelectSpell);
    body.action = (uint8_t)Action::Magic;

    body.SelectSpell.magicType = (uint8_t)MagicType::SelectSpell;
    body.SelectSpell.spellId = SpellId;
}
ClientPacket::ClientPacket(uint32_t SpellId, uint32_t TargetId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::CastSpell);
    body.action = (uint8_t)Action::Magic;

    body.CastSpell.magicType = (uint8_t)MagicType::CastSpell;
    body.CastSpell.spellId = SpellId;
    memcpy(&body.CastSpell.targetId, &TargetId, sizeof(AeBody::CastSpell.targetId));
    memset(&body.CastSpell.idk, 0, sizeof(AeBody::CastSpell.idk));
}

uint16_t CalcReceiveChecksum(uint8_t* Buffer, uint16_t Len)
{
    uint16_t checksum = 0;

    for (int i = 0; i < Len; i++)
    {
        checksum += Buffer[i];
    }
    return checksum;
}

void XorBuffer(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto buf = (uint8_t*)Packet + sizeof(AeHeader);
    auto thisPtr = &(*AeCtx->gPtr)[0x10];
    auto key = *(int32_t*)(thisPtr + NetCtx->xorKeyOffset);

    if (key)
    {
        for (int i = 0; i < Packet->header.size; i++)
        {
            uint32_t temp = key * 0x10003;
            int32_t temp2 = key * 0x10003 + 1;

            if (1 == *(int32_t*)(thisPtr + NetCtx->xorKeyOffset + 0x8))
            {
                key = temp - 1;

                if (temp == 3 * (temp / 3)) // ???
                {
                    key = temp2;
                }
            }
            else
            {
                key = key * 0x10003 + 1;
            }

            buf[i] ^= key >> 0x10;
        }

        // save key
        *(int32_t*)(thisPtr + NetCtx->xorKeyOffset) = key;
    }
}

void UndoXorBuffer(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto buf = (uint8_t*)Packet + sizeof(AeHeader);
    auto thisPtr = &(*AeCtx->gPtr)[0x10];
    auto key = NetCtx->lastSendKey;
    auto len = Packet->header.size;

    if (key)
    {
        if (!*(int8_t*)(thisPtr + NetCtx->xorKeyOffset + 0xC))
        {
            // 2 byte header

            len += 2;
            buf -= 2;
        }

        for (int i = 0; i < len; i++)
        {
            uint32_t temp = key * 0x10003;
            int32_t temp2 = key * 0x10003 + 1;

            if (1 == *(int32_t*)(thisPtr + NetCtx->xorKeyOffset + 0x8))
            {
                key = temp - 1;

                if (temp == 3 * (temp / 3)) // ???
                {
                    key = temp2;
                }
            }
            else
            {
                key = key * 0x10003 + 1;
            }

            buf[i] ^= key >> 0x10;
        }

        // save old key
        *(int32_t*)(thisPtr + NetCtx->xorKeyOffset) = NetCtx->lastSendKey;
    }
}

void CalcSendChecksum(ClientPacket* Packet)
{
    Packet->header.checksum = 0;

    if (Packet->header.size)
    {
        auto temp = (u8*)&Packet->body;

        for (int i = 0; i < Packet->header.size; i++)
        {
            Packet->header.checksum += temp[i];
        }
    }
}

void Send(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    CalcSendChecksum(Packet);
    XorBuffer(AeCtx, NetCtx, Packet);

    auto s = *(u32*)(&(*AeCtx->gPtr)[0x10] + NetCtx->xorKeyOffset - 0x30 + 0x10);
    if (s != -1)
    {
        auto result = NetCtx->send(s, (const char*)Packet, sizeof(AeHeader) + Packet->header.size, 0);
    }
}




struct string // std::string
{
    char buffer[16];
    u32 len;
    u32 maxLen;
};

// returns true if successful (if the chat manager is initialized)
bool WriteToChatWindow(char* Message)
{
    auto chat = *GLOBAL_CHAT;
    char format[] = { '@', '0', '3', 'A', 'E', 'B', 'o', 'o', 's', 't', ':', ' ', '%', 's', '\0' };
    
    if (nullptr != chat)
    {
        string str;

        ((u32*)&str.buffer)[0] = 'TSYS';
        ((u32*)&str.buffer)[1] = '\0ME';
        str.len = 6;
        str.maxLen = 15;

        ((CHAT_WINDOW_MSG)chat->vtable[3])(chat, 0, ChatType::System, &str, format, Message);

        return true;
    }

    return false;
}

ContainerObject* FindContainerObjectOfType(AeContext* Ctx, int Type)
{
    for (auto i = (*Ctx->inventory)->containers; nullptr != i; i = i->next)
    {
        // is container? (hotslot bar object returns 0)
        // CContainerGUI vs HotSlots class
        if (false == ((bool(*)())(i->vtable[0x1A]))())
        {
            continue;
        }

        for (auto j = i->contObjs; nullptr != j; j = j->next)
        {
            if (Type == j->info.type)
            {
                return j;
            }
        }
    }

    return NULL;
}

void SetPetMode(AeContext* AeCtx, NetContext* NetCtx, PetMode Mode, uint32_t PetId)
{
    auto packet = ClientPacket(Mode, PetId);
    Send(AeCtx, NetCtx, &packet);
}

void ChangeRune(AeContext* AeCtx, NetContext* NetCtx, uint8_t RuneSlot, uint32_t itemId)
{
    auto packet = ClientPacket(RuneSlot, itemId);
    Send(AeCtx, NetCtx, &packet);
}

void SelectSpell(AeContext* AeCtx, NetContext* NetCtx, uint32_t SpellId)
{
    auto packet = ClientPacket(SpellId); // select spell
    Send(AeCtx, NetCtx, &packet);
}

void CastSpell(AeContext* AeCtx, NetContext* NetCtx, uint32_t SpellId, uint32_t TargetId)
{
    SelectSpell(AeCtx, NetCtx, SpellId);

    auto packet = ClientPacket(SpellId, TargetId);
    Send(AeCtx, NetCtx, &packet);
}