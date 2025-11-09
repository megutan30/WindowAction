using MultiWindowActionGame.Player;

namespace MultiWindowActionGame.Player
{
    public interface IPlayerFormFactory
    {
        PlayerForm CreatePlayer(Point startPosition);
    }
}