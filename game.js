// GTA Monolith - HTML5 Canvas Port
// Ported from PSP C version

const canvas = document.getElementById('gameCanvas');
const ctx = canvas.getContext('2d');

// Game Constants
const SCREEN_W = 60;
const SCREEN_H = 34;
const MAP_WIDTH = 128;
const MAP_HEIGHT = 128;

const MAX_VEHICLES = 20;
const MAX_NPCS = 15;
const MAX_PLAYER_HP = 5;
const MAX_WANTED_LEVEL = 5;

const TILE_SIZE = 16;

const TILE_GRASS = 0;
const TILE_ROAD = 1;
const TILE_BUILDING = 2;
const TILE_BUILDING_ALT = 3;

const VEHICLE_ENTER_DIST = 2.0;
const NPC_SPAWN_DIST = 25;

const ROAD_SPACING = 16;
const PLAYER_SPEED = 0.3;
const VEHICLE_SPEED = 0.8;
const NPC_SPEED = 0.25;

const WANTED_DECAY_TIME = 300;
const WANTED_STAR_THRESHOLD = 50;

// Tile colors
const COLORS = {
    [TILE_GRASS]: '#2d5016',
    [TILE_ROAD]: '#444444',
    [TILE_BUILDING]: '#8b4513',
    [TILE_BUILDING_ALT]: '#a0522d'
};

// Game State
const STATE_TITLE = 0;
const STATE_PLAYING = 1;
const STATE_GAMEOVER = 2;

// Input handling
const keys = {};
let prevEnter = false;
let prevSpace = false;

// Game data
let worldMap = [];
let player = {};
let vehicles = [];
let npcs = [];
let bullets = [];
let cameraX = 0;
let cameraY = 0;
let gameState = STATE_TITLE;
let score = 0;
let frameCount = 0;
let blinkTimer = 0;

// Initialize arrays
function initArrays() {
    worldMap = new Array(MAP_HEIGHT).fill(null).map(() => new Array(MAP_WIDTH).fill(TILE_GRASS));
    vehicles = new Array(MAX_VEHICLES).fill(null).map(() => ({
        x: 0, y: 0, active: 0, type: 0
    }));
    npcs = new Array(MAX_NPCS).fill(null).map(() => ({
        x: 0, y: 0, active: 0, type: 0, hp: 2, isPolice: 0
    }));
    bullets = [];
}

function isSolid(x, y) {
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return true;
    const tile = worldMap[Math.floor(y)][Math.floor(x)];
    return tile === TILE_BUILDING || tile === TILE_BUILDING_ALT;
}

function initMap() {
    for (let y = 0; y < MAP_HEIGHT; y++) {
        for (let x = 0; x < MAP_WIDTH; x++) {
            worldMap[y][x] = TILE_GRASS;
        }
    }

    for (let y = 0; y < MAP_HEIGHT; y += ROAD_SPACING) {
        for (let x = 0; x < MAP_WIDTH; x++) {
            worldMap[y][x] = TILE_ROAD;
            if (y + 1 < MAP_HEIGHT) worldMap[y + 1][x] = TILE_ROAD;
        }
    }

    for (let x = 0; x < MAP_WIDTH; x += ROAD_SPACING) {
        for (let y = 0; y < MAP_HEIGHT; y++) {
            worldMap[y][x] = TILE_ROAD;
            if (x + 1 < MAP_WIDTH) worldMap[y][x + 1] = TILE_ROAD;
        }
    }

    for (let y = 4; y < MAP_HEIGHT - 4; y += ROAD_SPACING) {
        for (let x = 4; x < MAP_WIDTH - 4; x += ROAD_SPACING) {
            const blockW = ROAD_SPACING - 4;
            const blockH = ROAD_SPACING - 4;

            for (let by = 0; by < blockH && (y + by) < MAP_HEIGHT; by++) {
                for (let bx = 0; bx < blockW && (x + bx) < MAP_WIDTH; bx++) {
                    if ((bx + by) % 3 === 0) {
                        worldMap[y + by][x + bx] = TILE_BUILDING;
                    } else {
                        worldMap[y + by][x + bx] = TILE_BUILDING_ALT;
                    }
                }
            }
        }
    }
}

