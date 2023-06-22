#include <Mods.h>

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

struct StepShootContext
{
    bool hasTarget;
    u32 lastShootTime;
    bool enabled;
};

static void StepShoot(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto volatile ctx = (StepShootContext*)FIXUP_VALUE;
    u16 actionSize = 0;
    u32 weaponId = 0;
    ObjectData* object = nullptr;
    ItemInfo* weapon = nullptr;

    // todo: how to tell if target is lost? (it died or teleported the player)
    // ^ probably should use recv hook

    if (Packet->header.size >= sizeof(AeBody::action))
    {
        for (u32 i = 0; i < Packet->header.size; i += actionSize)
        {
            auto nextAction = (AeBody*)((u8*)&Packet->body + i);
            bool cancelAction = false;
            actionSize = sizeof(AeBody::action);

            if ((uint8_t)Action::NotCombatMode == nextAction->action)
            {
                ctx->hasTarget = false;
                ctx->lastShootTime = 0;
            }
            else if ((uint8_t)Action::SelectTarget == nextAction->action)
            {
                actionSize = sizeof(AeBody::action) + sizeof(AeBody::Select);

                ctx->hasTarget = true;
            }
            // todo: make a function or macro to simplify the size checking- can we reuse an AE function?
            else if ((uint8_t)Action::Move == nextAction->action)
            {
                actionSize = sizeof(AeBody::action) + sizeof(AeBody::Move);

                if (ctx->enabled && ctx->hasTarget)
                {
                    // get equipped item info
                    weaponId = (*ModCtx->ae.inventory)->equippedWeaponId;
                    if (!weaponId) return;

                    object = ModCtx->ae.getObjectFromId(&weaponId);
                    if (!object) return;

                    weapon = &(*ModCtx->ae.items)->items[object->type];
                    if (!weapon) return;

                    if (ItemFam::Weapon != (ItemFam)weapon->fam) return;
                    if (ItemCls::Ranged != (ItemCls)weapon->w_cls && ItemCls::Magic != (ItemCls)weapon->w_cls) return;

                    // constant declared locally to prevent the compiler from making it a global
                    volatile u32 multiplier = 0x447A0000; // 1000.0f
                    u32 timeToStepMs = float_f2i((*ModCtx->ae.inventory)->attackSpeed * *(float*)&multiplier) - 100;

                    if (ModCtx->getTimeMs() >= ctx->lastShootTime + timeToStepMs)
                    {
                        ctx->lastShootTime = ModCtx->getTimeMs();
                    }
                    else
                    {
                        cancelAction = true;
                    }
                }
            }
            else
            {
                break;
            }

            if (cancelAction)
            {
                // safe with same buffer because destination is before source (see memmove)
                memcpy(nextAction, (u8*)nextAction + actionSize, Packet->header.size - i - actionSize);
                Packet->header.size -= actionSize;
                actionSize = 0;
            }
        }
    }
}

static void ToggleStepShoot(ModContext* ModCtx, int Key)
{
    auto volatile ctx = (StepShootContext*)FIXUP_VALUE;
    char enabled[] = { 'S', 't', 'e', 'p', 'S', 'h', 'o', 'o', 't', ' ', 'O', 'n', '\0' };
    char disabled[] = { 'S', 't', 'e', 'p', 'S', 'h', 'o', 'o', 't', ' ', 'O', 'f', 'f', '\0' };

    ctx->enabled ^= 1;

    if (ctx->enabled)
    {
        WriteToChatWindow(enabled);
    }
    else
    {
        WriteToChatWindow(disabled);
    }
}

END_INJECT_CODE;

void InitStepShoot()
{
    StepShootContext context;
    u32 remoteContext = 0;
    
    memset(&context, 0, sizeof(context));
    AllocContext(&context, sizeof(context), &remoteContext);
    if (0 == remoteContext)
    {
        goto fail;
    }

#define VK_TAB            0x09 // todo: get key from GUI and remove
    RegisterForSendHook(StepShoot, remoteContext);
    RegisterForKeyCallback(ToggleStepShoot, VK_TAB, remoteContext);

fail:
    return;
}