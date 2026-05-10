#include <pspkernel.h>
#include <pspdebug.h>
#include <pspctrl.h>
#include <pspdisplay.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

PSP_MODULE_INFO("OpenWorldMonolith", 0, 1, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);

#define SCREEN_W 60
#define SCREEN_H 34
#define MAP_WIDTH 128
#define MAP_HEIGHT 128

#define MAX_VEHICLES 20
#define MAX_NPCS 15
#define MAX_PLAYER_HP 5
#define MAX_WANTED_LEVEL 5

#define TILE_GRASS '.'
#define TILE_ROAD '#'
#define TILE_BUILDING 'B'
#define TILE_BUILDING_ALT '='

#define VEHICLE_ENTER_DIST 2.0f
#define NPC_SPAWN_DIST 25

#define ROAD_SPACING 16
#define PLAYER_SPEED 0.3f
#define VEHICLE_SPEED 0.8f
#define NPC_SPEED 0.25f

#define WANTED_DECAY_TIME 300
#define WANTED_STAR_THRESHOLD 50

typedef struct {
    float x, y;
    int active;
    int type;
} Vehicle;

typedef struct {
    float x, y;
    int active;
    int type;
    int hp;
    int isPolice;
} NPC;

typedef struct {
    float x, y;
    int hp;
    int inVehicle;
    int vehicleIndex;
    int wantedLevel;
    int wantedScore;
    int wantedTimer;
} Player;

char worldMap[MAP_HEIGHT][MAP_WIDTH];
Player player;
Vehicle vehicles[MAX_VEHICLES];
NPC npcs[MAX_NPCS];
float cameraX, cameraY;
int gameState = 0;
int score = 0;
int frameCount = 0;

static char lineBuffer[SCREEN_W + 1];

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

int isSolid(int x, int y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return 1;
    return (worldMap[y][x] == TILE_BUILDING || worldMap[y][x] == TILE_BUILDING_ALT);
}

void initMap() {
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            worldMap[y][x] = TILE_GRASS;
        }
    }

    for (int y = 0; y < MAP_HEIGHT; y += ROAD_SPACING) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            worldMap[y][x] = TILE_ROAD;
            if (y + 1 < MAP_HEIGHT) worldMap[y + 1][x] = TILE_ROAD;
        }
    }

    for (int x = 0; x < MAP_WIDTH; x += ROAD_SPACING) {
        for (int y = 0; y < MAP_HEIGHT; y++) {
            worldMap[y][x] = TILE_ROAD;
            if (x + 1 < MAP_WIDTH) worldMap[y][x + 1] = TILE_ROAD;
        }
    }

    for (int y = 4; y < MAP_HEIGHT - 4; y += ROAD_SPACING) {
        for (int x = 4; x < MAP_WIDTH - 4; x += ROAD_SPACING) {
            int blockW = ROAD_SPACING - 4;
            int blockH = ROAD_SPACING - 4;
            
            for (int by = 0; by < blockH && (y + by) < MAP_HEIGHT; by++) {
                for (int bx = 0; bx < blockW && (x + bx) < MAP_WIDTH; bx++) {
                    if ((bx + by) % 3 == 0) {
                        worldMap[y + by][x + bx] = TILE_BUILDING;
                    } else {
                        worldMap[y + by][x + bx] = TILE_BUILDING_ALT;
                    }
                }
            }
        }
    }
}

void spawnVehicle(int index) {
    int attempts = 0;
    while (attempts < 100) {
        int x = rand() % MAP_WIDTH;
        int y = rand() % MAP_HEIGHT;
        
        if (worldMap[y][x] == TILE_ROAD) {
            int clear = 1;
            for (int i = 0; i < MAX_VEHICLES; i++) {
                if (i != index && vehicles[i].active) {
                    float dx = vehicles[i].x - x;
                    float dy = vehicles[i].y - y;
                    if (dx * dx + dy * dy < 9.0f) {
                        clear = 0;
                        break;
                    }
                }
            }
            
            if (clear) {
                vehicles[index].x = (float)x;
                vehicles[index].y = (float)y;
                vehicles[index].active = 1;
                vehicles[index].type = rand() % 3;
                return;
            }
        }
        attempts++;
    }
    vehicles[index].active = 0;
}