function spawnVehicle(index) {
    let attempts = 0;
    while (attempts < 100) {
        const x = Math.floor(Math.random() * MAP_WIDTH);
        const y = Math.floor(Math.random() * MAP_HEIGHT);

        if (worldMap[y][x] === TILE_ROAD) {
            let clear = true;
            for (let i = 0; i < MAX_VEHICLES; i++) {
                if (i !== index && vehicles[i].active) {
                    const dx = vehicles[i].x - x;
                    const dy = vehicles[i].y - y;
                    if (dx * dx + dy * dy < 9.0) {
                        clear = false;
                        break;
                    }
                }
            }

            if (clear) {
                vehicles[index].x = x;
                vehicles[index].y = y;
                vehicles[index].active = 1;
                vehicles[index].type = Math.floor(Math.random() * 3);
                return;
            }
        }
        attempts++;
    }
    vehicles[index].active = 0;
}

function spawnNPC(index, forcePolice) {
    npcs[index].active = 1;
    npcs[index].hp = 2;
    npcs[index].isPolice = forcePolice ? 1 : 0;

    const angle = (Math.random() * 360) * Math.PI / 180;
    const dist = NPC_SPAWN_DIST + (Math.random() * 10);

    npcs[index].x = player.x + Math.cos(angle) * dist;
    npcs[index].y = player.y + Math.sin(angle) * dist;

    npcs[index].x = Math.max(2, Math.min(MAP_WIDTH - 2, npcs[index].x));
    npcs[index].y = Math.max(2, Math.min(MAP_HEIGHT - 2, npcs[index].y));

    if (isSolid(Math.floor(npcs[index].x), Math.floor(npcs[index].y))) {
        npcs[index].active = 0;
    }
}

function updateWantedLevel() {
    if (player.wantedScore > 0) {
        player.wantedTimer++;
        if (player.wantedTimer >= WANTED_DECAY_TIME) {
            player.wantedScore = 0;
            player.wantedLevel = 0;
            player.wantedTimer = 0;
        }
    }

    let newLevel = Math.floor(player.wantedScore / WANTED_STAR_THRESHOLD);
    if (newLevel > MAX_WANTED_LEVEL) newLevel = MAX_WANTED_LEVEL;
    player.wantedLevel = newLevel;

    if (player.wantedLevel > 0) {
        const targetPolice = player.wantedLevel * 2;
        let activePolice = 0;

        for (let i = 0; i < MAX_NPCS; i++) {
            if (npcs[i].active && npcs[i].isPolice) activePolice++;
        }

        for (let i = 0; i < MAX_NPCS && activePolice < targetPolice; i++) {
            if (!npcs[i].active) {
                spawnNPC(i, 1);
                activePolice++;
            }
        }
    }
}

function updateNPCs() {
    for (let i = 0; i < MAX_NPCS; i++) {
        if (!npcs[i].active) continue;

        if (npcs[i].isPolice && player.wantedLevel > 0) {
            const dx = player.x - npcs[i].x;
            const dy = player.y - npcs[i].y;
            const dist = Math.sqrt(dx * dx + dy * dy);

            if (dist > 0.1) {
                const speed = NPC_SPEED + (player.wantedLevel * 0.05);
                const nx = npcs[i].x + (dx / dist) * speed;
                const ny = npcs[i].y + (dy / dist) * speed;

                // Wall-sliding collision
                if (!isSolid(Math.floor(nx), Math.floor(npcs[i].y))) {
                    npcs[i].x = nx;
                }
                if (!isSolid(Math.floor(npcs[i].x), Math.floor(ny))) {
                    npcs[i].y = ny;
                }
            }

            if (dist < 1.5) {
                player.hp--;
                player.wantedTimer = 0;
            }
        } else {
            npcs[i].x += (Math.floor(Math.random() * 3) - 1) * 0.1;
            npcs[i].y += (Math.floor(Math.random() * 3) - 1) * 0.1;
        }

        const camDistX = npcs[i].x - cameraX - SCREEN_W / 2;
        const camDistY = npcs[i].y - cameraY - SCREEN_H / 2;
        if (Math.abs(camDistX) > SCREEN_W || Math.abs(camDistY) > SCREEN_H) {
            if (!npcs[i].isPolice) {
                npcs[i].active = 0;
            }
        }
    }
}

