#pragma once
#include <stdint.h>
#include <intrin.h>

#pragma warning(disable: 4200)

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

// note: make sure /0b0 is not used (default for debug builds) or
// else __forceinline will not be honored and the target will crash

__forceinline void* memset(void* Dst, u8 Val, size_t Cnt)
{
    __writeeflags(__readeflags() & ~(1 << 10)); // cld

    __stosb((u8*)Dst, Val, Cnt);

    return NULL;
}

__forceinline void* memcpy(void* Dst, const void* Src, size_t Cnt)
{
    __writeeflags(__readeflags() & ~(1 << 10)); // cld

    __movsb((u8*)Dst, (const u8*)Src, Cnt);

    return Dst;
}

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

struct ChatManager;
struct ObjectData;
typedef ObjectData* (__thiscall* GET_OBJECT_FUNC)(u32* Id);

// these values will need to be updated for every build of the client
// until I'm not lazy enough to actually write heuristics
#define VERSION "1.1.0"
#define AE_BUILD 217
#define GLOBAL_PTR (uint8_t**)0x747D50 // after ref to string containing "Launching"
#define GLOBAL_SPELLS (Spells**)0x7A8950 // passed to rune function
#define GLOBAL_MANAGER (ObjectDataManager**)0x747F14 // a few lines above "Send frequency" string
#define GLOBAL_ITEMS (Items**)0x7A88C0 // right above "Initialized Spell Manager" string
#define GLOBAL_GET_OBJECT (GET_OBJECT_FUNC)0x408DF0 // called at end of case with "NMSG_TARGET_HEALTH" string
#define XOR_KEY_OFFSET 0x4004C // offset used for key in xor function that calls net
#define GLOBAL_CHAT (ChatManager**)0xB3FBD8 // a bit above "%s has died!\n" string
#define SEND_IAT_ENTRY 15
#define RECV_IAT_ENTRY 4
#define RUNE_TYPE_OFFSET 0x5AC

#pragma pack(push, 1)
struct ChatManager
{
    void** vtable;
};
enum class ChatType
{
    Say = 0,
    Admin = 2,
    Party = 4,
    System = 0xb
};

typedef void (__cdecl*CHAT_WINDOW_MSG)(void* ThisPtr, u32 Zero, ChatType Type, void* Null, char* Format, char* Message);

struct ContainerObjectInfo
{
    void* vtable;
    u8 unk[8];
    int type;
    u8 unk2[4];
};

struct ContainerObject
{
    void* vtable;
    struct ContainerObject* next;
    u8 unk[8];
    int id;
    ContainerObjectInfo info;
};

struct Container
{
    void** vtable;
    struct Container* next;
    struct Container* last;
    u8 unk[20];
    uint32_t typeOrSomething;
    u8 unk1[268];
    int bagId;
    u8 unk2[8];
    int itemId;
    u8 unk3[44];
    ContainerObject* contObjs;
    u8 unk4[376];
};

struct ObjectDataManager
{
    u8 unk[36];
    Container* containers;
    Container* containers2;
    u8 unk2[636];
    bool combatMode; // 680
    u8 unk3[87];
    u8 guildName[64]; // idk length
    u8 unk4[496];
    u32 equippedAmmoId; // 1328
    u32 equippedIdk[2];
    u32 equippedWeaponId; // 1340
    u8 unk5[148];
    float moveSpeed;
    float attackSpeed;
    int32_t toHit;
    u8 unk6[12];
    bool poisoned;
    bool diseased;
    u8 unk7[110];
    int targetId; // casting sets this and does not restore the old value
    u8 unk8[4];
    bool targetProtection;
};

struct ObjectData
{
    void* vptr;
    int id;
    int type;
};

enum class ItemFam
{
    Weapon = 4
};

enum class ItemCls
{
    Ranged = 5,
    Magic = 6
};

enum class ItemAmmo
{
    Arrow,
    Bolt
};

// same order as char screen I'm dumb for doing this manually
enum class ResitanceIndex
{
    Blunt = 1,
    Pierce = 2,
    Arrow = 4,
    Nature = 6,
    Soul = 7,
    Body = 9,
    Disease = 11
};

struct ItemInfo
{
  uint8_t unk[168];
  uint32_t w_ammo;
  uint8_t unk2[32];
  uint32_t fam;
  uint8_t unk3[36];
  uint32_t r_str;
  uint32_t r_int;
  uint32_t r_dex;
  uint32_t r_con;
  uint8_t unk4[104];
  uint32_t w_cls;
  uint8_t unk5[8];
  double w_speed;
  uint8_t unk6[152];
  double ar_resitances[15]; // sic
  uint8_t unk7[80];
};

struct Items
{
    uint32_t itemCount;
    ItemInfo* items;
};

enum class SpellClass
{
    Mind = 0,
    Body = 1,
    Soul = 2,
    Nature = 3
};

struct SpellInfo
{
    uint32_t id;
    u8 irrelavent2[0x220];
    uint32_t spellClass;
    u8 irrelavent3[4];
    uint32_t wtime;
    uint32_t csped;
    u8 irrelavent4[0x10];
    uint32_t numr;
    u8 irrelavent5[0x40];
};

