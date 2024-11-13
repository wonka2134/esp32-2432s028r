#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

// Forward declarations
void drawGame();
void drawGrid();
void drawPiece();
void drawUI();
void drawButton(struct Button& btn);
void drawScore();
void drawNextPiece();
bool checkTouch(struct Button &btn);
void handleGameButton(int buttonIndex);
bool isValidMove(int x, int y, uint8_t piece[4][4]);
void placePiece();
void checkLines();
void newPiece();
void rotatePiece();
void gameOverScreen();
void initGame();

// TFT setup
TFT_eSPI tft = TFT_eSPI();

// Touchscreen pins for CYD board
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// Create a separate SPI instance for touch
SPIClass touchscreenSPI = SPIClass(VSPI);
XPT2046_Touchscreen touchscreen(XPT2046_CS);

// Screen dimensions and game constants
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define GRID_COLS 10
#define GRID_ROWS 15
#define BLOCK_SIZE 12
#define GRID_X 20
#define GRID_Y 5
#define BASE_SPEED 500
#define MIN_SPEED 100

// Colors
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define BLUE 0x001F
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define ORANGE 0xFD20
#define GRAY 0x8410
#define DARKGRAY 0x4208

// Button structure
struct Button {
    int16_t x, y, w, h;
    const char* label;
    uint16_t color;
    bool pressed;
};

// Buttons array
Button buttons[] = {
    {20, SCREEN_HEIGHT - 45, 50, 30, "LEFT", BLUE, false},
    {80, SCREEN_HEIGHT - 45, 50, 30, "ROT", GREEN, false},
    {140, SCREEN_HEIGHT - 45, 50, 30, "RIGHT", BLUE, false},
    {200, SCREEN_HEIGHT - 45, 50, 30, "DROP", RED, false},
    {260, SCREEN_HEIGHT - 45, 50, 30, "START", GREEN, false}
};
#define NUM_BUTTONS 5

// Tetris colors and shapes
const uint16_t COLORS[] = {
    CYAN, BLUE, ORANGE, YELLOW, GREEN, MAGENTA, RED
};

