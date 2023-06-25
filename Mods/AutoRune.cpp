#include <Mods.h>

static int __fastcall RuneSlotHook(Spells* Spells, int ThiscallFix, int RuneType) // must have 3rd param to fix up stack (__thiscall)
{
    int index = 1;

    if (RuneType >= 1452 && RuneType <= 1455)
    {
        return 1;
    }

    for (u32* i = Spells->runeSlotTypes; *i != RuneType; i++)
    {
        if (++index >= 10)
        {
            return -1;
        }
    }

    return index;
}

static void AutoRune(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    ContainerObject* rune = NULL;
    auto castSpell = &Packet->body;
    bool reselectSpell = false;
    bool cancelSend = false;

    if (Packet->header.size >= sizeof(AeBody::action) + sizeof(AeBody::CastSpell))
    {
        // search packet for a CastSpell action
        // this search must be aggressive because missing one would disconnect the client
        for (u32 i = 0; i < Packet->header.size - sizeof(AeBody::CastSpell); i++)
        {
            castSpell = (AeBody*)((u8*)&Packet->body + i);

            CONTINUE_IF((uint8_t)Action::Magic != castSpell->action || (uint8_t)MagicType::CastSpell != castSpell->CastSpell.magicType);
            
            for (auto spell = (*ModCtx->ae.spells)->spell; spell < &(*ModCtx->ae.spells)->spell[0x200]; spell++)
            {
                CONTINUE_IF(spell->id != castSpell->CastSpell.spellId);
                
                // if the wrong rune class is equipped
                if ((*ModCtx->ae.spells)->runeSlotTypes[0] != spell->spellClass + RUNE_TYPE_OFFSET)
                {
                    rune = FindContainerObjectOfType(&ModCtx->ae, spell->spellClass + RUNE_TYPE_OFFSET);

                    if (rune)
                    {
                        ChangeRune(&ModCtx->ae, NetCtx, 1, rune->id);
                        reselectSpell = true;
                    }
                    else
                    {
                        char runeError[] = { 'R', 'u', 'n', 'e', ' ', 'm', 'i', 's', 's', 'i', 'n', 'g', ' ', 'f', 'o', 'r', ' ', 'c', 'a', 's', 't', '\0' };
                        WriteToChatWindow(runeError);

                        cancelSend = true;
                    }
                }

                uint8_t maxLeftToRefresh = spell->numr;

                // refresh runes that are low
                for (uint8_t i = 0; i < 0xA; i++)
                {
                    // only refresh as many runes required by the current spell to avoid
                    // refreshing a rune higher than the player's weapon's rune slot
                    BREAK_IF(0 == maxLeftToRefresh);

                    // don't refresh a class rune we just changed if the old one was low
                    CONTINUE_IF(0 == i && reselectSpell);

                    if (0 < (*ModCtx->ae.spells)->runeSlotCharges[i] && 6 > (*ModCtx->ae.spells)->runeSlotCharges[i])
                    {
                        rune = FindContainerObjectOfType(&ModCtx->ae, (*ModCtx->ae.spells)->runeSlotTypes[i]);

                        if (rune)
                        {
                            ChangeRune(&ModCtx->ae, NetCtx, 1 + i, rune->id);
                            reselectSpell = true;
                            maxLeftToRefresh--;
                        }
                        else
                        {
                            char runeError[] = { 'C', 'a', 'n', 'n', 'o', 't', ' ', 'r', 'e', 'f', 'r', 'e', 's', 'h', ' ', 'r', 'u', 'n', 'e', '\0' };
                            WriteToChatWindow(runeError);
                        }
                    }
                }

                break;
            }

            break;
        }

        if (cancelSend && !reselectSpell)
        {
            SelectSpell(&ModCtx->ae, NetCtx, 0xffffffff); // unselect
        }
        else if (reselectSpell) // equipping a rune unselects the spell
        {
            SelectSpell(&ModCtx->ae, NetCtx, castSpell->CastSpell.spellId);
        }
    }

    if (cancelSend)
    {
        Packet->header.size = 0;
    }
}

END_INJECT_CODE;

// stretch goal: hook canSpellBeCast (see v149 idb) to light up spells the player has runes for and block out those they don't
void InitAutoRune()
{
    u8 pattern[] = { 0x55, 0x8B, 0xEC, 0x8B, 0x55, 0x08, 0xB8, 0x01, 0x00, 0x00, 0x00 };

    InlineHook(pattern, sizeof(pattern), RuneSlotHook, 0);

    RegisterForSendHook(AutoRune, 0);
}