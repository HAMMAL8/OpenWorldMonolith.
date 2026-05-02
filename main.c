#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdlib.h>
#include <string.h>

PSP_MODULE_INFO("OpenWorldMonolith", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define MAX_PROJECTILES 5
#define MAX_ENEMIES 3
#define MAX_PLAYER_HP 5
#define KILLS_TO_WIN 10
#define WORLD_WIDTH 480
#define WORLD_HEIGHT 272
#define FIRE_COOLDOWN 20
#define RESPAWN_TIME 180

typedef struct {
    int x, y, active;
} Projectile;

typedef struct {
    int x, y, hp, alive, dir, hitTimer, respawnTimer, initX, initY;
} Enemy;

typedef struct {
    int x, y, hp, invincibleTimer;
} Player;

// Globals
Player player;
Projectile shots[MAX_PROJECTILES];
Enemy enemies[MAX_ENEMIES];
int score = 0;
int fireCooldown = 0;
int gameState = 0;

// PSP Exit Handlers
int exit_callback(int arg1, int arg2, void *common) {
    sceKernelExitGame();
    return 0;
}

int CallbackThread(SceSize args, void *argp) {
    int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    sceKernelSleepThreadCB();
    return 0;
}

int SetupCallbacks(void) {
    int thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0) sceKernelStartThread(thid, 0, 0);
    return thid;
}

// Initialize game state
void initGame() {
    player.x = 240;
    player.y = 136;
    player.hp = MAX_PLAYER_HP;
    player.invincibleTimer = 0;

    memset(shots, 0, sizeof(shots));

    enemies[0].x = 100;
    enemies[0].y = 100;
    enemies[0].initX = 100;
    enemies[0].initY = 100;
    enemies[0].hp = 3;
    enemies[0].alive = 1;
    enemies[0].dir = 1;
    enemies[0].hitTimer = 0;
    enemies[0].respawnTimer = 0;

    enemies[1].x = 350;
    enemies[1].y = 180;
    enemies[1].initX = 350;
    enemies[1].initY = 180;
    enemies[1].hp = 3;
    enemies[1].alive = 1;
    enemies[1].dir = -1;
    enemies[1].hitTimer = 0;
    enemies[1].respawnTimer = 0;

    enemies[2].x = 200;
    enemies[2].y = 60;
    enemies[2].initX = 200;
    enemies[2].initY = 60;
    enemies[2].hp = 3;
    enemies[2].alive = 1;
    enemies[2].dir = 1;
    enemies[2].hitTimer = 0;
    enemies[2].respawnTimer = 0;

    score = 0;
    fireCooldown = 0;
    gameState = 0;
}

// Reset game state for restart
void resetGame() {
    initGame();
}

// Title screen
void showTitleScreen() {
    int blink = 0;
    while (1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(10, 7);
        pspDebugScreenPrintf("=== OpenWorldMonolith ===");

        if (blink < 30) {
            pspDebugScreenSetXY(13, 10);
            pspDebugScreenPrintf("Press START to Begin");
        }

        blink = (blink + 1) % 60;

        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;
        sceDisplayWaitVblankStart();
    }
    pspDebugScreenClear();
}

// Game Over screen
void showGameOverScreen() {
    int blink = 0;
    while (1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(15, 10);
        pspDebugScreenPrintf("=== GAME OVER ===");

        pspDebugScreenSetXY(10, 13);
        pspDebugScreenPrintf("Final Score: %d", score);

        if (blink < 30) {
            pspDebugScreenSetXY(11, 16);
            pspDebugScreenPrintf("Press START to Restart");
        }

        blink = (blink + 1) % 60;

        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) {
            resetGame();
            break;
        }
        sceDisplayWaitVblankStart();
    }
    pspDebugScreenClear();
}

// Win screen
void showWinScreen() {
    int blink = 0;
    while (1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(15, 10);
        pspDebugScreenPrintf("=== YOU WIN! ===");

        pspDebugScreenSetXY(10, 13);
        pspDebugScreenPrintf("All %d enemies defeated!", KILLS_TO_WIN);

        if (blink < 30) {
            pspDebugScreenSetXY(11, 16);
            pspDebugScreenPrintf("Press START to Play Again");
        }

        blink = (blink + 1) % 60;

        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) {
            resetGame();
            break;
        }
        sceDisplayWaitVblankStart();
    }
    pspDebugScreenClear();
}

// Game Systems
void fireShot() {
    if (fireCooldown > 0) return;

    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!shots[i].active) {
            shots[i].x = player.x + 8;
            shots[i].y = player.y;
            shots[i].active = 1;
            fireCooldown = FIRE_COOLDOWN;
            break;
        }
    }
}

void updateShots() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (shots[i].active) {
            shots[i].x += 5;
            if (shots[i].x > WORLD_WIDTH) shots[i].active = 0;

            for (int j = 0; j < MAX_ENEMIES; j++) {
                Enemy *e = &enemies[j];
                if (e->alive && shots[i].x > e->x - 4 && shots[i].x < e->x + 12 &&
                    shots[i].y > e->y - 4 && shots[i].y < e->y + 12) {
                    shots[i].active = 0;
                    e->hp--;
                    e->hitTimer = 30;
                    if (e->hp <= 0) {
                        e->alive = 0;
                        e->respawnTimer = RESPAWN_TIME;
                        score++;
                    }
                }
            }
        }
    }
}

void updateEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];

        if (!e->alive) {
            if (e->respawnTimer > 0) {
                e->respawnTimer--;
                if (e->respawnTimer <= 0) {
                    e->x = e->initX;
                    e->y = e->initY;
                    e->hp = 3;
                    e->alive = 1;
                }
            }
            continue;
        }

        e->x += e->dir * 2;
        if (e->x < 10 || e->x > 440) e->dir *= -1;

        if (abs(player.x - e->x) < 12 && abs(player.y - e->y) < 12 && player.invincibleTimer == 0) {
            player.hp--;
            player.invincibleTimer = 60;
        }

        if (e->hitTimer > 0) e->hitTimer--;
    }
}

void drawShots() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (shots[i].active) {
            int screenX = shots[i].x / 8;
            int screenY = shots[i].y / 8;
            if (screenX < 60 && screenY < 34) {
                pspDebugScreenSetXY(screenX, screenY);
                pspDebugScreenPrintf("-");
            }
        }
    }
}

void drawEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) continue;

        int screenX = e->x / 8;
        int screenY = e->y / 8;
        if (screenX >= 60 || screenY >= 34) continue;

        pspDebugScreenSetXY(screenX, screenY);
        pspDebugScreenPrintf("E");

        int hpBarX = (e->x - 8) / 8;
        int hpBarY = (e->y - 16) / 8;
        if (hpBarX >= 0 && hpBarX < 60 && hpBarY >= 0 && hpBarY < 34) {
            pspDebugScreenSetXY(hpBarX, hpBarY);
            for (int h = 0; h < e->hp; h++) pspDebugScreenPrintf("*");
        }

        if (e->hitTimer > 0) {
            int hitX = (e->x - 8) / 8;
            int hitY = (e->y + 16) / 8;
            if (hitX >= 0 && hitX < 60 && hitY >= 0 && hitY < 34) {
                pspDebugScreenSetXY(hitX, hitY);
                pspDebugScreenPrintf("Hit!");
            }
        }
    }
}

void drawPlayer() {
    if (player.invincibleTimer % 6 < 3 || player.invincibleTimer == 0) {
        int screenX = player.x / 8;
        int screenY = player.y / 8;
        if (screenX < 60 && screenY < 34) {
            pspDebugScreenSetXY(screenX, screenY);
            pspDebugScreenPrintf("P");
        }
    }
}

void drawHUD() {
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("HP: ");
    for (int i = 0; i < MAX_PLAYER_HP; i++) {
        if (i < player.hp) pspDebugScreenPrintf("\x03 ");
        else pspDebugScreenPrintf("- ");
    }
    pspDebugScreenPrintf(" Score: %d/%d", score, KILLS_TO_WIN);
}

void drawMiniMap() {
    int mapBaseX = 45;
    int mapBaseY = 0;

    pspDebugScreenSetXY(mapBaseX, mapBaseY);
    pspDebugScreenPrintf("Map:");

    int mapRow = mapBaseY + 1;
    pspDebugScreenSetXY(mapBaseX, mapRow);
    for (int i = 0; i < 12; i++) pspDebugScreenPrintf(".");

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            int pos = enemies[i].x * 12 / WORLD_WIDTH;
            int drawX = mapBaseX + pos;
            if (drawX < 60 && mapRow < 34) {
                pspDebugScreenSetXY(drawX, mapRow);
                pspDebugScreenPrintf("e");
            }
        }
    }

    int ppos = player.x * 12 / WORLD_WIDTH;
    int pDrawX = mapBaseX + ppos;
    if (pDrawX < 60 && mapRow < 34) {
        pspDebugScreenSetXY(pDrawX, mapRow);
        pspDebugScreenPrintf("P");
    }
}

// Game Loop
int main() {
    pspDebugScreenInit();
    SetupCallbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

    initGame();

    showTitleScreen();

    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_UP) player.y -= 2;
        if (pad.Buttons & PSP_CTRL_DOWN) player.y += 2;
        if (pad.Buttons & PSP_CTRL_LEFT) player.x -= 2;
        if (pad.Buttons & PSP_CTRL_RIGHT) player.x += 2;

        // Clamp player position to stay on screen and away from HUD
        if (player.x < 8) player.x = 8;
        if (player.x > 472) player.x = 472;
        if (player.y < 32) player.y = 32;
        if (player.y > 264) player.y = 264;

        if (pad.Buttons & PSP_CTRL_CROSS) fireShot();
        if (player.invincibleTimer > 0) player.invincibleTimer--;
        if (fireCooldown > 0) fireCooldown--;

        updateShots();
        updateEnemies();

        pspDebugScreenClear();
        drawHUD();
        drawPlayer();
        drawEnemies();
        drawShots();
        drawMiniMap();

        if (player.hp <= 0) {
            showGameOverScreen();
        } else if (score >= KILLS_TO_WIN) {
            showWinScreen();
        }

        sceDisplayWaitVblankStart();
    }

    return 0;
}
