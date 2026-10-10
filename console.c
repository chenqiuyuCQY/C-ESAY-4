#include "console.h"

#include <stdio.h>
#include <string.h>

#define MAX_FRAME_WIDTH BOARD_CHAR_WIDTH        // 帧缓冲区宽度固定为棋盘宽度
#define MAX_FRAME_HEIGHT LAYOUT_FULL_HEIGHT     // 帧缓冲区最大高度（完整布局）

/* 配色：只用文字属性表示，不依赖 ANSI 转义序列，兼容旧版控制台 */
#define ATTR_GRID   (FOREGROUND_INTENSITY)                                       // 网格线：暗灰
#define ATTR_EMPTY  (FOREGROUND_INTENSITY)                                       // 空格子：暗灰
#define ATTR_X      (FOREGROUND_RED | FOREGROUND_INTENSITY)                      // X：亮红
#define ATTR_O      (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY)  // O：亮青
#define ATTR_LAST   (BACKGROUND_INTENSITY)                                       // 最后一手：暗底高亮
#define ATTR_HOVER  (BACKGROUND_GREEN | BACKGROUND_INTENSITY)                    // 鼠标悬停：暗绿底
#define ATTR_TITLE  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define ATTR_STATUS (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define ATTR_RESULT (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define ATTR_HINT   (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)

static void console_clear(const GameConsole* console);

/* ---------- 基础工具：UTF-8 转宽字符、帧缓冲区操作 ---------- */

// 控制台绘制接口使用宽字符，这里把源码里的 UTF-8 中文转换成宽字符
static void utf8_to_wide(const char* utf8, wchar_t* out, size_t out_count)
{
    if (out_count == 0){
        return;
    }
    out[0] = L'\0';
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, (int)out_count) <= 0){
        return;  // 转换失败时保持空串
    }
    out[out_count - 1] = L'\0';  // 保证一定以 0 结尾
}

// 计算宽字符串占用的字符格数（中文等全角字符算 2 格），用于居中
static int wide_width(const wchar_t* text)
{
    int width = 0;
    for (; *text != L'\0'; text++){
        width += (*text > 0x7F) ? 2 : 1;
    }
    return width;
}

static void frame_put(CHAR_INFO* frame, const GameConsole* console, int x, int y,
                      wchar_t ch, WORD attr)
{
    if (x < 0 || x >= console->frame_width || y < 0 || y >= console->frame_height){
        return;
    }
    frame[y * MAX_FRAME_WIDTH + x].Char.UnicodeChar = ch;
    frame[y * MAX_FRAME_WIDTH + x].Attributes = attr;
}

static void frame_fill(CHAR_INFO* frame, wchar_t ch, WORD attr)
{
    for (int i = 0; i < MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT; i++){
        frame[i].Char.UnicodeChar = ch;
        frame[i].Attributes = attr;
    }
}

static void frame_put_utf8(CHAR_INFO* frame, const GameConsole* console, int y,
                           const char* utf8, WORD attr, bool centered)
{
    wchar_t wide[256];
    utf8_to_wide(utf8, wide, 256);
    int x = centered ? (console->frame_width - wide_width(wide)) / 2 : 0;
    if (x < 0){
        x = 0;
    }
    // 按显示宽度摆放：全角字符占两格，放不下就停止，避免半个汉字压到帧外
    int col = x;
    for (int i = 0; wide[i] != L'\0'; i++){
        int cells = (wide[i] > 0x7F) ? 2 : 1;
        if (col + cells > console->frame_width){
            break;
        }
        frame_put(frame, console, col, y, wide[i], attr);
        col += cells;
    }
}

/* ---------- 三种绘制内容 ---------- */

static void frame_draw_title(CHAR_INFO* frame, const GameConsole* console)
{
    if (console->title_lines == 0){
        return;  // 紧凑布局没有标题区
    }
    // 注意：整帧宽度只有 37 个字符格，中文占两格，文字必须能放进这一宽度
    frame_put_utf8(frame, console, 0, "五子棋 GOMOKU  (9 x 9)", ATTR_TITLE, true);
}