const uint8_t TETROMINOES[7][4][4] = {
    {{0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0}},
    {{1,0,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    {{0,0,1,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,1,1,0},{1,1,0,0},{0,0,0,0},{0,0,0,0}},
    {{0,1,0,0},{1,1,1,0},{0,0,0,0},{0,0,0,0}},
    {{1,1,0,0},{0,1,1,0},{0,0,0,0},{0,0,0,0}}
};

// Game state structure
struct GameState {
    uint8_t grid[GRID_ROWS][GRID_COLS];
    uint8_t piece[4][4];
    uint8_t nextPiece[4][4];
    int currentX;
    int currentY;
    int currentPiece;
    int nextPieceType;
    unsigned long lastDrop;
    int score;
    bool gameOver;
    bool gamePaused;
    int level;
    int dropSpeed;

    GameState() {
        clearState();
    }

    void clearState() {
        memset(grid, 0, sizeof(grid));
        memset(piece, 0, sizeof(piece));
        memset(nextPiece, 0, sizeof(nextPiece));
        score = 0;
        gameOver = false;
        gamePaused = true;
        level = 1;
        dropSpeed = BASE_SPEED;
        lastDrop = 0;
        currentX = GRID_COLS/2 - 2;
        currentY = 0;
    }
} game;

// Touch coordinates
uint16_t t_x = 0, t_y = 0;
void setup() {
    Serial.begin(115200);
    
    // Initialize TFT
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(BLACK);
    tft.setTextColor(WHITE, BLACK);

    // Initialize touchscreen with VSPI
    touchscreenSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    touchscreen.begin(touchscreenSPI);
    touchscreen.setRotation(1);

    // Better random seed initialization
    uint32_t seed = analogRead(0);
    seed = (seed << 16) | analogRead(0);
    seed ^= millis();
    randomSeed(seed);

    // Start game
    initGame();
}

void loop() {
    static uint32_t lastTouchTime = 0;
    static bool wasTouched = false;
    const uint32_t TOUCH_DEBOUNCE = 100;  // Reduced debounce time

    // Handle touch input
    if (touchscreen.touched()) {
        if (!wasTouched && (millis() - lastTouchTime > TOUCH_DEBOUNCE)) {
            TS_Point p = touchscreen.getPoint();
            lastTouchTime = millis();

            // Map touch coordinates for CYD board
            t_x = map(p.x, 200, 3700, 1, SCREEN_WIDTH);
            t_y = map(p.y, 240, 3800, 1, SCREEN_HEIGHT);

            // Process buttons
            for (int i = 0; i < NUM_BUTTONS; i++) {
                if (checkTouch(buttons[i])) {
                    // Visual feedback
                    uint16_t originalColor = buttons[i].color;
                    buttons[i].color = WHITE;
                    drawButton(buttons[i]);
                    delay(30);
                    buttons[i].color = originalColor;
                    drawButton(buttons[i]);

                    if (i == 4) {  // START button
                        if (game.gameOver) {
                            initGame();
                            game.gamePaused = false;
                            buttons[4].label = "PAUSE";
                            drawButton(buttons[4]);
                        } else {
                            game.gamePaused = !game.gamePaused;
                            buttons[4].label = game.gamePaused ? "START" : "PAUSE";
                            drawButton(buttons[4]);
                        }
                    }
                    else if (!game.gameOver && !game.gamePaused) {
                        handleGameButton(i);
                    }
                }
            }
        }
        wasTouched = true;
    } else {
        wasTouched = false;
    }

    // Game logic
    if (!game.gameOver && !game.gamePaused) {
        unsigned long currentTime = millis();
        
        if (currentTime - game.lastDrop > game.dropSpeed) {
            if (isValidMove(game.currentX, game.currentY + 1, game.piece)) {
                game.currentY++;
                drawGame();
            } else {
                placePiece();
                checkLines();
                newPiece();
                
                if (!isValidMove(game.currentX, game.currentY, game.piece)) {
                    game.gameOver = true;
                    buttons[4].label = "START";
                    drawButton(buttons[4]);
                    gameOverScreen();
                }
            }
            game.lastDrop = currentTime;
        }
    }
}

void handleGameButton(int buttonIndex) {
    switch(buttonIndex) {
        case 0: // Left
            if (isValidMove(game.currentX - 1, game.currentY, game.piece)) {
                game.currentX--;
                drawGame();
            }
            break;
        
        case 1: // Rotate
            rotatePiece();
            break;
        
        case 2: // Right
            if (isValidMove(game.currentX + 1, game.currentY, game.piece)) {
                game.currentX++;
                drawGame();
            }
            break;
        
        case 3: // Drop
            while (isValidMove(game.currentX, game.currentY + 1, game.piece)) {
                game.currentY++;
            }
            drawGame();
            game.lastDrop = millis() - game.dropSpeed + 50;
            break;
    }
}

void initGame() {
    tft.fillScreen(BLACK);
    game.clearState();
    game.dropSpeed = BASE_SPEED;
    
    uint32_t randSeed = millis() ^ (analogRead(0) << 16);
    game.nextPieceType = randSeed % 7;
    memcpy(game.nextPiece, TETROMINOES[game.nextPieceType], sizeof(game.nextPiece));
    newPiece();
    
    drawUI();
}

bool isValidMove(int x, int y, uint8_t piece[4][4]) {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (piece[i][j]) {
                int newX = x + j;
                int newY = y + i;
                
                if (newX < 0 || newX >= GRID_COLS || newY >= GRID_ROWS || newY < 0) {
                    return false;
                }
                
                if (newY >= 0 && game.grid[newY][newX]) {
                    return false;
                }
            }
        }
    }
    return true;
}

void placePiece() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game.piece[i][j]) {
                game.grid[game.currentY + i][game.currentX + j] = game.currentPiece + 1;
            }
        }
    }
}

void checkLines() {
    int linesCleared = 0;
    
    for (int i = GRID_ROWS - 1; i >= 0; i--) {
        bool lineComplete = true;
        for (int j = 0; j < GRID_COLS; j++) {
            if (!game.grid[i][j]) {
                lineComplete = false;
                break;
            }
        }
        
        if (lineComplete) {
            linesCleared++;
            for (int k = i; k > 0; k--) {
                memcpy(game.grid[k], game.grid[k-1], GRID_COLS);
            }
            memset(game.grid[0], 0, GRID_COLS);
            i++;
        }
    }
    
    if (linesCleared > 0) {
        game.score += (1 << linesCleared) * 100;
        int oldLevel = game.level;
        game.level = (game.score / 1000) + 1;
        
        if (oldLevel != game.level) {
            game.dropSpeed = BASE_SPEED - (game.level * 45);
            if (game.dropSpeed < MIN_SPEED) game.dropSpeed = MIN_SPEED;
        }
        
        drawScore();
    }
}

void newPiece() {
    game.currentPiece = game.nextPieceType;
    memcpy(game.piece, game.nextPiece, sizeof(game.piece));
    
    uint32_t randSeed = millis() ^ (analogRead(0) << 16);
    game.nextPieceType = (randSeed % 7);
    memcpy(game.nextPiece, TETROMINOES[game.nextPieceType], sizeof(game.nextPiece));
    
    game.currentX = GRID_COLS / 2 - 2;
    game.currentY = 0;
    
    drawNextPiece();
}

void rotatePiece() {
    uint8_t temp[4][4] = {0};
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            temp[j][3-i] = game.piece[i][j];
        }
    }
    
    if (isValidMove(game.currentX, game.currentY, temp)) {
        memcpy(game.piece, temp, sizeof(game.piece));
        drawGame();
    }
}

