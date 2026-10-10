#include <stdbool.h>

#define BOARD_SIZE 9   // 定义棋盘大小，9x9
#define WIN_LENGTH 5   // 定义连续5颗棋子即可获胜
#define EMPTY_CELL '.'  // 用'.'表示棋盘上的空位置
#define PLAYER_X 'X'   // 先手玩家
#define PLAYER_O 'O'   // 后手玩家
#define TOTAL_CELLS (BOARD_SIZE * BOARD_SIZE)  // 棋盘总格数，用于判断平局

typedef enum {
    GAME_RUNNING, // 游戏进行中，默认值为0
    GAME_X_WINS, // X胜出，默认值为1
    GAME_O_WINS, // O胜出，默认值为2
    GAME_DRAW //平局，默认值为3
} GameStatus; // 枚举游戏状态

typedef enum {
    MOVE_ACCEPTED, // 可以下棋，默认值为0
    MOVE_OUT_OF_BOUNDS, // 当前位置在棋盘外，默认值为1
    MOVE_OCCUPIED // 当前位置已占用，不重复落子，默认值为2
} MoveResult; // 下棋结果

typedef struct {
    char cells[BOARD_SIZE][BOARD_SIZE];
    // 棋盘信息，二维数组，共9x9个位置
    char current_player;
    // 记录当前轮到哪一方下棋，用'X'和'O'表示
    int move_count; // 步数统计，每成功落下一颗棋子计数加一
    GameStatus status;
    // 记录当前游戏状态
    int last_row; // 最后一手所在的行，-1 表示还没有落子（用于高亮最后一手）
    int last_col; // 最后一手所在的列，-1 表示还没有落子
} Game;

void game_init(Game* game);
// 初始化棋盘：全部置空、X 先手、步数清零、状态为进行中

MoveResult game_place(Game* game, int row, int col);
// 在(row, col)落下一颗当前玩家的棋子，并更新胜负状态与执子方

bool check_win(const Game* game,int row,int col);
// 判断五子是否连成一条线（以(row, col)为中心，四个方向双向计数）

bool check_draw(const Game* game);
// 判断棋盘是否已满

void update_game_status(Game* game,int row,int col);
//根据最后一次落子更新游戏状态

char game_opponent(char player);
// 返回另一位玩家

const char* game_status_text(GameStatus status);
// 返回游戏状态的文字描述