static void frame_draw_board(CHAR_INFO* frame, const GameConsole* console, const Game* game)
{
    bool full_grid = (console->cell_height == CELL_HEIGHT);

    for (int y = 0; y < console->board_height; y++){
        int frame_y = console->title_lines + y;
        // 完整布局每隔 2 行画一条横网格线；紧凑布局只在棋盘最上和最下画边框
        bool border = full_grid ? (y % CELL_HEIGHT == 0)
                                : (y == 0 || y == console->board_height - 1);
        int row = full_grid ? (y - 1) / CELL_HEIGHT : y - 1;  // 边框行用不到 row

        for (int x = 0; x < console->frame_width; x++){
            if (border){
                frame_put(frame, console, x, frame_y,
                          (x % CELL_WIDTH == 0) ? L'+' : L'-', ATTR_GRID);
                continue;
            }
            if (x % CELL_WIDTH == 0){
                frame_put(frame, console, x, frame_y, L'|', ATTR_GRID);
                continue;
            }

            int col = x / CELL_WIDTH;
            bool hovered = (row == console->hover_row && col == console->hover_col);

            if (x % CELL_WIDTH == 2){
                // 棋格中心放棋子
                char cell = game->cells[row][col];
                WORD attr = ATTR_EMPTY;
                if (cell == PLAYER_X){
                    attr = ATTR_X;
                } else if (cell == PLAYER_O){
                    attr = ATTR_O;
                }
                if (row == game->last_row && col == game->last_col){
                    attr |= ATTR_LAST;  // 高亮最后一手，方便看清对手刚下在哪里
                }
                if (cell == EMPTY_CELL && hovered){
                    // 悬停时预览当前玩家将要落下的棋子
                    attr = (game->current_player == PLAYER_X) ? ATTR_X : ATTR_O;
                }
                if (hovered){
                    attr |= ATTR_HOVER;
                }
                frame_put(frame, console, x, frame_y, (wchar_t)cell, attr);
            } else {
                frame_put(frame, console, x, frame_y, L' ',
                          hovered ? (WORD)(ATTR_EMPTY | ATTR_HOVER) : ATTR_EMPTY);
            }
        }
    }
}

static void frame_draw_status(CHAR_INFO* frame, const GameConsole* console, const Game* game)
{
    int top = console->title_lines + console->board_height;  // 状态区第一行
    char line[256];
    bool running = (game->status == GAME_RUNNING);

    // 紧凑/迷你布局：状态区只有一行，把执子方、状态、步数（或提示）挤在一行里
    if (console->status_lines <= 1){
        if (console->hint[0] != '\0'){
            snprintf(line, sizeof line, "  %s", console->hint);
            frame_put_utf8(frame, console, top, line, ATTR_HINT, false);
        } else if (running){
            snprintf(line, sizeof line, "  当前：%c  进行中  已落子 %d/%d",
                     game->current_player, game->move_count, TOTAL_CELLS);
            frame_put_utf8(frame, console, top, line,
                           (game->current_player == PLAYER_X) ? ATTR_X : ATTR_O, false);
        } else {
            snprintf(line, sizeof line, "  结束：%s  有效步数 %d/%d",
                     game_status_text(game->status), game->move_count, TOTAL_CELLS);
            frame_put_utf8(frame, console, top, line, ATTR_RESULT, false);
        }
        return;
    }

    // 完整布局：空行 + 当前执子 + 状态与步数 + 操作提示
    if (running){
        snprintf(line, sizeof line, "  当前执子：%c", game->current_player);
        frame_put_utf8(frame, console, top + 1, line,
                       (game->current_player == PLAYER_X) ? ATTR_X : ATTR_O, false);
    } else {
        snprintf(line, sizeof line, "  游戏结束：%s", game_status_text(game->status));
        frame_put_utf8(frame, console, top + 1, line, ATTR_RESULT, false);
    }

    if (running){
        snprintf(line, sizeof line, "  状态：进行中    已落子：%d / %d",
                 game->move_count, TOTAL_CELLS);
    } else {
        snprintf(line, sizeof line, "  状态：%s    有效步数：%d / %d",
                 game_status_text(game->status), game->move_count, TOTAL_CELLS);
    }
    frame_put_utf8(frame, console, top + 2, line, ATTR_STATUS, false);

    if (console->hint[0] != '\0'){
        snprintf(line, sizeof line, "  %s", console->hint);
        frame_put_utf8(frame, console, top + 3, line, ATTR_HINT, false);
    } else if (running){
        frame_put_utf8(frame, console, top + 3, "  鼠标左键点击棋格落子，Esc 退出",
                       ATTR_HINT, false);
    } else {
        frame_put_utf8(frame, console, top + 3, "  按 R 重新开始一局，按其他键退出",
                       ATTR_HINT, false);
    }
}