function updateBullets() {
    for (let i = bullets.length - 1; i >= 0; i--) {
        const b = bullets[i];
        b.x += b.vx;
        b.y += b.vy;

        // Check collision with buildings
        if (isSolid(Math.floor(b.x), Math.floor(b.y))) {
            bullets.splice(i, 1);
            continue;
        }

        // Check collision with NPCs
        let hit = false;
        for (let j = 0; j < MAX_NPCS; j++) {
            if (npcs[j].active) {
                const dx = npcs[j].x - b.x;
                const dy = npcs[j].y - b.y;
                const dist = Math.sqrt(dx * dx + dy * dy);
                if (dist < 1.0) {
                    npcs[j].hp--;
                    if (npcs[j].hp <= 0) {
                        npcs[j].active = 0;
                        if (npcs[j].isPolice) {
                            player.wantedScore = Math.max(0, player.wantedScore - 20);
                        }
                        score += npcs[j].isPolice ? 100 : 10;
                    }
                    hit = true;
                    break;
                }
            }
        }

        if (hit) {
            bullets.splice(i, 1);
            continue;
        }

        // Remove bullets out of bounds
        if (b.x < 0 || b.x >= MAP_WIDTH || b.y < 0 || b.y >= MAP_HEIGHT) {
            bullets.splice(i, 1);
        }
    }
}

function initGame() {
    initArrays();
    initMap();

    let startX = ROAD_SPACING + 2;
    let startY = ROAD_SPACING + 2;
    while (isSolid(startX, startY) && startX < MAP_WIDTH) {
        startX++;
        if (isSolid(startX, startY)) {
            startX = ROAD_SPACING + 2;
            startY++;
        }
    }

    player = {
        x: startX,
        y: startY,
        hp: MAX_PLAYER_HP,
        inVehicle: 0,
        vehicleIndex: -1,
        wantedLevel: 0,
        wantedScore: 0,
        wantedTimer: 0,
        facingX: 0,
        facingY: -1
    };

    for (let i = 0; i < MAX_VEHICLES; i++) {
        spawnVehicle(i);
    }

    for (let i = 0; i < MAX_NPCS; i++) {
        npcs[i].active = 0;
    }

    bullets = [];
    score = 0;
    frameCount = 0;
}

function updateCamera() {
    cameraX = player.x - SCREEN_W / 2;
    cameraY = player.y - SCREEN_H / 2;

    if (cameraX < 0) cameraX = 0;
    if (cameraY < 0) cameraY = 0;
    if (cameraX > MAP_WIDTH - SCREEN_W) cameraX = MAP_WIDTH - SCREEN_W;
    if (cameraY > MAP_HEIGHT - SCREEN_H) cameraY = MAP_HEIGHT - SCREEN_H;
}

function getVehicleColor(type) {
    switch (type) {
        case 0: return '#cc3333';
        case 1: return '#3366cc';
        default: return '#cccc33';
    }
}

function getNPCColor(npc) {
    if (npc.isPolice) return '#3333ff';
    return '#ff66cc';
}