void spawnNPC(int index, int forcePolice) {
    npcs[index].active = 1;
    npcs[index].hp = 2;
    npcs[index].isPolice = forcePolice;
    
    float angle = (float)(rand() % 360) * 3.14159f / 180.0f;
    float dist = NPC_SPAWN_DIST + (rand() % 10);
    
    npcs[index].x = player.x + cosf(angle) * dist;
    npcs[index].y = player.y + sinf(angle) * dist;
    
    npcs[index].x = fmaxf(2.0f, fminf(MAP_WIDTH - 2, npcs[index].x));
    npcs[index].y = fmaxf(2.0f, fminf(MAP_HEIGHT - 2, npcs[index].y));
    
    if (isSolid((int)npcs[index].x, (int)npcs[index].y)) {
        npcs[index].active = 0;
    }
}

void updateWantedLevel() {
    if (player.wantedScore > 0) {
        player.wantedTimer++;
        if (player.wantedTimer >= WANTED_DECAY_TIME) {
            player.wantedScore = 0;
            player.wantedLevel = 0;
            player.wantedTimer = 0;
        }
    }
    
    int newLevel = player.wantedScore / WANTED_STAR_THRESHOLD;
    if (newLevel > MAX_WANTED_LEVEL) newLevel = MAX_WANTED_LEVEL;
    player.wantedLevel = newLevel;
    
    if (player.wantedLevel > 0) {
        int targetPolice = player.wantedLevel * 2;
        int activePolice = 0;
        
        for (int i = 0; i < MAX_NPCS; i++) {
            if (npcs[i].active && npcs[i].isPolice) activePolice++;
        }
        
        for (int i = 0; i < MAX_NPCS && activePolice < targetPolice; i++) {
            if (!npcs[i].active) {
                spawnNPC(i, 1);
                activePolice++;
            }
        }
    }
}

void updateNPCs() {
    for (int i = 0; i < MAX_NPCS; i++) {
        if (!npcs[i].active) continue;
        
        if (npcs[i].isPolice && player.wantedLevel > 0) {
            float dx = player.x - npcs[i].x;
            float dy = player.y - npcs[i].y;
            float dist = sqrtf(dx * dx + dy * dy);
            
            if (dist > 0.1f) {
                float speed = NPC_SPEED + (player.wantedLevel * 0.05f);
                float nx = npcs[i].x + (dx / dist) * speed;
                float ny = npcs[i].y + (dy / dist) * speed;
                
                if (!isSolid((int)nx, (int)npcs[i].y)) {
                    npcs[i].x = nx;
                }
                if (!isSolid((int)npcs[i].x, (int)ny)) {
                    npcs[i].y = ny;
                }
            }
            
            if (dist < 1.5f) {
                player.hp--;
                player.wantedTimer = 0;
            }
        } else {
            npcs[i].x += ((rand() % 3) - 1) * 0.1f;
            npcs[i].y += ((rand() % 3) - 1) * 0.1f;
        }
        
        float camDistX = npcs[i].x - cameraX - SCREEN_W / 2;
        float camDistY = npcs[i].y - cameraY - SCREEN_H / 2;
        if (fabsf(camDistX) > SCREEN_W || fabsf(camDistY) > SCREEN_H) {
            if (!npcs[i].isPolice) {
                npcs[i].active = 0;
            }
        }
    }
}

void initGame() {
    initMap();
    
    int startX = ROAD_SPACING + 2;
    int startY = ROAD_SPACING + 2;
    while (isSolid(startX, startY) && startX < MAP_WIDTH) {
        startX++;
        if (isSolid(startX, startY)) {
            startX = ROAD_SPACING + 2;
            startY++;
        }
    }
    
    player.x = (float)startX;
    player.y = (float)startY;
    player.hp = MAX_PLAYER_HP;
    player.inVehicle = 0;
    player.vehicleIndex = -1;
    player.wantedLevel = 0;
    player.wantedScore = 0;
    player.wantedTimer = 0;
    
    for (int i = 0; i < MAX_VEHICLES; i++) {
        spawnVehicle(i);
    }
    
    for (int i = 0; i < MAX_NPCS; i++) {
        npcs[i].active = 0;
    }
    
    score = 0;
    frameCount = 0;
    gameState = 0;
}

void resetGame() {
    initGame();
}