/* ---------- 窗口尺寸与布局 ---------- */

// 计算可见窗口的宽高（行/列）
static bool console_view_size(const GameConsole* console, int* width, int* height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(console->h_output, &info)){
        return false;
    }
    *width = info.srWindow.Right - info.srWindow.Left + 1;
    *height = info.srWindow.Bottom - info.srWindow.Top + 1;
    return true;
}

// 如果视口被滚动过（窗口不在缓冲区左上角），把它移回左上角，
// 否则整帧会画在可见区域之外，看起来就像“棋盘只剩几行”
static void console_home_viewport(const GameConsole* console)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(console->h_output, &info)){
        return;
    }
    if (info.srWindow.Left == 0 && info.srWindow.Top == 0){
        return;  // 已经在左上角
    }
    int width = info.srWindow.Right - info.srWindow.Left + 1;
    int height = info.srWindow.Bottom - info.srWindow.Top + 1;
    SMALL_RECT window = {0, 0, (SHORT)(width - 1), (SHORT)(height - 1)};
    SetConsoleWindowInfo(console->h_output, TRUE, &window);  // 只滚动视口，不改变窗口大小
}

/* 按可见窗口大小选择布局并算出棋盘位置：
   窗口够高用完整布局，不足则依次降级为紧凑、迷你布局 */
static void console_layout(GameConsole* console, int view_width, int view_height)
{
    if (view_height >= LAYOUT_FULL_HEIGHT){
        console->cell_height = CELL_HEIGHT;
        console->title_lines = 2;
        console->status_lines = 4;
    } else if (view_height >= LAYOUT_COMPACT_HEIGHT){
        console->cell_height = CELL_HEIGHT;
        console->title_lines = 0;
        console->status_lines = 1;
    } else {
        console->cell_height = COMPACT_CELL_HEIGHT;
        console->title_lines = 0;
        console->status_lines = 1;
    }

    // 棋盘行数 = 9 行棋格 + 横线行数：完整布局是每行之间一条线（10 条），
    // 紧凑布局只在最上和最下各画一条边框（2 条）
    int border_lines = (console->cell_height == CELL_HEIGHT) ? BOARD_SIZE + 1 : 2;
    console->board_height = BOARD_SIZE + border_lines;
    console->frame_width = BOARD_CHAR_WIDTH;
    console->frame_height = console->title_lines + console->board_height + console->status_lines;
    console->frame_x = (view_width - console->frame_width) / 2;
    console->frame_y = (view_height - console->frame_height) / 2;
    if (console->frame_x < 0){
        console->frame_x = 0;
    }
    if (console->frame_y < 0){
        console->frame_y = 0;
    }
    console->board_x = console->frame_x;
    console->board_y = console->frame_y + console->title_lines;
}

static void console_clear(const GameConsole* console)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(console->h_output, &info)){
        return;
    }
    DWORD total = (DWORD)info.dwSize.X * (DWORD)info.dwSize.Y;
    COORD home = {0, 0};
    DWORD written = 0;
    FillConsoleOutputCharacterW(console->h_output, L' ', total, home, &written);
    FillConsoleOutputAttribute(console->h_output, console->saved_attributes, total, home, &written);
    SetConsoleCursorPosition(console->h_output, home);
}

/* ---------- 对外接口 ---------- */

