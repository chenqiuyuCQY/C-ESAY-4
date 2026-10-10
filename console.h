#ifndef CONSOLE_H
#define CONSOLE_H

#include <stdbool.h>
#include <windows.h>

#include "game.h"

/* 棋盘在字符坐标下的尺寸：
   每个棋格 3 个字符宽（内部）+ 1 条竖线，高度 1 行 + 1 条横线 */
#define CELL_WIDTH 4                 // 棋格宽度（字符格）
#define CELL_HEIGHT 2                // 完整布局下棋格高度（字符行）
#define COMPACT_CELL_HEIGHT 1        // 窗口太矮时的紧凑棋格高度
#define BOARD_CHAR_WIDTH  (CELL_WIDTH * BOARD_SIZE + 1)            // 棋盘总宽 37 = 9*4+1
#define BOARD_FULL_HEIGHT (CELL_HEIGHT * BOARD_SIZE + 1)           // 完整棋盘高 19 = 9*2+1
#define BOARD_COMPACT_HEIGHT (COMPACT_CELL_HEIGHT * BOARD_SIZE + 2) // 紧凑棋盘高 11 = 9 行 + 上下边框

/* 三种布局所需的窗口高度（行）：
   完整 = 标题 2 + 棋盘 19 + 状态 4 = 25
   紧凑 = 棋盘 19 + 状态 1 = 20（去掉标题，状态压成一行）
   迷你 = 棋盘 11 + 状态 1 = 12（棋格压成一行，用于很矮的终端窗口） */
#define LAYOUT_FULL_HEIGHT    (2 + BOARD_FULL_HEIGHT + 4)
#define LAYOUT_COMPACT_HEIGHT (BOARD_FULL_HEIGHT + 1)
#define LAYOUT_MINI_HEIGHT    (BOARD_COMPACT_HEIGHT + 1)
#define MIN_FRAME_HEIGHT      LAYOUT_MINI_HEIGHT

typedef struct {
    HANDLE h_output;                   // 标准输出句柄
    HANDLE h_input;                    // 标准输入句柄
    DWORD saved_output_mode;           // 进入游戏前的输出模式
    DWORD saved_input_mode;            // 进入游戏前的输入模式
    CONSOLE_CURSOR_INFO saved_cursor;  // 进入游戏前的光标信息
    WORD saved_attributes;             // 进入游戏前的文字颜色

    int cell_height;                   // 当前棋格占用的行数：2 = 完整网格，1 = 紧凑网格
    int title_lines;                   // 标题区行数（0 表示紧凑布局没有标题）
    int status_lines;                  // 状态区行数（4 或 1）
    int board_height;                  // 棋盘区行数（随 cell_height 变化）
    int frame_width, frame_height;     // 整帧（标题 + 棋盘 + 状态区）的尺寸
    int frame_x, frame_y;              // 整帧左上角的字符坐标
    int board_x, board_y;              // 棋盘左上角（第一个 '+'）的字符坐标

    int hover_row, hover_col;          // 鼠标悬停的棋盘坐标，-1 表示不在棋盘内
    char hint[160];                    // 棋盘下方的操作提示（UTF-8）
} GameConsole;

bool console_init(GameConsole* console);
// 保存控制台原状态，开启鼠标输入、隐藏光标，按窗口大小选择布局并居中摆放棋盘

void console_shutdown(GameConsole* console);
// 恢复输入模式、光标和文字颜色

void console_draw(const GameConsole* console, const Game* game);
// 绘制“标题 + 棋盘 + 当前执子 + 游戏状态”整帧

bool console_wait_left_click(GameConsole* console, const Game* game, int* row, int* col);
// 等待一次落在棋格内部的鼠标左键点击，返回棋盘坐标；返回 false 表示玩家要求退出

bool console_wait_restart(const GameConsole* console);
// 游戏结束后等待按键，返回 true 表示按了 R 再来一局

bool console_screen_to_board(const GameConsole* console, int screen_x, int screen_y,
                             int* row, int* col);
// 把鼠标的字符坐标换算成棋盘坐标；点击落在网格线或棋盘外时返回 false

void console_set_hint(GameConsole* console, const char* text);
// 设置棋盘下方的提示文字（UTF-8），空字符串表示不提示

#endif
