#include <Mods.h>

void ClientPacket::SetPet(PetMode Mode, uint32_t PetId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::SetPet);
    body.action = (uint8_t)Action::Pet;

    body.SetPet.mode = (uint8_t)Mode;
    memcpy(&body.SetPet.petId, &PetId, sizeof(AeBody::SetPet.petId));

    body.SetPet.zero[0] = 0;
    body.SetPet.zero[1] = 0;
    body.SetPet.zero[2] = 0;
}
void ClientPacket::ChangeRune(uint8_t RuneSlot, uint32_t RuneItemId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::ChangeRune);
    body.action = (uint8_t)Action::Magic;

    body.ChangeRune.magicType = (uint8_t)MagicType::ChangeRune;
    memcpy(&body.ChangeRune.runeItemId, &RuneItemId, sizeof(AeBody::ChangeRune.runeItemId));
    body.ChangeRune.runeSlot = RuneSlot;
}
void ClientPacket::SelectSpell(uint32_t SpellId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::SelectSpell);
    body.action = (uint8_t)Action::Magic;

    body.SelectSpell.magicType = (uint8_t)MagicType::SelectSpell;
    body.SelectSpell.spellId = SpellId;
}
void ClientPacket::CastSpell(uint32_t SpellId, uint32_t TargetId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::CastSpell);
    body.action = (uint8_t)Action::Magic;

    body.CastSpell.magicType = (uint8_t)MagicType::CastSpell;
    body.CastSpell.spellId = SpellId;
    memcpy(&body.CastSpell.targetId, &TargetId, sizeof(AeBody::CastSpell.targetId));
    memset(&body.CastSpell.idk, 0, sizeof(AeBody::CastSpell.idk));
}
void ClientPacket::UseItemOn(uint32_t Id)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::Select);
    body.action = (uint8_t)Action::UseItemOn;
    
    memcpy(&body.Select.id, &Id, sizeof(AeBody::Select.id));
}
void ClientPacket::SelectItem(uint32_t Id)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::Select);
    body.action = (uint8_t)Action::SelectItem;
    
    memcpy(&body.Select.id, &Id, sizeof(AeBody::Select.id));
}
void ClientPacket::SelectTarget(uint32_t Id)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::Select);
    body.action = (uint8_t)Action::SelectTarget;
    
    memcpy(&body.Select.id, &Id, sizeof(AeBody::Select.id));
}
void ClientPacket::DeselectItem()
{
    header.size = sizeof(AeBody::action);
    body.action = (uint8_t)Action::DeselectItem;
}
void ClientPacket::Touch(uint32_t TargetId)
{
    header.size = sizeof(AeBody::action) + sizeof(AeBody::Select);
    body.action = (uint8_t)Action::Touch;
    
    memcpy(&body.Select.id, &TargetId, sizeof(AeBody::Select.id));
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

u16 CalcSendChecksum(ClientPacket* Packet)
{
    u16 checksum = 0;

    if (Packet->header.size)
    {
        auto temp = (u8*)&Packet->body;

        for (int i = 0; i < Packet->header.size; i++)
        {
            checksum += temp[i];
        }
    }

    return checksum;
}

void XorBuffer(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet, bool UndoFailed)
{
    auto buf = (uint8_t*)Packet + sizeof(AeHeader);
    auto thisPtr = &(*AeCtx->gPtr)[GLOBAL_PTR_KEY_OFFSET];
    int32_t key = 0;

    if (UndoFailed)
    {
        key = NetCtx->lastSendKey;
    }
    else
    {
        key = *(int32_t*)(thisPtr + NetCtx->xorKeyOffset);
    }

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

        if (!UndoFailed)
        {
            // save key
            *(int32_t*)(thisPtr + NetCtx->xorKeyOffset) = key;
        }
    }
}