bool console_init(GameConsole* console)
{
    memset(console, 0, sizeof *console);
    console->hover_row = -1;
    console->hover_col = -1;

    console->h_output = GetStdHandle(STD_OUTPUT_HANDLE);
    console->h_input = GetStdHandle(STD_INPUT_HANDLE);
    if (console->h_output == INVALID_HANDLE_VALUE || console->h_input == INVALID_HANDLE_VALUE){
        return false;
    }
    // 拿不到控制台模式说明输出被重定向了，这个游戏必须在真正的终端里运行
    if (!GetConsoleMode(console->h_output, &console->saved_output_mode) ||
        !GetConsoleMode(console->h_input, &console->saved_input_mode)){
        return false;
    }
    GetConsoleCursorInfo(console->h_output, &console->saved_cursor);

    CONSOLE_SCREEN_BUFFER_INFO info;
    if (!GetConsoleScreenBufferInfo(console->h_output, &info)){
        return false;
    }
    console->saved_attributes = info.wAttributes;

    console_home_viewport(console);

    int view_width = 0;
    int view_height = 0;
    if (!console_view_size(console, &view_width, &view_height)){
        return false;
    }
    // 窗口太小就明确告诉用户差多少，而不是画出一个被裁掉一半的棋盘
    if (view_width < BOARD_CHAR_WIDTH || view_height < MIN_FRAME_HEIGHT){
        fprintf(stderr,
                "控制台窗口太小：当前 %d 列 × %d 行，本游戏至少需要 %d 列 × %d 行。\n"
                "请把窗口拉大（或按 Ctrl + 鼠标滚轮把字号调小）后重新运行；\n"
                "窗口高度达到 %d 行可以显示完整网格，达到 %d 行还能看到标题和分行状态。\n",
                view_width, view_height, BOARD_CHAR_WIDTH, MIN_FRAME_HEIGHT,
                LAYOUT_COMPACT_HEIGHT, LAYOUT_FULL_HEIGHT);
        return false;
    }
    console_layout(console, view_width, view_height);

    // 开启鼠标输入；同时关闭“快速编辑”，否则鼠标按下会被窗口用于选择文本，收不到点击事件
    DWORD input_mode = console->saved_input_mode;
    input_mode |= ENABLE_MOUSE_INPUT | ENABLE_EXTENDED_FLAGS;
    input_mode &= ~(DWORD)ENABLE_QUICK_EDIT_MODE;
#ifdef ENABLE_VIRTUAL_TERMINAL_INPUT
    input_mode &= ~(DWORD)ENABLE_VIRTUAL_TERMINAL_INPUT;
#endif
    SetConsoleMode(console->h_input, input_mode);

    // 隐藏光标，避免光标在棋盘上闪烁
    CONSOLE_CURSOR_INFO hidden = {25, FALSE};
    SetConsoleCursorInfo(console->h_output, &hidden);

    console_clear(console);
    return true;
}

void console_shutdown(GameConsole* console)
{
    // 光标移到整帧下方，恢复原来的颜色、光标和输入输出模式
    COORD position = {(SHORT)console->frame_x, (SHORT)(console->frame_y + console->frame_height)};
    SetConsoleCursorPosition(console->h_output, position);
    SetConsoleTextAttribute(console->h_output, console->saved_attributes);
    SetConsoleCursorInfo(console->h_output, &console->saved_cursor);
    SetConsoleMode(console->h_input, console->saved_input_mode);
    SetConsoleMode(console->h_output, console->saved_output_mode);
}

void console_draw(const GameConsole* console, const Game* game)
{
    // 整帧先画在内存缓冲区里，再用 WriteConsoleOutput 一次性刷到控制台，避免闪烁
    CHAR_INFO frame[MAX_FRAME_WIDTH * MAX_FRAME_HEIGHT];
    frame_fill(frame, L' ', ATTR_EMPTY);
    frame_draw_title(frame, console);
    frame_draw_board(frame, console, game);
    frame_draw_status(frame, console, game);

    COORD buffer_size = {(SHORT)console->frame_width, (SHORT)console->frame_height};
    COORD buffer_origin = {0, 0};
    SMALL_RECT region = {
        (SHORT)console->frame_x, (SHORT)console->frame_y,
        (SHORT)(console->frame_x + console->frame_width - 1),
        (SHORT)(console->frame_y + console->frame_height - 1)
    };
    WriteConsoleOutputW(console->h_output, frame, buffer_size, buffer_origin, &region);
}

