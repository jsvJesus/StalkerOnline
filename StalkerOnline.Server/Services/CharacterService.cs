using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Services;

public sealed class CharacterService
{
    public PlayerState LoadOrCreatePlayerState(int accountId, string login)
    {
        // Временно создаём персонажа в памяти.
        // Потом тут будет загрузка персонажа из базы данных.

        string nickname = login;

        PlayerState state = PlayerState.CreateDefault(accountId, nickname);

        return state;
    }
}