function drawMap() {
    const startX = Math.floor(cameraX);
    const startY = Math.floor(cameraY);
    const offsetX = (cameraX - startX) * TILE_SIZE;
    const offsetY = (cameraY - startY) * TILE_SIZE;

    for (let row = 0; row < SCREEN_H + 1; row++) {
        for (let col = 0; col < SCREEN_W + 1; col++) {
            const mapX = startX + col;
            const mapY = startY + row;

            if (mapX >= 0 && mapX < MAP_WIDTH && mapY >= 0 && mapY < MAP_HEIGHT) {
                const tile = worldMap[mapY][mapX];
                ctx.fillStyle = COLORS[tile];
                ctx.fillRect(
                    col * TILE_SIZE - offsetX,
                    row * TILE_SIZE - offsetY,
                    TILE_SIZE,
                    TILE_SIZE
                );

                // Add some detail for buildings
                if (tile === TILE_BUILDING || tile === TILE_BUILDING_ALT) {
                    ctx.fillStyle = 'rgba(0,0,0,0.2)';
                    ctx.fillRect(
                        col * TILE_SIZE - offsetX + 2,
                        row * TILE_SIZE - offsetY + 2,
                        TILE_SIZE - 4,
                        TILE_SIZE - 4
                    );
                }
            }
        }
    }
}

function drawEntities() {
    // Draw vehicles
    for (let i = 0; i < MAX_VEHICLES; i++) {
        if (!vehicles[i].active) continue;
        const sx = (vehicles[i].x - cameraX) * TILE_SIZE;
        const sy = (vehicles[i].y - cameraY) * TILE_SIZE;

        if (sx >= -TILE_SIZE && sx < canvas.width && sy >= -TILE_SIZE && sy < canvas.height) {
            ctx.fillStyle = getVehicleColor(vehicles[i].type);
            ctx.fillRect(sx + 2, sy + 2, TILE_SIZE - 4, TILE_SIZE - 4);
            // Windshield
            ctx.fillStyle = '#88ccff';
            ctx.fillRect(sx + 4, sy + 4, TILE_SIZE - 8, 4);
        }
    }

    // Draw NPCs
    for (let i = 0; i < MAX_NPCS; i++) {
        if (!npcs[i].active) continue;
        const sx = (npcs[i].x - cameraX) * TILE_SIZE;
        const sy = (npcs[i].y - cameraY) * TILE_SIZE;

        if (sx >= -TILE_SIZE && sx < canvas.width && sy >= -TILE_SIZE && sy < canvas.height) {
            ctx.fillStyle = getNPCColor(npcs[i]);
            ctx.beginPath();
            ctx.arc(sx + TILE_SIZE/2, sy + TILE_SIZE/2, TILE_SIZE/3, 0, Math.PI * 2);
            ctx.fill();

            // Police hat
            if (npcs[i].isPolice) {
                ctx.fillStyle = '#111';
                ctx.fillRect(sx + 4, sy + 2, TILE_SIZE - 8, 4);
            }
        }
    }

    // Draw bullets
    ctx.fillStyle = '#ffff00';
    for (const b of bullets) {
        const sx = (b.x - cameraX) * TILE_SIZE;
        const sy = (b.y - cameraY) * TILE_SIZE;
        ctx.fillRect(sx + TILE_SIZE/2 - 2, sy + TILE_SIZE/2 - 2, 4, 4);
    }

    // Draw player
    const psx = (player.x - cameraX) * TILE_SIZE;
    const psy = (player.y - cameraY) * TILE_SIZE;

    if (player.inVehicle) {
        ctx.fillStyle = '#00ff00';
        ctx.fillRect(psx, psy, TILE_SIZE, TILE_SIZE);
        ctx.fillStyle = '#88ccff';
        ctx.fillRect(psx + 2, psy + 2, TILE_SIZE - 4, 4);
    } else {
        ctx.fillStyle = '#00ff00';
        ctx.beginPath();
        ctx.arc(psx + TILE_SIZE/2, psy + TILE_SIZE/2, TILE_SIZE/3, 0, Math.PI * 2);
        ctx.fill();

        // Direction indicator
        ctx.strokeStyle = '#00ff00';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(psx + TILE_SIZE/2, psy + TILE_SIZE/2);
        ctx.lineTo(
            psx + TILE_SIZE/2 + player.facingX * 8,
            psy + TILE_SIZE/2 + player.facingY * 8
        );
        ctx.stroke();
    }
}

