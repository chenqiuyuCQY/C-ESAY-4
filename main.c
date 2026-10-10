#include "game.h"
#include "console.h"

#include <stdio.h>
#include <windows.h>

/* 模块分工：
   game.c    —— 棋盘数据与规则（初始化、落子、胜负、平局）
   console.c —— 控制台相关（鼠标事件、坐标换算、绘制刷新）
   main.c    —— 初始化 + 主游戏循环 + 收尾
*/
int main(void)
{
    SetConsoleOutputCP(CP_UTF8);  // 让 printf 输出的中文不乱码，我的编译器问题

    GameConsole console;
    if (!console_init(&console)){
        fprintf(stderr, "控制台初始化失败：请按上面的提示处理，或换用真正的终端窗口"
                        "(cmd / PowerShell / Windows Terminal)重新运行。\n");
        return 1;
    }

    for (;;){
        Game game;
        game_init(&game);              // 初始化棋盘：全空、X 先手、0 步、进行中
        console_draw(&console, &game); // 画出空棋盘和底部状态

        bool quit = false;
        // 主循环：等待一次有效的鼠标格子点击并落子，直到游戏结束
        while (game.status == GAME_RUNNING){
            int row = 0;
            int col = 0;
            if (!console_wait_left_click(&console, &game, &row, &col)){
                quit = true;  // 玩家按 Esc / Q 主动退出
                break;
            }

            MoveResult result = game_place(&game, row, col);  // 落子并内部判定胜负
            if (result == MOVE_OCCUPIED){
                console_set_hint(&console, "该棋格已有棋子，不能重复落子");
            } else if (result == MOVE_OUT_OF_BOUNDS){
                console_set_hint(&console, "落子位置在棋盘之外");
            } else {
                console_set_hint(&console, "");
            }
            console_draw(&console, &game);  // 重新绘制棋盘和底部状态
        }

        if (quit){
            break;
        }
        // 游戏结束后可以按 R 重开一局
        if (!console_wait_restart(&console)){
            break;
        }
    }

    console_shutdown(&console);  // 恢复输入模式、光标和颜色
    printf("已退出五子棋游戏。\n");
    return 0;
}