struct Spells
{
    u8 unk[8];
    SpellInfo spell[0x200];
    u8 unk2[0x10];
    uint32_t runeSlotTypes[0xA];
    uint32_t runeSlotCharges[0xA];
};


struct AeContext
{
    ObjectDataManager** manager;
    Spells** spells;
    Items** items;
    uint8_t** gPtr;
    GET_OBJECT_FUNC getObjectFromId;
};

struct NetContext
{
    uint32_t xorKeyOffset;
    uint32_t lastSendKey;
    int (__stdcall *send)(u32 s, const char* buf, int len, int flags);
    int (__stdcall *recv)(u32 s, const char* buf, int len, int flags);
};

enum class Action
{ // some are out of date
    Hello = 0xd,
    UseItemOn = 0x2a,
    UseGroundTool = 0x2b,
    DeselectItem = 0x2c,
    Move = 0x2d,
    Touch = 0x2e,
    CombatMode = 0x32,
    SelectTarget = 0x33,
    GrabItem = 0x36,
    DragItem = 0x37,
    MoveItem = 0x38,
    SplitItem = 0x39,
    UnequipItem = 0x3b,
    NotCombatMode = 0x3e,
    GetInfo = 0x5a,
    Trade = 0x8d,
    Pet = 0x95,
    Magic = 0x99,
    Logout = 0x9b,
    SelectItem = 0x9c
};

enum class PetAction
{
    Spawn = 0x2,
    Target = 0x4
};

enum class PetMode
{
    Follow = 0xb,
    Protect = 0xd,
    Dismiss = 0xa,
    Assist = 0x13
};

enum class MagicType
{
    ChangeRune = 1,
    SelectSpell = 3,
    CastSpell = 5
};

struct AeHeader
{
    uint16_t size;
    uint16_t checksum;
};

struct AeBody
{
    uint8_t action = 0;

    union
    {
        struct
        {
            uint8_t type;
        }Move;
        struct
        {
            uint8_t mode;
            uint8_t petId[3];
            uint8_t zero[3]; // used for protect?
        }SetPet;
        struct
        {
            u8 idk;
            uint8_t itemId[3];
        }Drag;
        struct
        {
            uint8_t id[3];
        }Select;
        struct
        {
            uint8_t magicType;
            uint8_t runeItemId[3];
            uint8_t runeSlot;
        }ChangeRune;
        struct
        {
            uint8_t magicType;
            uint32_t spellId;
        }SelectSpell;
        struct
        {
            uint8_t magicType;
            uint32_t spellId;
            uint8_t targetId[3];
            uint8_t idk[3];
        }CastSpell;
    };
};

enum class PlayerDataRace
{
    Human = 0x41,
    Orc = 0x42,
    Elf = 0x44
};

struct PlayerPacketData
{
    u8 id[3];
    u8 race;
    u8 unk[2];
    u32 hp1;
    u32 hp2;
    u8 unk2;
    u8 lvl;
    u8 unk6[7];
    u8 name[];
};

struct ClientPacket
{
    AeHeader header;
    AeBody body;

    void SetPet(PetMode Mode, uint32_t PetId);
    void ChangeRune(uint8_t RuneSlot, uint32_t RuneItemId);
    void SelectSpell(uint32_t SpellId);
    void CastSpell(uint32_t SpellId, uint32_t TargetId);
    void UseItemOn(uint32_t Id);
    void SelectItem(uint32_t Id);
    void SelectTarget(uint32_t Id);
    void DeselectItem();
    void Touch(uint32_t TargetId);
};

struct ServerPacket
{
    u8 action;
    u32 size; // total size, not sizeof data
    u8 data[0];
};
#pragma pack(pop)

uint16_t CalcReceiveChecksum(uint8_t* Buffer, uint16_t Len);
void XorBuffer(AeContext* AeCtx, NetContext* SendCtx, ClientPacket* Packet, bool UndoFailed);
bool UndoXorBuffer(AeContext* AeCtx, NetContext* SendCtx, ClientPacket* Packet);
u16 CalcSendChecksum(ClientPacket* Packet);
void Send(AeContext* AeCtx, NetContext* NetCtx, ClientPacket* Packet);

bool WriteToChatWindow(char* Message);
ContainerObject* FindContainerObjectOfType(AeContext* Ctx, int Type);
void SetPetMode(AeContext* AeCtx, NetContext* NetCtx, PetMode Mode, uint32_t PetId);
void ChangeRune(AeContext* AeCtx, NetContext* SendCtx, uint8_t RuneSlot, uint32_t itemId);
void SelectSpell(AeContext* AeCtx, NetContext* SendCtx, uint32_t SpellId);
void CastSpell(AeContext* AeCtx, NetContext* SendCtx, uint32_t SpellId, uint32_t TargetId);
void SelectItem(AeContext* AeCtx, NetContext* NetCtx, uint32_t ItemId);
void SelectTarget(AeContext* AeCtx, NetContext* NetCtx, uint32_t TargetId);
void DeselectItem(AeContext* AeCtx, NetContext* NetCtx);
void UseItemOn(AeContext* AeCtx, NetContext* NetCtx, uint32_t ItemId, uint32_t TargetId);
void Touch(AeContext* AeCtx, NetContext* NetCtx, uint32_t TargetId);

int float_f2i(float f);