function drawHUD() {
    // HP Hearts
    ctx.fillStyle = '#fff';
    ctx.font = 'bold 16px monospace';
    ctx.textAlign = 'left';
    let hpText = 'HP: ';
    for (let i = 0; i < MAX_PLAYER_HP; i++) {
        hpText += i < player.hp ? '♥' : '-';
    }
    ctx.fillText(hpText, 10, 24);

    // Wanted Level Stars
    if (player.wantedLevel > 0) {
        ctx.fillStyle = '#ff3333';
        let wantedText = '[';
        for (let i = 0; i < player.wantedLevel; i++) {
            wantedText += '★';
        }
        wantedText += ']';
        ctx.fillText(wantedText, 150, 24);
    }

    // In Car indicator
    ctx.fillStyle = '#ffff00';
    if (player.inVehicle) {
        ctx.fillText('[IN CAR]', 280, 24);
    }

    // Score
    ctx.fillStyle = '#fff';
    ctx.fillText(`Score: ${score}`, 400, 24);

    // Wanted Score (debug/info)
    if (player.wantedScore > 0) {
        ctx.fillStyle = '#ff6666';
        ctx.fillText(`Wanted: ${player.wantedScore}`, 520, 24);
    }
}

function handleInput() {
    const speed = player.inVehicle ? VEHICLE_SPEED : PLAYER_SPEED;
    let newX = player.x;
    let newY = player.y;
    let moved = false;

    if (keys['ArrowUp']) {
        newY -= speed;
        player.facingX = 0;
        player.facingY = -1;
        moved = true;
    }
    if (keys['ArrowDown']) {
        newY += speed;
        player.facingX = 0;
        player.facingY = 1;
        moved = true;
    }
    if (keys['ArrowLeft']) {
        newX -= speed;
        player.facingX = -1;
        player.facingY = 0;
        moved = true;
    }
    if (keys['ArrowRight']) {
        newX += speed;
        player.facingX = 1;
        player.facingY = 0;
        moved = true;
    }

    if (newX < 1) newX = 1;
    if (newX > MAP_WIDTH - 2) newX = MAP_WIDTH - 2;
    if (newY < 1) newY = 1;
    if (newY > MAP_HEIGHT - 2) newY = MAP_HEIGHT - 2;

    // Wall-sliding collision - try X and Y independently
    if (!isSolid(Math.floor(newX), Math.floor(player.y))) {
        player.x = newX;
    }
    if (!isSolid(Math.floor(player.x), Math.floor(newY))) {
        player.y = newY;
    }

    // Shooting (Space key)
    const spacePressed = keys[' '] && !prevSpace;
    prevSpace = keys[' '] ? true : false;

    if (spacePressed) {
        // Commit crime
        player.wantedScore += 10;
        player.wantedTimer = 0;

        // Fire bullet
        bullets.push({
            x: player.x,
            y: player.y,
            vx: player.facingX * 0.8,
            vy: player.facingY * 0.8
        });
    }

    // Enter/Exit Vehicle (Enter or F key)
    const enterPressed = (keys['Enter'] || keys['f'] || keys['F']) && !prevEnter;
    prevEnter = (keys['Enter'] || keys['f'] || keys['F']) ? true : false;

    if (enterPressed) {
        if (player.inVehicle) {
            player.inVehicle = 0;
            vehicles[player.vehicleIndex].active = 1;
            vehicles[player.vehicleIndex].x = player.x;
            vehicles[player.vehicleIndex].y = player.y;
            player.vehicleIndex = -1;

            let attempts = 0;
            while (isSolid(Math.floor(player.x), Math.floor(player.y)) && attempts < 4) {
                if (!isSolid(Math.floor(player.x) + 1, Math.floor(player.y))) player.x += 1;
                else if (!isSolid(Math.floor(player.x) - 1, Math.floor(player.y))) player.x -= 1;
                else if (!isSolid(Math.floor(player.x), Math.floor(player.y) + 1)) player.y += 1;
                else if (!isSolid(Math.floor(player.x), Math.floor(player.y) - 1)) player.y -= 1;
                attempts++;
            }
        } else {
            for (let i = 0; i < MAX_VEHICLES; i++) {
                if (vehicles[i].active) {
                    const dx = vehicles[i].x - player.x;
                    const dy = vehicles[i].y - player.y;
                    const dist = Math.sqrt(dx * dx + dy * dy);

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

function drawTitleScreen() {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = '#fff';
    ctx.font = 'bold 36px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('=== GTA MONOLITH ===', canvas.width / 2, 180);

    ctx.font = '20px monospace';
    ctx.fillText('Open World Action Game', canvas.width / 2, 230);

    ctx.font = '16px monospace';
    ctx.fillStyle = '#aaa';
    ctx.fillText('Arrow Keys: Move    Space: Shoot    Enter/F: Enter/Exit Car', canvas.width / 2, 280);

    if (blinkTimer < 30) {
        ctx.fillStyle = '#ffff00';
        ctx.font = 'bold 20px monospace';
        ctx.fillText('Press ENTER to Begin', canvas.width / 2, 350);
    }
}

function drawGameOverScreen() {
    ctx.fillStyle = 'rgba(0, 0, 0, 0.8)';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.fillStyle = '#ff0000';
    ctx.font = 'bold 48px monospace';
    ctx.textAlign = 'center';
    ctx.fillText('WASTED', canvas.width / 2, 220);

    ctx.fillStyle = '#fff';
    ctx.font = '24px monospace';
    ctx.fillText(`Score: ${score}`, canvas.width / 2, 270);

    if (blinkTimer < 30) {
        ctx.fillStyle = '#ffff00';
        ctx.font = 'bold 18px monospace';
        ctx.fillText('Press ENTER to Restart', canvas.width / 2, 330);
    }
}

function gameLoop() {
    blinkTimer = (blinkTimer + 1) % 60;

    if (gameState === STATE_TITLE) {
        drawTitleScreen();

        if (keys['Enter']) {
            gameState = STATE_PLAYING;
            initGame();
        }
    } else if (gameState === STATE_PLAYING) {
        handleInput();
        updateCamera();
        updateNPCs();
        updateWantedLevel();
        updateBullets();

        // Clear and draw
        ctx.fillStyle = '#000';
        ctx.fillRect(0, 0, canvas.width, canvas.height);

        drawMap();
        drawEntities();
        drawHUD();

        if (player.hp <= 0) {
            gameState = STATE_GAMEOVER;
        }

        frameCount++;
    } else if (gameState === STATE_GAMEOVER) {
        drawGameOverScreen();

        if (keys['Enter']) {
            initGame();
            gameState = STATE_PLAYING;
        }
    }

    requestAnimationFrame(gameLoop);
}

// Event listeners
document.addEventListener('keydown', (e) => {
    keys[e.key] = true;
    if (e.key === ' ' || e.key === 'ArrowUp' || e.key === 'ArrowDown' ||
        e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        e.preventDefault();
    }
});

document.addEventListener('keyup', (e) => {
    keys[e.key] = false;
});

// Start the game
initArrays();
initGame();
gameState = STATE_TITLE;
requestAnimationFrame(gameLoop);