void gameOverScreen() {
    int centerX = GRID_X + ((GRID_COLS * BLOCK_SIZE) / 2) - 50;
    int centerY = GRID_Y + ((GRID_ROWS * BLOCK_SIZE) / 2) - 10;
    
    tft.fillRect(centerX - 10, centerY - 10, 120, 40, RED);
    tft.setTextColor(WHITE);
    tft.drawString("GAME OVER!", centerX, centerY, 4);
}

void drawScore() {
    int scoreX = GRID_X + (GRID_COLS * BLOCK_SIZE) + 30;
    int scoreY = 5;
    
    tft.setTextColor(WHITE, BLACK);
    tft.fillRect(scoreX, scoreY, 80, 25, BLACK);
    tft.drawString("SCORE:", scoreX, scoreY, 2);
    tft.drawNumber(game.score, scoreX + 55, scoreY, 2);
    
    tft.fillRect(scoreX, scoreY + 15, 80, 25, BLACK);
    tft.drawString("LEVEL:", scoreX, scoreY + 15, 2);
    tft.drawNumber(game.level, scoreX + 55, scoreY + 15, 2);
}

void drawGrid() {
    tft.drawRect(GRID_X - 1, GRID_Y - 1, 
                 (GRID_COLS * BLOCK_SIZE) + 2, 
                 (GRID_ROWS * BLOCK_SIZE) + 2, WHITE);
                 
    for (int i = 0; i < GRID_ROWS; i++) {
        for (int j = 0; j < GRID_COLS; j++) {
            int x = GRID_X + (j * BLOCK_SIZE);
            int y = GRID_Y + (i * BLOCK_SIZE);
            
            if (game.grid[i][j] == 0) {
                tft.fillRect(x, y, BLOCK_SIZE - 1, BLOCK_SIZE - 1, BLACK);
                tft.drawRect(x, y, BLOCK_SIZE - 1, BLOCK_SIZE - 1, DARKGRAY);
            } else {
                tft.fillRect(x, y, BLOCK_SIZE - 1, BLOCK_SIZE - 1, COLORS[game.grid[i][j] - 1]);
            }
        }
    }
}

void drawPiece() {
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game.piece[i][j]) {
                int x = GRID_X + ((game.currentX + j) * BLOCK_SIZE);
                int y = GRID_Y + ((game.currentY + i) * BLOCK_SIZE);
                tft.fillRect(x, y, BLOCK_SIZE - 1, BLOCK_SIZE - 1, COLORS[game.currentPiece]);
            }
        }
    }
}

void drawNextPiece() {
    int nextX = GRID_X + (GRID_COLS * BLOCK_SIZE) + 30;
    int nextY = 45;
    
    tft.fillRect(nextX, nextY, 50, 50, BLACK);
    tft.drawRect(nextX - 1, nextY - 1, 52, 52, WHITE);
    
    int offsetX = nextX + 10;
    int offsetY = nextY + 10;
    
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            if (game.nextPiece[i][j]) {
                int x = offsetX + (j * (BLOCK_SIZE - 2));
                int y = offsetY + (i * (BLOCK_SIZE - 2));
                tft.fillRect(x, y, BLOCK_SIZE - 2, BLOCK_SIZE - 2, COLORS[game.nextPieceType]);
            }
        }
    }
}

void drawButton(Button& btn) {
    tft.fillRoundRect(btn.x, btn.y, btn.w, btn.h, 5, btn.color);
    tft.drawRoundRect(btn.x, btn.y, btn.w, btn.h, 5, WHITE);
    tft.setTextColor(WHITE);
    int textX = btn.x + (btn.w - strlen(btn.label) * 6) / 2;
    int textY = btn.y + (btn.h - 8) / 2;
    tft.drawString(btn.label, textX, textY, 2);
}

void drawGame() {
    drawGrid();
    drawPiece();
    drawNextPiece();
    drawScore();
}

void drawUI() {
    tft.fillScreen(BLACK);
    tft.drawRect(GRID_X - 2, GRID_Y - 2, 
                 (GRID_COLS * BLOCK_SIZE) + 4, 
                 (GRID_ROWS * BLOCK_SIZE) + 4, WHITE);
    
    for (int i = 0; i < NUM_BUTTONS; i++) {
        drawButton(buttons[i]);
    }
    
    drawGame();
}

bool checkTouch(Button &btn) {
    const int TOUCH_MARGIN = 5;
    return (t_x >= (btn.x - TOUCH_MARGIN) && 
            t_x < (btn.x + btn.w + TOUCH_MARGIN) &&
            t_y >= (btn.y - TOUCH_MARGIN) && 
            t_y < (btn.y + btn.h + TOUCH_MARGIN));
}
