using StalkerOnline.Shared.Game;

namespace StalkerOnline.Server.Services;

public sealed class CharacterService
{
    private readonly CharacterRepository _characterRepository;

    public CharacterService(CharacterRepository characterRepository)
    {
        _characterRepository = characterRepository;
    }

    public PlayerState LoadOrCreatePlayerState(int accountId, string login)
    {
        PlayerState? existingState = _characterRepository.FindByAccountId(accountId);

        if (existingState != null)
        {
            Console.WriteLine(
                $"[CHARACTER LOADED] AccountId={accountId}, CharacterId={existingState.CharacterId}, Nickname={existingState.Nickname}, Position={existingState.Position}");

            return existingState;
        }

        string nickname = login;

        PlayerState newState = _characterRepository.CreateDefault(accountId, nickname);

        Console.WriteLine(
            $"[CHARACTER CREATED] AccountId={accountId}, CharacterId={newState.CharacterId}, Nickname={newState.Nickname}");

        return newState;
    }

    public void SavePlayerState(PlayerState state)
    {
        _characterRepository.Save(state);

        Console.WriteLine(
            $"[CHARACTER SAVED] AccountId={state.AccountId}, CharacterId={state.CharacterId}, Nickname={state.Nickname}, Position={state.Position}");
    }
}