// returns false if the key is not correct
bool UndoXorBuffer(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto buf = (uint8_t*)Packet + sizeof(AeHeader);
    auto thisPtr = &(*AeCtx->gPtr)[GLOBAL_PTR_KEY_OFFSET];
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

        if (Packet->header.checksum == CalcSendChecksum(Packet))
        {
            // save old key
            *(int32_t*)(thisPtr + NetCtx->xorKeyOffset) = NetCtx->lastSendKey;

            return true; // success
        }
        else
        {
            XorBuffer(AeCtx, NetCtx, Packet, true);
        }
    }

    return false;
}

void Send(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    Packet->header.checksum = CalcSendChecksum(Packet);
    XorBuffer(AeCtx, NetCtx, Packet, false);

    auto s = *(u32*)(&(*AeCtx->gPtr)[GLOBAL_PTR_KEY_OFFSET] + NetCtx->xorKeyOffset - 0x30 + 0x10);
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
    for (auto i = (*Ctx->manager)->containers; nullptr != i; i = i->next)
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
    ClientPacket packet;
    
    packet.SetPet(Mode, PetId);
    Send(AeCtx, NetCtx, &packet);
}

void ChangeRune(AeContext* AeCtx, NetContext* NetCtx, uint8_t RuneSlot, uint32_t itemId)
{
    ClientPacket packet;
    
    packet.ChangeRune(RuneSlot, itemId);
    Send(AeCtx, NetCtx, &packet);
}

void SelectSpell(AeContext* AeCtx, NetContext* NetCtx, uint32_t SpellId)
{
    ClientPacket packet;
    
    packet.SelectSpell(SpellId); // select spell
    Send(AeCtx, NetCtx, &packet);
}

void CastSpell(AeContext* AeCtx, NetContext* NetCtx, uint32_t SpellId, uint32_t TargetId)
{
    SelectSpell(AeCtx, NetCtx, SpellId);

    ClientPacket packet;
    
    packet.CastSpell(SpellId, TargetId);
    Send(AeCtx, NetCtx, &packet);
}

void SelectItem(AeContext* AeCtx, NetContext* NetCtx, uint32_t ItemId)
{
    ClientPacket packet;

    packet.SelectItem(ItemId);
    Send(AeCtx, NetCtx, &packet);
}

void SelectTarget(AeContext* AeCtx, NetContext* NetCtx, uint32_t TargetId)
{
    ClientPacket packet;
    
    packet.SelectTarget(TargetId);
    Send(AeCtx, NetCtx, &packet);
}

void DeselectItem(AeContext* AeCtx, NetContext* NetCtx)
{
    ClientPacket packet;

    packet.DeselectItem();
    Send(AeCtx, NetCtx, &packet);
}

void UseItemOn(AeContext* AeCtx, NetContext* NetCtx, uint32_t ItemId, uint32_t TargetId)
{
    SelectItem(AeCtx, NetCtx, ItemId);
    
    ClientPacket packet;

    packet.UseItemOn(TargetId);
    Send(AeCtx, NetCtx, &packet);
}

void Touch(AeContext* AeCtx, NetContext* NetCtx, uint32_t TargetId)
{
    ClientPacket packet;

    packet.Touch(TargetId);
    Send(AeCtx, NetCtx, &packet);
}

// https://onestepcode.com/float-to-int-c/
#define NAN 0x80000000
#define BIAS 127
#define K 8
#define N 23
/* Compute (int) f.
 * If conversion causes overflow or f is NaN, return 0x80000000
 */
typedef unsigned float_bits;
int float_f2i(float f) {
    float_bits* fb = (float_bits*)&f;
    unsigned s = *fb >> (K + N);
    unsigned exp = *fb >> N & 0xFF;
    unsigned frac = *fb & 0x7FFFFF;

    /* Denormalized values round to 0 */
    if (exp == 0)
        return 0;
    /* f is NaN */
    if (exp == 0xFF)
        return NAN;
    /* Normalized values */
    int x;
    int E = exp - BIAS;
    /* Normalized value less than 0, return 0 */
    if (E < 0)
        return 0;
    /* Overflow condition */
    if (E > 30)
        return NAN;
    x = 1 << E;
    if (E < N)
        x |= frac >> (N - E);
    else
        x |= frac << (E - N);
    /* Negative values */
    if (s == 1)
        x = ~x + 1;
    return x;
}