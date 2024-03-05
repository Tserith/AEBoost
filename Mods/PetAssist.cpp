#include <Mods.h>

struct PetAssistContext
{
    u32 petId[2];
    bool toggleMode;
};

// send here instead of inside recv hook so we don't break state
static void PetAssistSend(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto volatile ctx = (PetAssistContext*)FIXUP_VALUE;
    PetMode mode = ctx->toggleMode ? PetMode::Protect : PetMode::Assist;

    if (ctx->petId[0])
    {
        SetPetMode(&ModCtx->ae, NetCtx, mode, ctx->petId[0]);

        if (ctx->petId[1])
        {
            SetPetMode(&ModCtx->ae, NetCtx, mode, ctx->petId[1]);
        }

        memset(ctx->petId, 0, sizeof(ctx->petId));
    }
}

static void PetAssistRecv(ModContext* ModCtx, NetContext* NetCtx, ServerPacket* Packet)
{
    auto volatile ctx = (PetAssistContext*)FIXUP_VALUE;
    u32* petId = nullptr;

    if (ctx->petId[0])
    {
        if (!ctx->petId[1])
        {
            petId = &ctx->petId[1];
        }
    }
    else
    {
        petId = &ctx->petId[0];
    }

    if (petId && 0x12 == Packet->size &&
        Action::Pet == (Action)Packet->action &&
        PetAction::Spawn == (PetAction)Packet->data[0])
    {
        memcpy(petId, &Packet->data[1], 3);
    }
}

static void ToggleAssist(ModContext* ModCtx, int Key)
{
    auto volatile ctx = (PetAssistContext*)FIXUP_VALUE;
    char assist[] = { 'P', 'e', 't', 'A', 's', 's', 'i', 's', 't', '\0' };
    char protect[] = { 'P', 'e', 't', 'P', 'r', 'o', 't', 'e', 'c', 't', '\0' };

    ctx->toggleMode ^= 1;

    if (ctx->toggleMode)
    {
        WriteToChatWindow(protect);
    }
    else
    {
        WriteToChatWindow(assist);
    }
}

END_INJECT_CODE;

void InitPetAssist()
{
    PetAssistContext context;
    u32 remoteContext = 0;

    memset(&context, 0, sizeof(context));

    AllocContext(&context, sizeof(context), &remoteContext);
    if (0 == remoteContext)
    {
        goto fail;
    }

    RegisterForSendHook(PetAssistSend, remoteContext);
    RegisterForRecvHook(PetAssistRecv, remoteContext);
    
#define VK_RMENU            0xA5 // right alt
    RegisterForKeyCallback(ToggleAssist, VK_RMENU, remoteContext);

fail:
    return;
}