void updateCamera() {
    cameraX = player.x - SCREEN_W / 2;
    cameraY = player.y - SCREEN_H / 2;
    
    if (cameraX < 0) cameraX = 0;
    if (cameraY < 0) cameraY = 0;
    if (cameraX > MAP_WIDTH - SCREEN_W) cameraX = MAP_WIDTH - SCREEN_W;
    if (cameraY > MAP_HEIGHT - SCREEN_H) cameraY = MAP_HEIGHT - SCREEN_H;
}

char getVehicleChar(int type) {
    switch (type) {
        case 0: return 'v';
        case 1: return 'V';
        default: return 'c';
    }
}

char getNPCChar(NPC *npc) {
    if (npc->isPolice) return 'E';
    return 'N';
}

void drawFrame() {
    for (int row = 0; row < SCREEN_H; row++) {
        int mapY = (int)(cameraY + row);
        if (mapY < 0 || mapY >= MAP_HEIGHT) {
            memset(lineBuffer, ' ', SCREEN_W);
            lineBuffer[SCREEN_W] = '\0';
        } else {
            for (int col = 0; col < SCREEN_W; col++) {
                int mapX = (int)(cameraX + col);
                if (mapX < 0 || mapX >= MAP_WIDTH) {
                    lineBuffer[col] = ' ';
                } else {
                    lineBuffer[col] = worldMap[mapY][mapX];
                }
            }
            lineBuffer[SCREEN_W] = '\0';
        }
        
        pspDebugScreenSetXY(0, row);
        pspDebugScreenPrintf("%s", lineBuffer);
    }
    
    for (int i = 0; i < MAX_VEHICLES; i++) {
        if (!vehicles[i].active) continue;
        
        int screenX = (int)(vehicles[i].x - cameraX);
        int screenY = (int)(vehicles[i].y - cameraY);
        
        if (screenX >= 0 && screenX < SCREEN_W && screenY >= 0 && screenY < SCREEN_H) {
            pspDebugScreenSetXY(screenX, screenY);
            pspDebugScreenPrintf("%c", getVehicleChar(vehicles[i].type));
        }
    }
    
    for (int i = 0; i < MAX_NPCS; i++) {
        if (!npcs[i].active) continue;
        
        int screenX = (int)(npcs[i].x - cameraX);
        int screenY = (int)(npcs[i].y - cameraY);
        
        if (screenX >= 0 && screenX < SCREEN_W && screenY >= 0 && screenY < SCREEN_H) {
            pspDebugScreenSetXY(screenX, screenY);
            pspDebugScreenPrintf("%c", getNPCChar(&npcs[i]));
        }
    }
    
    int playerScreenX = (int)(player.x - cameraX);
    int playerScreenY = (int)(player.y - cameraY);
    
    if (playerScreenX >= 0 && playerScreenX < SCREEN_W && playerScreenY >= 0 && playerScreenY < SCREEN_H) {
        pspDebugScreenSetXY(playerScreenX, playerScreenY);
        if (player.inVehicle) {
            pspDebugScreenPrintf("@");
        } else {
            pspDebugScreenPrintf("P");
        }
    }
}

void drawHUD() {
    pspDebugScreenSetXY(0, 0);
    pspDebugScreenPrintf("HP:");
    for (int i = 0; i < MAX_PLAYER_HP; i++) {
        if (i < player.hp) pspDebugScreenPrintf("\x03");
        else pspDebugScreenPrintf("-");
    }
    
    pspDebugScreenSetXY(15, 0);
    if (player.wantedLevel > 0) {
        pspDebugScreenPrintf("[");
        for (int i = 0; i < player.wantedLevel; i++) {
            pspDebugScreenPrintf("*");
        }
        pspDebugScreenPrintf("]");
    }
    
    pspDebugScreenSetXY(35, 0);
    if (player.inVehicle) {
        pspDebugScreenPrintf("[IN CAR]");
    } else {
        pspDebugScreenPrintf("        ");
    }
}

