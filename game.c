#include "game.h"

/* 四个需要检查的方向：横向、纵向、右下斜向、左下斜向 */
static const int DIRECTIONS[4][2] = {
    {0, 1},   // 横向：向右
    {1, 0},   // 纵向：向下
    {1, 1},   // 斜向：向右下
    {1, -1}   // 斜向：向左下
};

static int count_direction(
// 统计从指定位置出发，沿某个方向连续相同的棋子的数量
    const Game* game,
    int row,
    int col,
    int row_dir,  // 行的变化方向
    int col_dir,  // 列的变化方向
    char player   // 要统计的玩家（显式传入，起点越界时也能安全返回 0）
){
    int count = 0;

    //从当前位置开始，沿指定方向不断前进
    while (row >= 0 && row < BOARD_SIZE &&
           col >= 0 && col < BOARD_SIZE &&
           game->cells[row][col] == player){
                count++;   // 当前位置的棋子与player相同，计数加一
                row += row_dir;
                col += col_dir;
           }
    return count;
}

bool check_win(const Game* game,int row,int col)
{
    // 检查传入的坐标是否合法
    if (row < 0 || row >= BOARD_SIZE ||
        col < 0 || col >= BOARD_SIZE ||
        game->cells[row][col] == EMPTY_CELL){
            return false;  // 坐标越界或该格为空，不可能连成五子
        }

    char player = game->cells[row][col];  // 只需要判断最后一手这一方

    // 依次检查4种方向：正方向 + 反方向，总数达到5即获胜
    for (int i = 0; i < 4; i++)
    {
        int row_dir = DIRECTIONS[i][0];
        int col_dir = DIRECTIONS[i][1];
        int forward = count_direction(game, row, col, row_dir, col_dir, player);
        int backward = count_direction(game, row - row_dir, col - col_dir,
                                       -row_dir, -col_dir, player);

        if (forward + backward >= WIN_LENGTH){
            return true;
        }
    }
    return false;  // 四个方向都没连成五子
}

bool check_draw(const Game* game)
{
    // 棋盘被填满且无人获胜即为平局
    return game->move_count >= TOTAL_CELLS;
}

void update_game_status(Game* game,int row,int col)
{
    if (check_win(game, row, col)){
        game->status = (game->cells[row][col] == PLAYER_X) ? GAME_X_WINS : GAME_O_WINS;
    }
    else if (check_draw(game)){
        game->status = GAME_DRAW;
    }
    else{
        game->status = GAME_RUNNING;
    }
}

MoveResult game_place(Game* game, int row, int col)
{
    // 落在棋盘之外（包括点击到网格线的情况）
    if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE){
        return MOVE_OUT_OF_BOUNDS;
    }
    // 已经有棋子的格子不能重复落子
    if (game->cells[row][col] != EMPTY_CELL){
        return MOVE_OCCUPIED;
    }
    // 游戏已经结束时不再接受落子
    if (game->status != GAME_RUNNING){
        return MOVE_OCCUPIED;
    }

    game->cells[row][col] = game->current_player;
    game->move_count++;
    game->last_row = row;
    game->last_col = col;

    update_game_status(game, row, col);

    // 只有游戏还在进行时才换手，避免结束后 current_player 被改写
    if (game->status == GAME_RUNNING){
        game->current_player = game_opponent(game->current_player);
    }
    return MOVE_ACCEPTED;
}

void game_init(Game* game)
{
    for (int row = 0; row < BOARD_SIZE; row++){
        for (int col = 0; col < BOARD_SIZE; col++){
            game->cells[row][col] = EMPTY_CELL;  // 棋盘初始全部为空
        }
    }
    game->current_player = PLAYER_X;  // X 先手
    game->move_count = 0;            // 步数清零
    game->status = GAME_RUNNING;     // 游戏进行中
    game->last_row = -1;
    game->last_col = -1;
}

char game_opponent(char player)
{
    return (player == PLAYER_X) ? PLAYER_O : PLAYER_X;
}

const char* game_status_text(GameStatus status)
{
    switch (status){
        case GAME_X_WINS: return "X 获胜";
        case GAME_O_WINS: return "O 获胜";
        case GAME_DRAW:   return "平局";
        default:          return "游戏进行中";
    }
}
