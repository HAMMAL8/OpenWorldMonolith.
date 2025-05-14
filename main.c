#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <string.h>

PSP_MODULE_INFO("OpenWorldMonolith", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define MAX_PROJECTILES 5
#define MAX_ENEMIES 3
#define MAX_PLAYER_HP 5
#define WORLD_WIDTH 480
#define WORLD_HEIGHT 272

typedef struct {
    int x, y, active;
} Projectile;

typedef struct {
    int x, y, hp, alive, dir, hitTimer;
} Enemy;

typedef struct {
    int x, y, hp, invincibleTimer;
} Player;

// Globals
Player player = {240, 136, MAX_PLAYER_HP, 0};
Projectile shots[MAX_PROJECTILES];
Enemy enemies[MAX_ENEMIES] = {
    {100, 100, 3, 1, 1, 0},
    {350, 180, 3, 1, -1, 0},
    {200, 60, 3, 1, 1, 0}
};

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

// Game Systems
void fireShot() {
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        if (!shots[i].active) {
            shots[i].x = player.x + 8;
            shots[i].y = player.y;
            shots[i].active = 1;
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
                    if (e->hp <= 0) e->alive = 0;
                }
            }
        }
    }
}

void updateEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) continue;

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
            pspDebugScreenSetXY(shots[i].x / 8, shots[i].y / 8);
            pspDebugScreenPrintf("-");
        }
    }
}

void drawEnemies() {
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy *e = &enemies[i];
        if (!e->alive) continue;

        pspDebugScreenSetXY(e->x / 8, e->y / 8);
        pspDebugScreenPrintf("E");

        pspDebugScreenSetXY((e->x - 8) / 8, (e->y - 16) / 8);
        for (int h = 0; h < e->hp; h++) pspDebugScreenPrintf("*");

        if (e->hitTimer > 0) {
            pspDebugScreenSetXY((e->x - 8) / 8, (e->y + 16) / 8);
            pspDebugScreenPrintf("Hit!");
        }
    }
}

void drawPlayer() {
    if (player.invincibleTimer % 6 < 3 || player.invincibleTimer == 0) {
        pspDebugScreenSetXY(player.x / 8, player.y / 8);
        pspDebugScreenPrintf("P");
    }
}

void drawHUD() {
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("HP: ");
    for (int i = 0; i < MAX_PLAYER_HP; i++) {
        if (i < player.hp) pspDebugScreenPrintf("\x03 ");
        else pspDebugScreenPrintf("- ");
    }
}

void drawMiniMap() {
    pspDebugScreenSetXY(60, 0);
    pspDebugScreenPrintf("Map:");
    pspDebugScreenSetXY(60, 1);
    for (int i = 0; i < 10; i++) pspDebugScreenPrintf(".");

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].alive) {
            int pos = enemies[i].x * 10 / WORLD_WIDTH;
            pspDebugScreenSetXY(60 + pos, 1);
            pspDebugScreenPrintf("e");
        }
    }

    int ppos = player.x * 10 / WORLD_WIDTH;
    pspDebugScreenSetXY(60 + ppos, 1);
    pspDebugScreenPrintf("P");
}

// Game Loop
int main() {
    pspDebugScreenInit();
    SetupCallbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    memset(shots, 0, sizeof(shots));

    showTitleScreen();

    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);

        if (pad.Buttons & PSP_CTRL_UP) player.y -= 2;
        if (pad.Buttons & PSP_CTRL_DOWN) player.y += 2;
        if (pad.Buttons & PSP_CTRL_LEFT) player.x -= 2;
        if (pad.Buttons & PSP_CTRL_RIGHT) player.x += 2;

        if (pad.Buttons & PSP_CTRL_CROSS) fireShot();
        if (player.invincibleTimer > 0) player.invincibleTimer--;

        updateShots();
        updateEnemies();

        pspDebugScreenClear();
        drawHUD();
        drawPlayer();
        drawEnemies();
        drawShots();
        drawMiniMap();

        if (player.hp <= 0) {
            pspDebugScreenSetXY(15, 15);
            pspDebugScreenPrintf("GAME OVER");
        }

        sceDisplayWaitVblankStart();
    }

    return 0;
}