bool console_screen_to_board(const GameConsole* console, int screen_x, int screen_y,
                             int* row, int* col)
{
    int offset_x = screen_x - console->board_x;
    int offset_y = screen_y - console->board_y - 1;  // 减掉棋盘最上面那条边框
    if (offset_x < 0 || offset_y < 0){
        return false;  // 落在棋盘左上方之外
    }

    // 横向：每 CELL_WIDTH 列一格，余数为 0 说明正好点在竖网格线上
    int board_col = offset_x / CELL_WIDTH;
    if (board_col >= BOARD_SIZE || offset_x % CELL_WIDTH == 0){
        return false;
    }

    int board_row = 0;
    if (console->cell_height == CELL_HEIGHT){
        if (offset_y % CELL_HEIGHT != 0){
            return false;  // 落在横网格线上
        }
        board_row = offset_y / CELL_HEIGHT;
    } else {
        board_row = offset_y;  // 紧凑布局：每行就是一个棋格
    }
    if (board_row >= BOARD_SIZE){
        return false;  // 落在棋盘下边框或更下面
    }

    *row = board_row;
    *col = board_col;
    return true;
}

void console_set_hint(GameConsole* console, const char* text)
{
    snprintf(console->hint, sizeof console->hint, "%s", text);
}

bool console_wait_left_click(GameConsole* console, const Game* game, int* row, int* col)
{
    bool was_pressed = false;  // 上一次事件里左键是否处于按下状态
    for (;;){
        INPUT_RECORD record;
        DWORD read_count = 0;
        // 阻塞式读取一条输入事件（鼠标或键盘）
        if (!ReadConsoleInputW(console->h_input, &record, 1, &read_count)){
            return false;
        }
        if (read_count == 0){
            continue;
        }

        if (record.EventType == KEY_EVENT){
            const KEY_EVENT_RECORD* key = &record.Event.KeyEvent;
            if (key->bKeyDown &&
                (key->wVirtualKeyCode == VK_ESCAPE || key->wVirtualKeyCode == 'Q')){
                return false;  // 玩家主动退出
            }
            continue;
        }
        if (record.EventType != MOUSE_EVENT){
            continue;
        }

        const MOUSE_EVENT_RECORD* mouse = &record.Event.MouseEvent;
        int mouse_row = -1;
        int mouse_col = -1;
        bool on_cell = console_screen_to_board(console, mouse->dwMousePosition.X,
                                               mouse->dwMousePosition.Y,
                                               &mouse_row, &mouse_col);

        // 只在左键“从松开变成按下”的那一刻算一次点击：
        // 按住左键拖动时不会再重复落子，双击、滚轮也不会被误判成落子
        bool pressed = (mouse->dwButtonState & FROM_LEFT_1ST_BUTTON_PRESSED) != 0;
        bool just_pressed = pressed && !was_pressed;
        was_pressed = pressed;

        if ((mouse->dwEventFlags & MOUSE_MOVED) && !just_pressed){
            // 鼠标移动：只更新悬停高亮和提示，不改变游戏状态
            if (mouse_row != console->hover_row || mouse_col != console->hover_col){
                console->hover_row = mouse_row;
                console->hover_col = mouse_col;
                if (on_cell && game->cells[mouse_row][mouse_col] != EMPTY_CELL){
                    console_set_hint(console, "该棋格已有棋子，不能重复落子");
                } else {
                    console_set_hint(console, "");
                }
                console_draw(console, game);
            }
            continue;
        }

        if (just_pressed){
            console->hover_row = -1;  // 落子后取消悬停高亮
            console->hover_col = -1;
            if (!on_cell){
                console_set_hint(console, "点击无效：请点在棋格内部");
                console_draw(console, game);
                continue;
            }
            *row = mouse_row;
            *col = mouse_col;
            console_set_hint(console, "");
            return true;
        }
    }
}

bool console_wait_restart(const GameConsole* console)
{
    for (;;){
        INPUT_RECORD record;
        DWORD read_count = 0;
        if (!ReadConsoleInputW(console->h_input, &record, 1, &read_count)){
            return false;
        }
        if (read_count == 0){
            continue;
        }
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown){
            continue;
        }
        WCHAR ch = record.Event.KeyEvent.uChar.UnicodeChar;
        return (ch == L'R' || ch == L'r');
    }
}
