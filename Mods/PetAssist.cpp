#include <Mods.h>

struct PetAssistContext
{
    u32 petId[2];
};

// send here instead of inside recv hook so we don't break state
static void PetAssistSend(ModContext* ModCtx, NetContext* NetCtx, ClientPacket* Packet)
{
    auto volatile ctx = (PetAssistContext*)FIXUP_VALUE;

    if (ctx->petId[0])
    {
        SetPetMode(&ModCtx->ae, NetCtx, PetMode::Assist, ctx->petId[0]);

        if (ctx->petId[1])
        {
            SetPetMode(&ModCtx->ae, NetCtx, PetMode::Assist, ctx->petId[1]);
        }

        memset(ctx, 0, sizeof(*ctx));
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

fail:
    return;
}