void handleInput(SceCtrlData *pad) {
    float speed = player.inVehicle ? VEHICLE_SPEED : PLAYER_SPEED;
    float newX = player.x;
    float newY = player.y;
    
    if (pad->Buttons & PSP_CTRL_UP) newY -= speed;
    if (pad->Buttons & PSP_CTRL_DOWN) newY += speed;
    if (pad->Buttons & PSP_CTRL_LEFT) newX -= speed;
    if (pad->Buttons & PSP_CTRL_RIGHT) newX += speed;
    
    if (newX < 1) newX = 1;
    if (newX > MAP_WIDTH - 2) newX = MAP_WIDTH - 2;
    if (newY < 1) newY = 1;
    if (newY > MAP_HEIGHT - 2) newY = MAP_HEIGHT - 2;
    
    int canMove = 1;
    if (isSolid((int)newX, (int)player.y)) {
        canMove = 0;
    }
    if (isSolid((int)player.x, (int)newY)) {
        canMove = 0;
    }
    
    if (canMove) {
        player.x = newX;
        player.y = newY;
    }
    
    static int prevCircle = 0;
    int circlePressed = (pad->Buttons & PSP_CTRL_CIRCLE) && !prevCircle;
    prevCircle = (pad->Buttons & PSP_CTRL_CIRCLE) ? 1 : 0;
    
    if (circlePressed) {
        if (player.inVehicle) {
            player.inVehicle = 0;
            vehicles[player.vehicleIndex].active = 1;
            vehicles[player.vehicleIndex].x = player.x;
            vehicles[player.vehicleIndex].y = player.y;
            player.vehicleIndex = -1;
            
            int attempts = 0;
            while (isSolid((int)player.x, (int)player.y) && attempts < 4) {
                if (!isSolid((int)player.x + 1, (int)player.y)) player.x += 1;
                else if (!isSolid((int)player.x - 1, (int)player.y)) player.x -= 1;
                else if (!isSolid((int)player.x, (int)player.y + 1)) player.y += 1;
                else if (!isSolid((int)player.x, (int)player.y - 1)) player.y -= 1;
                attempts++;
            }
        } else {
            for (int i = 0; i < MAX_VEHICLES; i++) {
                if (vehicles[i].active) {
                    float dx = vehicles[i].x - player.x;
                    float dy = vehicles[i].y - player.y;
                    float dist = sqrtf(dx * dx + dy * dy);
                    
                    if (dist < VEHICLE_ENTER_DIST) {
                        player.inVehicle = 1;
                        player.vehicleIndex = i;
                        vehicles[i].active = 0;
                        break;
                    }
                }
            }
        }
    }
}

void showTitleScreen() {
    int blink = 0;
    while (1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(15, 10);
        pspDebugScreenPrintf("=== GTA MONOLITH ===");
        
        pspDebugScreenSetXY(10, 13);
        pspDebugScreenPrintf("Open World Action Game");
        
        pspDebugScreenSetXY(8, 16);
        pspDebugScreenPrintf("D-Pad: Move  O: Enter/Exit Car");
        
        if (blink < 30) {
            pspDebugScreenSetXY(12, 20);
            pspDebugScreenPrintf("Press START to Begin");
        }
        
        blink = (blink + 1) % 60;
        
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        if (pad.Buttons & PSP_CTRL_START) break;
        sceDisplayWaitVblankStart();
    }
}

void showGameOverScreen() {
    int blink = 0;
    while (1) {
        pspDebugScreenClear();
        pspDebugScreenSetXY(18, 12);
        pspDebugScreenPrintf("WASTED");
        
        pspDebugScreenSetXY(12, 15);
        pspDebugScreenPrintf("Score: %d", score);
        
        if (blink < 30) {
            pspDebugScreenSetXY(10, 18);
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
}

int main() {
    pspDebugScreenInit();
    SetupCallbacks();
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    
    srand((unsigned int)sceKernelGetSystemTimeLow());
    
    initGame();
    showTitleScreen();
    
    while (1) {
        SceCtrlData pad;
        sceCtrlReadBufferPositive(&pad, 1);
        
        handleInput(&pad);
        updateCamera();
        updateNPCs();
        updateWantedLevel();
        
        if (pad.Buttons & PSP_CTRL_SQUARE) {
            player.wantedScore += 10;
            player.wantedTimer = 0;
        }
        
        drawFrame();
        drawHUD();
        
        if (player.hp <= 0) {
            showGameOverScreen();
        }
        
        frameCount++;
        sceDisplayWaitVblankStart();
    }
    
    return 0;
}
