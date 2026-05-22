namespace StalkerOnline.Server.Game;

public enum WorldObjectType : byte
{
    None = 0,

    Player = 1,
    Item = 2,
    LootContainer = 3,
    DeadBody = 4,

    Npc = 10,
    Mutant = 11
}