#include <time.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
// Constants
#define WINDOW_WIDTH 880
#define GRID_SPACING 40
#define WINDOW_HEIGHT 600
#define BUTTON_WIDTH 350
#define BUTTON_HEIGHT 70
#define FONT_SIZE 30
#define MAX_NAME_LENGTH 20
#define FPS 60
#define GRID_SIZE 4
#define TILE_SIZE 100

#define MAX_SCORES 5
#define MAX_NAME_LENGTH 20

// structure to store high score
typedef struct {
    char name[MAX_NAME_LENGTH];
    int score;
    int time;
} HighScore;

// Global variables for game state

int bestScore = 0;
int highestScore = 0;
 // Taille de la grille
int grid[GRID_SIZE][GRID_SIZE];
int score = 0;
bool isPaused = false;
#define MODE_PLAYER 0
#define MODE_MACHINE 1
#define MODE_PLAYER_VS_MACHINE 2
Uint32 startTime = 0;
Uint32 pausedTime = 0;
Uint32 totalPausedDuration = 0;// Durée de la pause
Uint32 pauseStartTime = 0;  // Temps auquel la pause a commencé
HighScore highScores[MAX_SCORES];
int playerVsMachineGrid[2][GRID_SIZE][GRID_SIZE];  // [0] for player, [1] for machine
int playerVsMachineScores[2] = {0, 0};  // [0] for player, [1] for machine

int gameMode = -1; // Mode actuel de jeu
bool isGameSaved = false;  // To track if game state is saved
// Initialise la grille avec deux tuiles
void PrepareGrid() {
    srand(time(NULL));
    for (int i = 0; i < GRID_SIZE; ++i)
        for (int j = 0; j < GRID_SIZE; ++j)
            grid[i][j] = 0;

    for (int i = 0; i < 2; ++i) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
        while (grid[x][y] != 0) {
            x = rand() % GRID_SIZE;
            y = rand() % GRID_SIZE;
        }
        grid[x][y] = (rand() % 2 + 1) * 2;
    }
}

// Structure to save entire game state
typedef struct {
    int grid[GRID_SIZE][GRID_SIZE];
    int score;
    int gameMode;
    Uint32 elapsedTime;  // Store actual elapsed time instead of timestamps
    bool isPaused;
    // Add player vs machine specific data
    int playerVsMachineGrid[2][GRID_SIZE][GRID_SIZE];
    int playerVsMachineScores[2];
} GameState;
// Save game state to a file
void saveCurrentState() {
    FILE* file = fopen("game_save.bin", "wb");
    if (!file) {
        printf("Error saving game state\n");
        return;
    }

    GameState state;
    memcpy(state.grid, grid, sizeof(grid));
    state.score = score;
    state.gameMode = gameMode;
    state.elapsedTime = SDL_GetTicks() - startTime - totalPausedDuration;
    state.isPaused = isPaused;

    // Save player vs machine data if in that mode
    if (gameMode == MODE_PLAYER_VS_MACHINE) {
        memcpy(state.playerVsMachineGrid, playerVsMachineGrid, sizeof(playerVsMachineGrid));
        memcpy(state.playerVsMachineScores, playerVsMachineScores, sizeof(playerVsMachineScores));
    }

    fwrite(&state, sizeof(GameState), 1, file);
    fclose(file);
    isGameSaved = true;
}
// Load game state from file
bool RestoreGameState() {
    FILE* file = fopen("game_save.bin", "rb");
    if (!file) {
        printf("No saved game found\n");
        return false;
    }

    GameState state;
    if (fread(&state, sizeof(GameState), 1, file) != 1) {
        fclose(file);
        printf("Error reading saved game\n");
        return false;
    }

    // Validate game mode
    if (state.gameMode < MODE_PLAYER || state.gameMode > MODE_PLAYER_VS_MACHINE) {
        fclose(file);
        return false;
    }

    // Restore game state
    memcpy(grid, state.grid, sizeof(grid));
    score = state.score;
    gameMode = state.gameMode;
    startTime = SDL_GetTicks() - state.elapsedTime;  // Calculate new start time
    isPaused = state.isPaused;
    totalPausedDuration = 0;  // Reset pause duration for new session

    // Restore player vs machine data if applicable
    if (gameMode == MODE_PLAYER_VS_MACHINE) {
        memcpy(playerVsMachineGrid, state.playerVsMachineGrid, sizeof(playerVsMachineGrid));
        memcpy(playerVsMachineScores, state.playerVsMachineScores, sizeof(playerVsMachineScores));
    }

    fclose(file);
    return true;
}
// Function to load high scores from file
void loadTopScores() {
    FILE* file = fopen("highscores.dat", "rb");
    if (file) {
        fread(highScores, sizeof(HighScore), MAX_SCORES, file);
        fclose(file);

        // Update highest score
        highestScore = 0;
        for (int i = 0; i < MAX_SCORES; i++) {
            if (highScores[i].score > highestScore) {
                highestScore = highScores[i].score;
            }
        }
    } else {
        // Initialize with empty scores if file doesn't exist
        for (int i = 0; i < MAX_SCORES; i++) {
            strcpy(highScores[i].name, "");
            highScores[i].score = 0;
            highScores[i].time = 0;
        }
        highestScore = 0;
    }
}

Uint32 getCurrentGameTime() {
    if (isPaused) {
        return pausedTime - startTime - totalPausedDuration;
    } else {
        return SDL_GetTicks() - startTime - totalPausedDuration;
    }
}
// Function to save high scores to file
void saveTopScores() {
    FILE* file = fopen("highscores.dat", "wb");
    if (!file) {
        printf("Failed to save high scores\n");
        return;
    }

    fwrite(highScores, sizeof(HighScore), MAX_SCORES, file);
    fclose(file);
}
// Function to update high scores
void updateTopScores(const char* playerName) {
    // Calculate total game time using our new function
    Uint32 totalTime = getCurrentGameTime() / 1000;  // Convert to seconds

    // Create a temporary high score entry
    HighScore newScore;
    strncpy(newScore.name, playerName, MAX_NAME_LENGTH - 1);
    newScore.name[MAX_NAME_LENGTH - 1] = '\0';
    newScore.score = score;
    newScore.time = totalTime;

    // Sort high scores
    int insertIndex = -1;
    for (int i = 0; i < MAX_SCORES; i++) {
        if (newScore.score > highScores[i].score) {
            insertIndex = i;
            break;
        }
    }

    // Shift scores and insert new score
    if (insertIndex != -1) {
        for (int i = MAX_SCORES - 1; i > insertIndex; i--) {
            highScores[i] = highScores[i-1];
        }
        highScores[insertIndex] = newScore;

        // Update highest score if needed
        if (score > highestScore) {
            highestScore = score;
        }

        saveTopScores();
    }
}
// Dessiner une tuile
void drawCell(SDL_Renderer* renderer, TTF_Font* font, int value, int x, int y) {
    SDL_Rect tile = {x, y, TILE_SIZE, TILE_SIZE};

    // Couleurs des tuiles (ajuster si nécessaire)
    SDL_Color colors[] = {
        {205, 193, 180, 255}, {238, 228, 218, 255}, {237, 224, 200, 255},
        {242, 177, 121, 255}, {245, 149, 99, 255}, {246, 124, 95, 255}, // 2-64
        {246, 94, 59, 255}, {237, 207, 114, 255}, {237, 204, 97, 255}, // 128-512
        {237, 200, 80, 255}, {237, 197, 63, 255} // 1024-2048
    };

    SDL_Color tileColor;
    if (value == 64) {
        // Couleur spécifique pour la tuile de valeur 64
        tileColor = (SDL_Color){255, 215, 0, 255}; // Or
    } else {
        int colorIndex = value > 0 ? (value <= 2048 ? (int)log2(value) : 5) - 1 : 0;
        tileColor = colors[colorIndex];
    }

    SDL_SetRenderDrawColor(renderer, tileColor.r, tileColor.g, tileColor.b, tileColor.a);
    SDL_RenderFillRect(renderer, &tile);

    if (value > 0) {
        char buffer[16];
        sprintf(buffer, "%d", value);
        SDL_Surface* textSurface = TTF_RenderText_Blended(font, buffer, (SDL_Color){0, 0, 0, 255});
        SDL_Texture* text = SDL_CreateTextureFromSurface(renderer, textSurface);
        SDL_Rect textRect = {x + (TILE_SIZE - textSurface->w) / 2, y + (TILE_SIZE - textSurface->h) / 2, textSurface->w, textSurface->h};
        SDL_RenderCopy(renderer, text, NULL, &textRect);
        SDL_FreeSurface(textSurface);
        SDL_DestroyTexture(text);
    }
}

// Button structure
typedef struct {
    SDL_Rect rect;
    SDL_Color baseColor;
    SDL_Color hoverColor;
    SDL_Color textColor;
    const char* text;
} Button;
// Draw a button with text
void DesignButton(SDL_Renderer* renderer, TTF_Font* font, Button button, bool isHovered) {
    SDL_Color color = isHovered ? button.hoverColor : button.baseColor;
    // Draw button background
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &button.rect);
    // Render button text
    SDL_Surface* textSurface = TTF_RenderText_Blended(font, button.text, button.textColor);
    if (!textSurface) {
        printf("Failed to render button text: %s\n", TTF_GetError());
        return;
    }
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    if (!textTexture) {
        SDL_FreeSurface(textSurface);
        printf("Failed to create text texture: %s\n", SDL_GetError());
        return;
    }
    SDL_Rect textRect = {
        button.rect.x + (button.rect.w - textSurface->w) / 2,
        button.rect.y + (button.rect.h - textSurface->h) / 2,
        textSurface->w, textSurface->h
    };
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
}
void DisplayScore(SDL_Renderer* renderer, TTF_Font* font) {
    // Calculate current game time
    Uint32 currentTime;
    if (isPaused) {
        // If paused, use the time when we paused
        currentTime = pauseStartTime - startTime - totalPausedDuration;
    } else {
        // If not paused, calculate current time minus all paused durations
        currentTime = SDL_GetTicks() - startTime - totalPausedDuration;
    }
    int timeSeconds = currentTime / 1000;
    int minutes = timeSeconds / 60;
    int seconds = timeSeconds % 60;

    // Create text strings using existing variables
    char scoreStr[50], bestScoreStr[50], timeStr[50];
    snprintf(scoreStr, sizeof(scoreStr), "Score: %d", score);
    snprintf(bestScoreStr, sizeof(bestScoreStr), "Best: %d", highestScore);  // Using highestScore instead of bestScore
    snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d", minutes, seconds);

    // Set up text color
    SDL_Color textColor = {0, 0, 0, 255};

    // Draw background
    SDL_Rect bgRect = {0, 0, WINDOW_WIDTH, 50};
    SDL_SetRenderDrawColor(renderer, 187, 173, 160, 255);
    SDL_RenderFillRect(renderer, &bgRect);

    // Render and position each text element separately
    SDL_Surface* scoreSurface = TTF_RenderText_Blended(font, scoreStr, textColor);
    SDL_Surface* bestScoreSurface = TTF_RenderText_Blended(font, bestScoreStr, textColor);
    SDL_Surface* timeSurface = TTF_RenderText_Blended(font, timeStr, textColor);

    if (!scoreSurface || !bestScoreSurface || !timeSurface) {
        if (scoreSurface) SDL_FreeSurface(scoreSurface);
        if (bestScoreSurface) SDL_FreeSurface(bestScoreSurface);
        if (timeSurface) SDL_FreeSurface(timeSurface);
        return;
    }

    SDL_Texture* scoreTexture = SDL_CreateTextureFromSurface(renderer, scoreSurface);
    SDL_Texture* bestScoreTexture = SDL_CreateTextureFromSurface(renderer, bestScoreSurface);
    SDL_Texture* timeTexture = SDL_CreateTextureFromSurface(renderer, timeSurface);

    // Calculate positions to space elements evenly
    SDL_Rect scoreRect = {20, 10, scoreSurface->w, scoreSurface->h};
    SDL_Rect bestScoreRect = {WINDOW_WIDTH/2 - bestScoreSurface->w/2, 10, bestScoreSurface->w, bestScoreSurface->h};
    SDL_Rect timeRect = {WINDOW_WIDTH - timeSurface->w - 20, 10, timeSurface->w, timeSurface->h};

    // Render all elements
    SDL_RenderCopy(renderer, scoreTexture, NULL, &scoreRect);
    SDL_RenderCopy(renderer, bestScoreTexture, NULL, &bestScoreRect);
    SDL_RenderCopy(renderer, timeTexture, NULL, &timeRect);

    // Cleanup
    SDL_FreeSurface(scoreSurface);
    SDL_FreeSurface(bestScoreSurface);
    SDL_FreeSurface(timeSurface);
    SDL_DestroyTexture(scoreTexture);
    SDL_DestroyTexture(bestScoreTexture);
    SDL_DestroyTexture(timeTexture);
}
// Dessiner la grille compl te
void drawBoard(SDL_Renderer* renderer, TTF_Font* font, int yOffset) {
    // Clear the screen with the background color
    SDL_SetRenderDrawColor(renderer, 187, 173, 160, 255);
    SDL_RenderClear(renderer);

    // Draw the tiles
    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            int x = j * TILE_SIZE + 10 * (j + 1);
            int y = i * TILE_SIZE + 10 * (i + 1) + yOffset;
          drawCell(renderer, font, grid[i][j], x, y);
        }
    }

    // Draw the pause button
    Button pauseButton = {
        { 10, WINDOW_HEIGHT - 80, 100, 50 },  // Position in bottom left
        { 200, 200, 200, 255 },  // Base color
        { 150, 150, 255, 255 },  // Hover color
        { 0, 0, 0, 255 },        // Text color
        "Pause"
    };
    DesignButton(renderer, font, pauseButton, false);

    // Draw the score and timer
   DisplayScore(renderer, font);

    // Present the updated renderer
    SDL_RenderPresent(renderer);
}
void showCenteredText(SDL_Renderer* renderer, TTF_Font* font, const char* text, SDL_Color color, int y) {
    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    int textWidth = 0;
    int textHeight = 0;
    SDL_QueryTexture(texture, NULL, NULL, &textWidth, &textHeight);
    SDL_Rect destRect = { (WINDOW_WIDTH - textWidth) / 2, y, textWidth, textHeight };
    SDL_RenderCopy(renderer, texture, NULL, &destRect);
    SDL_DestroyTexture(texture);
}

void getUserName(SDL_Renderer* renderer, TTF_Font* font, char* playerName) {
    SDL_StartTextInput();

    SDL_Color textColor = {0, 0, 0, 255};
    bool enterName = true;
    SDL_Event event;
    playerName[0] = '\0';  // Initialize playerName with an empty string

    while (enterName) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                SDL_StopTextInput();
                exit(0);
            } else if (event.type == SDL_TEXTINPUT) {
                if (strlen(playerName) < MAX_NAME_LENGTH - 1) {
                    strcat(playerName, event.text.text);
                }
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_BACKSPACE && strlen(playerName) > 0) {
                    playerName[strlen(playerName) - 1] = '\0';
                } else if (event.key.keysym.sym == SDLK_RETURN && strlen(playerName) > 0) {
                    enterName = false;
                }
            }
        }

        // Clear renderer
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        // Render prompt text
        showCenteredText(renderer, font, " ------game over--------- Enter your name:", textColor, WINDOW_HEIGHT / 3);

        // Render player name text
        showCenteredText(renderer, font, playerName, textColor, WINDOW_HEIGHT / 2);

        SDL_RenderPresent(renderer);
    }

    SDL_StopTextInput();
}

// Check if the mouse is over a button
bool checkMouseOverButton(int mouseX, int mouseY, Button button) {
    return (mouseX >= button.rect.x && mouseX <= button.rect.x + button.rect.w &&
            mouseY >= button.rect.y && mouseY <= button.rect.y + button.rect.h);
}
void DesignPauseMenu(SDL_Renderer* renderer, TTF_Font* font, Button* buttons, int buttonCount, Mix_Chunk* hoverSound, Mix_Chunk* clickSound) {
    static int lastHoveredButton = -1;

    // Semi-transparent overlay
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 128);
    SDL_Rect overlay = {0, 0, WINDOW_WIDTH, WINDOW_HEIGHT};
    SDL_RenderFillRect(renderer, &overlay);

    // Pause title
   showCenteredText(renderer, font, "GAME PAUSED", (SDL_Color){255, 255, 255, 255}, WINDOW_HEIGHT / 4);

    // Draw buttons and handle hover sound
    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    for (int i = 0; i < buttonCount; i++) {
        bool isHovered = checkMouseOverButton(mouseX, mouseY, buttons[i]);
        DesignButton(renderer, font, buttons[i], isHovered);

        if (isHovered && lastHoveredButton != i) {
            Mix_PlayChannel(-1, hoverSound, 0);
            lastHoveredButton = i;
        }
    }

    SDL_RenderPresent(renderer);
}
void processPauseEvents(SDL_Event* event, Button* buttons, int buttonCount, bool* running, bool* isPaused, Mix_Chunk* clickSound) {
    int mouseX, mouseY;
    if (event->type == SDL_MOUSEBUTTONDOWN) {
        mouseX = event->button.x;
        mouseY = event->button.y;
        for (int i = 0; i < buttonCount; i++) {
            if (checkMouseOverButton(mouseX, mouseY, buttons[i])) {
                Mix_PlayChannel(-1, clickSound, 0);  // Play click sound
                if (strcmp(buttons[i].text, "Save Game") == 0) {
                   saveCurrentState();
                } else if (strcmp(buttons[i].text, "Resume (R or Click)") == 0) {
                    *isPaused = false;
                } else if (strcmp(buttons[i].text, "Quit to Menu (Q)") == 0) {
                    *running = false;
                }
            }
        }
    }
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_r) {
            *isPaused = false;
        } else if (event->key.keysym.sym == SDLK_q) {
            *running = false;
        }
    }
}

// Draw an animated title
void DesignAnimatedTitle(SDL_Renderer* renderer, TTF_Font* font, const char* title, int x, int y, int baseSize, Uint32 time) {
    int sizeOffset = (int)(10 * sin(time / 500.0));
    int fontSize = baseSize + sizeOffset;

    TTF_Font* tempFont = TTF_OpenFont("C:\\WINDOWS\\FONTS\\ARIAL.TTF", fontSize);
    if (!tempFont) {
        printf("Failed to load font for animation: %s\n", TTF_GetError());
        return;
    }
    SDL_Color color = {50, 50, 200, 255};
    SDL_Surface* textSurface = TTF_RenderText_Blended(tempFont, title, color);
    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
    SDL_Rect textRect = {
        x - textSurface->w / 2,
        y - textSurface->h / 2,
        textSurface->w,
        textSurface->h
    };
    SDL_RenderCopy(renderer, textTexture, NULL, &textRect);
    SDL_FreeSurface(textSurface);
    SDL_DestroyTexture(textTexture);
    TTF_CloseFont(tempFont);
}

// Show high scores
void DispalyHighScores(SDL_Renderer* renderer, TTF_Font* font) {
    bool showingScores = true;
    SDL_Event event;

    // Define the Quit button
    Button quitButton = {
        { (WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT - 100, BUTTON_WIDTH, BUTTON_HEIGHT },
        { 255, 186, 186, 255 },
        { 200, 150, 150, 255 },
        { 0, 0, 0, 255 },
        "Quit to Menu"
    };

    while (showingScores) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                showingScores = false;
                exit(0);
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                showingScores = false;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                if (checkMouseOverButton(mouseX, mouseY, quitButton)) {
                    showingScores = false;
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderClear(renderer);

        SDL_Color textColor = {0, 0, 0, 255};
        char scoreText[100];

        // Draw title
      showCenteredText(renderer, font, "High Scores", textColor, 50);

        for (int i = 0; i < MAX_SCORES; i++) {
            snprintf(scoreText, sizeof(scoreText),
                     "%d. %s: Score %d, Time %02d:%02d",
                     i+1,
                     highScores[i].name,
                     highScores[i].score,
                     highScores[i].time / 60,
                     highScores[i].time % 60);

           showCenteredText(renderer, font, scoreText, textColor, 150 + i * 50);
        }

        // Draw Quit button
       DesignButton(renderer, font, quitButton, false);

        SDL_RenderPresent(renderer);
    }
}
// Cleanup resources
void freeResources(SDL_Window* window, SDL_Renderer* renderer, TTF_Font* font, Mix_Chunk* hoverSound, Mix_Chunk* clickSound) {
    if (hoverSound) Mix_FreeChunk(hoverSound);
    if (clickSound) Mix_FreeChunk(clickSound);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (font) TTF_CloseFont(font);
    Mix_CloseAudio();
    TTF_Quit();
    SDL_Quit();
}

// D tecter la fin de jeu
bool isGameOver() {
    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            if (grid[i][j] == 0) return false;
            if (i > 0 && grid[i][j] == grid[i - 1][j]) return false;
            if (i < GRID_SIZE - 1 && grid[i][j] == grid[i + 1][j]) return false;
            if (j > 0 && grid[i][j] == grid[i][j - 1]) return false;
            if (j < GRID_SIZE - 1 && grid[i][j] == grid[i][j + 1]) return false;
        }
    }
    return true;
}

// V rifie si le joueur a gagner
bool isGameWon() {
    for (int i = 0; i < GRID_SIZE; ++i)
        for (int j = 0; j < GRID_SIZE; ++j)
            if (grid[i][j] == 2048)
                return true;
    return false;
}

// Ajouter une tuile alÃ©atoire
bool addNewTile(int grid[GRID_SIZE][GRID_SIZE]) {
    int emptyTiles[GRID_SIZE * GRID_SIZE][2];
    int count = 0;

    for (int i = 0; i < GRID_SIZE; ++i)
        for (int j = 0; j < GRID_SIZE; ++j)
            if (grid[i][j] == 0)
                emptyTiles[count][0] = i, emptyTiles[count++][1] = j;

    if (count == 0)
        return false;

    int index = rand() % count;
    int x = emptyTiles[index][0];
    int y = emptyTiles[index][1];
    grid[x][y] = (rand() % 2 + 1) * 2;

    return true;
}

// D placer les tuiles
bool slideTiles(int dx, int dy, int grid[GRID_SIZE][GRID_SIZE], int* score) {
    bool moved = false;
    bool merged[GRID_SIZE][GRID_SIZE] = {false};

    // Direction de parcours de la grille
    int startI = (dx > 0) ? GRID_SIZE - 1 : 0;
    int endI = (dx > 0) ? -1 : GRID_SIZE;
    int stepI = (dx > 0) ? -1 : 1;

    int startJ = (dy > 0) ? GRID_SIZE - 1 : 0;
    int endJ = (dy > 0) ? -1 : GRID_SIZE;
    int stepJ = (dy > 0) ? -1 : 1;

    // Un seul parcours de la grille
    for (int i = startI; i != endI; i += stepI) {
        for (int j = startJ; j != endJ; j += stepJ) {
            if (grid[i][j] == 0) continue;

            int x = i;
            int y = j;

            // Déplacer la tuile aussi loin que possible
            while (true) {
                int nx = x + dx;
                int ny = y + dy;

                // Vérifier les limites de la grille
                if (nx < 0 || nx >= GRID_SIZE || ny < 0 || ny >= GRID_SIZE) {
                    break;
                }

                // Case vide : on déplace
                if (grid[nx][ny] == 0) {
                    grid[nx][ny] = grid[x][y];
                    grid[x][y] = 0;
                    x = nx;
                    y = ny;
                    moved = true;
                }
                // Fusion possible
                else if (grid[nx][ny] == grid[x][y] && !merged[nx][ny] && !merged[x][y]) {
                    grid[nx][ny] *= 2;
                    *score += grid[nx][ny];
                    grid[x][y] = 0;
                    merged[nx][ny] = true;
                    moved = true;
                    break;
                }
                // Blocage
                else {
                    break;
                }
            }
        }
    }
    return moved;
}
// Fonction pour que la machine joue (mouvement al atoire)
int analyzePosition(int grid[GRID_SIZE][GRID_SIZE]) {
    int score = 0;
    int emptyCount = 0;

    // Count empty cells and calculate total value
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            if (grid[i][j] == 0) {
                emptyCount++;
            }
            score += grid[i][j];
        }
    }

    // Bonus for empty cells (mobility)
    score += emptyCount * 10;

    // Bonus for keeping high values in corners
    int corners[4][2] = {{0,0}, {0,GRID_SIZE-1}, {GRID_SIZE-1,0}, {GRID_SIZE-1,GRID_SIZE-1}};
    for (int i = 0; i < 4; i++) {
        score += grid[corners[i][0]][corners[i][1]] * 2;
    }

    return score;
}

// Improved AI move selection
void machinePlay(int grid[GRID_SIZE][GRID_SIZE], int* score) {
    int bestScore = -1;
    int bestMove = -1;
    int tempGrid[GRID_SIZE][GRID_SIZE];
    int tempScore = *score;

    // Try each possible move
    for (int move = 0; move < 4; move++) {
        // Copy grid for simulation
        memcpy(tempGrid, grid, sizeof(int) * GRID_SIZE * GRID_SIZE);

        bool moved = false;
        switch (move) {
            case 0: moved = slideTiles(-1, 0, tempGrid, &tempScore); break; // Up
            case 1: moved = slideTiles(1, 0, tempGrid, &tempScore); break;  // Down
            case 2: moved = slideTiles(0, -1, tempGrid, &tempScore); break; // Left
            case 3: moved = slideTiles(0, 1, tempGrid, &tempScore); break;  // Right
        }

        if (moved) {
            int moveScore = analyzePosition(tempGrid);
            if (moveScore > bestScore) {
                bestScore = moveScore;
                bestMove = move;
            }
        }
    }

    // Execute best move found
    if (bestMove != -1) {
        switch (bestMove) {
            case 0:slideTiles(-1, 0, grid, score); break;
            case 1: slideTiles(1, 0, grid, score); break;
            case 2: slideTiles(0, -1, grid, score); break;
            case 3: slideTiles(0, 1, grid, score); break;
        }
        addNewTile(grid);
    }
}
// Fonction pour initialiser le jeu en fonction du mode choisi
void prepareGameForMode(int mode) {
    if (mode == MODE_PLAYER_VS_MACHINE) {
        // Initialize both player and machine grids
        for (int i = 0; i < GRID_SIZE; ++i) {
            for (int j = 0; j < GRID_SIZE; ++j) {
                playerVsMachineGrid[0][i][j] = 0;  // Player grid
                playerVsMachineGrid[1][i][j] = 0;  // Machine grid
            }
        }
        // Add initial tiles for both grids
        for (int k = 0; k < 2; k++) {  // For both player and machine grids
            for (int i = 0; i < 2; i++) {  // Add two tiles to each grid
                int x = rand() % GRID_SIZE;
                int y = rand() % GRID_SIZE;
                while (playerVsMachineGrid[k][x][y] != 0) {
                    x = rand() % GRID_SIZE;
                    y = rand() % GRID_SIZE;
                }
                playerVsMachineGrid[k][x][y] = (rand() % 2 + 1) * 2;
            }
        }
        playerVsMachineScores[0] = 0;  // Reset player score
        playerVsMachineScores[1] = 0;  // Reset machine score
    } else {
        PrepareGrid();  // Original initialization for other modes
    }
    gameMode = mode;
    startTime = SDL_GetTicks();
    pausedTime = 0;
    totalPausedDuration = 0;
}


// Variables pour suivre la position de la souris ou du doigt
int startX = -1, startY = -1;
int endX = -1, endY = -1;
bool dragging = false;  // Indicateur pour savoir si l'utilisateur fait un glissement



// Fonction pour g rer le d but du glissement
void manageMouseDown(SDL_Event* event) {
    startX = event->button.x;
    startY = event->button.y;
    dragging = true;
}

// Fonction pour g rer la fin du glissement
// Assuming grid is a 2D array maintained globally or accessible within the context
extern int grid[GRID_SIZE][GRID_SIZE];
extern int score;

void manageMouseUp(SDL_Event* event) {
    if (dragging) {
        endX = event->button.x;
        endY = event->button.y;

        // Calculer la direction du glissement
        int deltaX = endX - startX;
        int deltaY = endY - startY;

        // Détecter si le mouvement est plus horizontal ou vertical
        if (abs(deltaX) > abs(deltaY)) {
            if (deltaX > 0) {
                // Glissement vers la droite
                slideTiles(0, 1, grid, &score);
            } else {
                // Glissement vers la gauche
                slideTiles(0, -1, grid, &score);
            }
        } else {
            if (deltaY > 0) {
                // Glissement vers le bas
               slideTiles(1, 0, grid, &score);
            } else {
                // Glissement vers le haut
               slideTiles(-1, 0, grid, &score);
            }
        }

        addNewTile(grid);  // Ajouter une nouvelle tuile après le déplacement
        dragging = false;
    }
}
void DesignPlayerVsMachineCell(SDL_Renderer* renderer, TTF_Font* font, Uint32 currentTime) {
    // Clear screen
    SDL_SetRenderDrawColor(renderer, 187, 173, 160, 255);
    SDL_RenderClear(renderer);

    // Calculate grid positions and sizes
    int gridSize = GRID_SIZE * (TILE_SIZE + 10) + 10;
    int firstGridX = (WINDOW_WIDTH / 4) - (gridSize / 2);
    int secondGridX = (3 * WINDOW_WIDTH / 4) - (gridSize / 2);
    int gridY = 80;

    // Draw player grid (left side)
    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            int x = firstGridX + j * (TILE_SIZE + 10);
            int y = gridY + i * (TILE_SIZE + 10);
            drawCell(renderer, font, playerVsMachineGrid[0][i][j], x, y);
        }
    }

    // Draw machine grid (right side)
    for (int i = 0; i < GRID_SIZE; ++i) {
        for (int j = 0; j < GRID_SIZE; ++j) {
            int x = secondGridX + j * (TILE_SIZE + 10);
            int y = gridY + i * (TILE_SIZE + 10);
           drawCell(renderer, font, playerVsMachineGrid[1][i][j], x, y);
        }
    }

    // Draw separator line matching grid height
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawLine(renderer,
        WINDOW_WIDTH / 2,
        gridY,
        WINDOW_WIDTH / 2,
        gridY + gridSize);

    // Draw scores and time
    SDL_Color textColor = {0, 0, 0, 255};
    char scoreText[100];
    snprintf(scoreText, sizeof(scoreText), "Player score: %d | Machine score: %d",
             playerVsMachineScores[0], playerVsMachineScores[1]);
    showCenteredText(renderer, font, scoreText, textColor, 20);

    char timeText[50];
    int gameTime = (currentTime - startTime - totalPausedDuration) / 1000;
    snprintf(timeText, sizeof(timeText), "Time: %02d:%02d", gameTime / 60, gameTime % 60);
showCenteredText(renderer, font, timeText, textColor, WINDOW_HEIGHT - 30);

    // Draw the pause button
    Button pauseButton = {
        { 10, WINDOW_HEIGHT - 80, 100, 50 },  // Position in bottom left
        { 200, 200, 200, 255 },  // Base color
        { 150, 150, 255, 255 },  // Hover color
        { 0, 0, 0, 255 },        // Text color
        "Pause"
    };
    DesignButton(renderer, font, pauseButton, false);

    SDL_RenderPresent(renderer);
}
void machineControlledMode(SDL_Renderer* renderer, TTF_Font* font, Mix_Chunk* hoverSound, Mix_Chunk* clickSound) {
    gameMode = MODE_MACHINE;
    // Reset game state
   PrepareGrid();
    score = 0;
    startTime = SDL_GetTicks();
    pausedTime = 0;
    totalPausedDuration = 0;

    SDL_Event event;
    bool running = true;
    Uint32 lastMoveTime = SDL_GetTicks();

    // Pause menu buttons
    Button pauseMenuButtons[] = {
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 - 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Save Game"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 255, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Resume (R or Click)"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 + 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {255, 186, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Quit to Menu (Q)"}
    };
    int pauseMenuButtonCount = sizeof(pauseMenuButtons) / sizeof(pauseMenuButtons[0]);

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                Button pauseButton = { { 10, WINDOW_HEIGHT - 80, 100, 50 }, { 200, 200, 200, 255 }, { 150, 150, 255, 255 }, { 0, 0, 0, 255 }, "Pause" };
                if (checkMouseOverButton(mouseX, mouseY, pauseButton)) {
                    isPaused = !isPaused;
                    if (isPaused) {
                        pausedTime = SDL_GetTicks();
                    } else {
                        totalPausedDuration += SDL_GetTicks() - pausedTime;
                    }
                    Mix_PlayChannel(-1, clickSound, 0);
                }
            }

            // Handle pause menu events
            if (isPaused) {
                processPauseEvents(&event, pauseMenuButtons, pauseMenuButtonCount, &running, &isPaused, clickSound);
            }
        }

        if (!isPaused) {
            // Make machine move every 1 second
            Uint32 currentTime = SDL_GetTicks();
            if (currentTime - lastMoveTime >= 1000) {
               machinePlay(grid, &score);
                lastMoveTime = currentTime;
            }

            // Draw the grid and score
            drawBoard(renderer, font, 60);  // Pass a y-offset value (e.g., 60) to move the grid down
            DisplayScore(renderer, font);
            SDL_RenderPresent(renderer);

            if (isGameOver()) {
                char playerName[MAX_NAME_LENGTH];
                strcpy(playerName, "Machine");  // For machine mode, use "Machine" as player name
                updateTopScores(playerName);
                SDL_Delay(2000);
                running = false;
            }
        } else {
          DesignPauseMenu(renderer, font, pauseMenuButtons, pauseMenuButtonCount, hoverSound, clickSound);
            SDL_RenderPresent(renderer);
        }

        SDL_Delay(1000 / FPS);
    }
}

// For Player vs Machine mode, we'll implement proper side-by-side grids

void PreparePlayerVsMachineMode() {
    // Initialize both grids
    for (int i = 0; i < GRID_SIZE; i++) {
        for (int j = 0; j < GRID_SIZE; j++) {
            playerVsMachineGrid[0][i][j] = 0;  // Player grid
            playerVsMachineGrid[1][i][j] = 0;  // Machine grid
        }
    }

    // Add initial tiles to both grids
    for (int k = 0; k < 2; k++) {
        addNewTile(playerVsMachineGrid[k]);
        addNewTile(playerVsMachineGrid[k]);
    }

    // Reset scores
    playerVsMachineScores[0] = 0;
    playerVsMachineScores[1] = 0;
}
// Fonction auxiliaire pour vérifier si un point est dans la grille du joueur
bool isInPlayerGrid(int x, int y) {
    // Ajustez ces valeurs selon votre mise en page
    int gridStartX = 50;  // Position X du début de la grille du joueur
    int gridEndX = WINDOW_WIDTH / 2 - 50;  // Position X de la fin de la grille du joueur
    int gridStartY = 60;  // Position Y du début de la grille
    int gridEndY = gridStartY + 400;  // Position Y de la fin de la grille (ajustez selon la taille)

    return (x >= gridStartX && x <= gridEndX && y >= gridStartY && y <= gridEndY);
}

void playerVsMachineMode(SDL_Renderer* renderer, TTF_Font* font, Mix_Chunk* hoverSound, Mix_Chunk* clickSound) {
    gameMode = MODE_PLAYER_VS_MACHINE;

    if (!isGameSaved) {
        PreparePlayerVsMachineMode();
        startTime = SDL_GetTicks();
    }
    pausedTime = 0;
    totalPausedDuration = 0;

    SDL_Event event;
    bool running = true;
    Uint32 lastMachineMove = SDL_GetTicks();

    // Variables pour le contrôle par souris
    bool dragging = false;
    int startX, startY;
    Uint32 lastDrawTime = 0;
    const Uint32 DRAW_INTERVAL = 1000 / 60;  // 60 FPS

    // Pause menu buttons
    Button pauseMenuButtons[] = {
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 - 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Save Game"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 255, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Resume (R or Click)"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 + 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {255, 186, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Quit to Menu (Q)"}
    };
    int pauseMenuButtonCount = sizeof(pauseMenuButtons) / sizeof(pauseMenuButtons[0]);

    while (running) {
        Uint32 currentTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }

            // Gestion du bouton pause
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                Button pauseButton = { { 10, WINDOW_HEIGHT - 80, 100, 50 }, { 200, 200, 200, 255 }, { 150, 150, 255, 255 }, { 0, 0, 0, 255 }, "Pause" };

                if (checkMouseOverButton(mouseX, mouseY, pauseButton)) {
                    isPaused = !isPaused;
                    if (isPaused) {
                        pausedTime = currentTime;
                    } else {
                        totalPausedDuration += currentTime - pausedTime;
                    }
                    Mix_PlayChannel(-1, clickSound, 0);
                } else if (!isPaused) {
                    // Début du glissement uniquement si on est dans la grille du joueur
                    if (isInPlayerGrid(mouseX, mouseY)) {
                        startX = mouseX;
                        startY = mouseY;
                        dragging = true;
                    }
                }
            }

            // Gestion de la fin du glissement
            if (event.type == SDL_MOUSEBUTTONUP && dragging && !isPaused) {
                int endX = event.button.x;
                int endY = event.button.y;
                int deltaX = endX - startX;
                int deltaY = endY - startY;

                // Seuil minimum pour détecter un glissement
                const int SWIPE_THRESHOLD = 50;
                bool moved = false;

                // Vérifier que le point de départ est dans la grille du joueur
                if (isInPlayerGrid(startX, startY)) {
                    if (abs(deltaX) > abs(deltaY) && abs(deltaX) > SWIPE_THRESHOLD) {
                        // Mouvement horizontal
                        if (deltaX > 0) {
                            moved = slideTiles(0, 1, playerVsMachineGrid[0], &playerVsMachineScores[0]);  // Droite
                        } else {
                            moved = slideTiles(0, -1, playerVsMachineGrid[0], &playerVsMachineScores[0]); // Gauche
                        }
                    } else if (abs(deltaY) > abs(deltaX) && abs(deltaY) > SWIPE_THRESHOLD) {
                        // Mouvement vertical
                        if (deltaY > 0) {
                            moved = slideTiles(1, 0, playerVsMachineGrid[0], &playerVsMachineScores[0]);  // Bas
                        } else {
                            moved = slideTiles(-1, 0, playerVsMachineGrid[0], &playerVsMachineScores[0]); // Haut
                        }
                    }

                    if (moved) {
                       addNewTile(playerVsMachineGrid[0]);
                    }
                }
                dragging = false;
            }

            // Support clavier
            if (!isPaused && event.type == SDL_KEYDOWN) {
                bool moved = false;
                switch (event.key.keysym.sym) {
                    case SDLK_UP:
                        moved = slideTiles(-1, 0, playerVsMachineGrid[0], &playerVsMachineScores[0]);
                        break;
                    case SDLK_DOWN:
                        moved = slideTiles(1, 0, playerVsMachineGrid[0], &playerVsMachineScores[0]);
                        break;
                    case SDLK_LEFT:
                        moved = slideTiles(0, -1, playerVsMachineGrid[0], &playerVsMachineScores[0]);
                        break;
                    case SDLK_RIGHT:
                        moved =slideTiles(0, 1, playerVsMachineGrid[0], &playerVsMachineScores[0]);
                        break;
                }
                if (moved) {
                    addNewTile(playerVsMachineGrid[0]);
                }
            }

            if (isPaused) {
               processPauseEvents(&event, pauseMenuButtons, pauseMenuButtonCount, &running, &isPaused, clickSound);
            }
        }

        if (currentTime - lastDrawTime >= DRAW_INTERVAL) {
            if (!isPaused) {
                // Machine moves every 1.5 seconds
                if (currentTime - lastMachineMove >= 1500) {
                  machinePlay(playerVsMachineGrid[1], &playerVsMachineScores[1]);
                    lastMachineMove = currentTime;
                }

                // Draw both grids
                DesignPlayerVsMachineCell(renderer, font, currentTime);

                // Check for game over conditions
                bool playerGameOver = isGameOver();
                bool machineGameOver = isGameOver();

                if (playerGameOver || machineGameOver) {
                    char winner[MAX_NAME_LENGTH];
                    if (playerGameOver && machineGameOver) {
                        if (playerVsMachineScores[0] > playerVsMachineScores[1]) {
                            strcpy(winner, "Player");
                        } else if (playerVsMachineScores[1] > playerVsMachineScores[0]) {
                            strcpy(winner, "Machine");
                        } else {
                            strcpy(winner, "Draw");
                        }
                    } else if (playerGameOver) {
                        strcpy(winner, "Machine");
                    } else {
                        strcpy(winner, "Player");
                    }

                   updateTopScores(winner);
                    SDL_Delay(2000);
                    running = false;
                }
            } else {
                DesignPauseMenu(renderer, font, pauseMenuButtons, pauseMenuButtonCount, hoverSound, clickSound);
            }

            SDL_RenderPresent(renderer);
            lastDrawTime = currentTime;
        }

        SDL_Delay(1);
    }
}
void playerMode(SDL_Renderer* renderer, TTF_Font* font, Mix_Chunk* hoverSound, Mix_Chunk* clickSound) {
    gameMode = MODE_PLAYER;
    if (!isGameSaved) {
        PrepareGrid();
        score = 0;
        startTime = SDL_GetTicks();
    }
    pausedTime = 0;
    totalPausedDuration = 0;

    SDL_Event event;
    bool running = true;
    bool dragging = false;
    int startX, startY;
    Uint32 lastDrawTime = 0;
    const Uint32 DRAW_INTERVAL = 1000 / 60;  // 60 FPS

    // Pause menu buttons remain the same
    Button pauseMenuButtons[] = {
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 - 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Save Game"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2, BUTTON_WIDTH, BUTTON_HEIGHT}, {186, 233, 255, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Resume (R or Click)"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, WINDOW_HEIGHT / 2 + 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {255, 186, 186, 255}, {0, 0, 0, 255}, {0, 0, 0, 255}, "Quit to Menu (Q)"}
    };
    int pauseMenuButtonCount = sizeof(pauseMenuButtons) / sizeof(pauseMenuButtons[0]);

    while (running) {
        Uint32 currentTime = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }

            // Gestion du bouton pause
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                int mouseX = event.button.x;
                int mouseY = event.button.y;
                Button pauseButton = { { 10, WINDOW_HEIGHT - 80, 100, 50 }, { 200, 200, 200, 255 }, { 150, 150, 255, 255 }, { 0, 0, 0, 255 }, "Pause" };

               if (checkMouseOverButton(mouseX, mouseY, pauseButton)) {
    isPaused = !isPaused;
    if (isPaused) {
        // When pausing, store the current time
        pauseStartTime = SDL_GetTicks();
    } else {
        // When resuming, add the paused duration to total
        totalPausedDuration += SDL_GetTicks() - pauseStartTime;
    }
    Mix_PlayChannel(-1, clickSound, 0);
        } else if (!isPaused) {
                    // Début du glissement
                    startX = mouseX;
                    startY = mouseY;
                    dragging = true;
                }
            }

            // Gestion de la fin du glissement
            if (event.type == SDL_MOUSEBUTTONUP && dragging && !isPaused) {
                int endX = event.button.x;
                int endY = event.button.y;
                int deltaX = endX - startX;
                int deltaY = endY - startY;

                // Seuil minimum pour détecter un glissement (pour éviter les clics accidentels)
                const int SWIPE_THRESHOLD = 50;
                bool moved = false;

                if (abs(deltaX) > abs(deltaY) && abs(deltaX) > SWIPE_THRESHOLD) {
                    // Mouvement horizontal
                    if (deltaX > 0) {
                        moved = slideTiles(0, 1, grid, &score);  // Droite
                    } else {
                        moved = slideTiles(0, -1, grid, &score); // Gauche
                    }
                } else if (abs(deltaY) > abs(deltaX) && abs(deltaY) > SWIPE_THRESHOLD) {
                    // Mouvement vertical
                    if (deltaY > 0) {
                        moved =slideTiles(1, 0, grid, &score);  // Bas
                    } else {
                        moved =slideTiles(-1, 0, grid, &score); // Haut
                    }
                }

                if (moved) {
                    addNewTile(grid);
                }
                dragging = false;
            }

            // Garder le support clavier existant
            if (!isPaused && event.type == SDL_KEYDOWN) {
                bool moved = false;
                switch (event.key.keysym.sym) {
                    case SDLK_UP: moved =slideTiles(-1, 0, grid, &score); break;
                    case SDLK_DOWN: moved = slideTiles(1, 0, grid, &score); break;
                    case SDLK_LEFT: moved = slideTiles(0, -1, grid, &score); break;
                    case SDLK_RIGHT: moved = slideTiles(0, 1, grid, &score); break;
                }
                if (moved) addNewTile(grid);
            }

            if (isPaused) {
               processPauseEvents(&event, pauseMenuButtons, pauseMenuButtonCount, &running, &isPaused, clickSound);
            }
        }

        // Le reste de la fonction reste inchangé...
        if (currentTime - lastDrawTime >= DRAW_INTERVAL) {
            if (!isPaused) {
                drawBoard(renderer, font, 60);
             DisplayScore(renderer, font);

                if (isGameOver()) {
                    char playerName[MAX_NAME_LENGTH];
                    getUserName(renderer, font, playerName);
                   updateTopScores(playerName);
                    SDL_Delay(2000);
                    running = false;
                }
            } else {
                DesignPauseMenu(renderer, font, pauseMenuButtons, pauseMenuButtonCount, hoverSound, clickSound);
            }

            SDL_RenderPresent(renderer);
            lastDrawTime = currentTime;
        }

        SDL_Delay(1);
    }
}
int main(int argc, char* argv[]) {
    // SDL Initialization
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("Failed to initialize SDL: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        printf("Failed to initialize SDL_ttf: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) != 0) {
        printf("Failed to initialize SDL_mixer: %s\n", Mix_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // Load high scores at the start of the game
   loadTopScores();

    // Create window and renderer
    SDL_Window* window = SDL_CreateWindow(
        "Number Slide", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        printf("Failed to create window: %s\n", SDL_GetError());
        freeResources(window, NULL, NULL, NULL, NULL);
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        printf("Failed to create renderer: %s\n", SDL_GetError());
        freeResources(window, NULL, NULL, NULL, NULL);
        return 1;
    }

    // Load font
    TTF_Font* font = TTF_OpenFont("arial.ttf", FONT_SIZE);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
       freeResources(window, renderer, NULL, NULL, NULL);
        return 1;
    }

    // Load sounds
    Mix_Chunk* hoverSound = Mix_LoadWAV("click.mp3"); //hover.wav
    Mix_Chunk* clickSound = Mix_LoadWAV("click.mp3");  //click.wav
    if (!hoverSound || !clickSound) {
        printf("Failed to load sounds: %s\n", Mix_GetError());
      freeResources(window, renderer, font, hoverSound, clickSound);
        return 1;
    }

    // Buttons
    Button buttons[] = {
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 100, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {150, 150, 255, 255}, {0, 0, 0, 255},"Player Mode"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 180, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {150, 255, 150, 255}, {0, 0, 0, 255},"Machine Mode"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 260, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {255, 150, 150, 255}, {0, 0, 0, 255},"Player vs Machine"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 340, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {255, 255, 150, 255}, {0, 0, 0, 255},"High Scores"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 420, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {150, 255, 255, 255}, {0, 0, 0, 255},"Quit"},
        {{(WINDOW_WIDTH - BUTTON_WIDTH) / 2, 500, BUTTON_WIDTH, BUTTON_HEIGHT}, {200, 200, 200, 255}, {255, 255, 150, 255},{0, 0, 0, 255},"Load Game"}
    };
    int buttonCount = sizeof(buttons) / sizeof(buttons[0]);
    srand((unsigned int)time(NULL));

    // Main loop
    bool running = true;
    bool menu = true;
    int mouseX = 0, mouseY = 0;
    SDL_Event event;
    int lastHoveredButton = -1;
    Uint32 lastMachineMove = SDL_GetTicks();
    Uint32 startTime = SDL_GetTicks();  // Time when the game starts

    while (running) {
        Uint32 startTicks = SDL_GetTicks();  // Frame timing

        if (menu) {
            // Handle menu events
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                    break;
                }
                if (event.type == SDL_MOUSEMOTION) {
                    mouseX = event.motion.x;
                    mouseY = event.motion.y;
                }
                if (event.type == SDL_MOUSEBUTTONDOWN) {
                    for (int i = 0; i < buttonCount; i++) {
                        if (checkMouseOverButton(mouseX, mouseY, buttons[i])) {
                            Mix_PlayChannel(-1, clickSound, 0);
                            if (i == 0) {  // Player Mode
                               menu = false;
                               playerMode(renderer, font, hoverSound, clickSound);
                               menu = true;  // Return to menu after game ends
                             } else if (i == 1) {  // Machine Mode
                               menu = false;
                              machineControlledMode(renderer, font, hoverSound, clickSound);
                               menu = true;  // Return to menu after game ends
                             } else if (i == 2) {  // Player vs Machine Mode
                               menu = false;
                               playerVsMachineMode(renderer, font, hoverSound, clickSound);
                               menu = true;  // Return to menu after game ends
                            } else if (i == 3) {
                                DispalyHighScores(renderer, font);
                                menu = true; // Ensure we return to the menu after showing high scores
                            } else if (i == 4) {
                                running = false;
                            } else if (i == 5) {  // Load Game button
                               if (RestoreGameState()) {
                                menu = false;
                                isPaused = false;
                                startTime = SDL_GetTicks();
                                pausedTime = 0;
                                totalPausedDuration = 0;

                              // Launch the appropriate game mode
                              switch (gameMode) {
                                 case MODE_PLAYER:
                                  playerMode(renderer, font, hoverSound, clickSound);
                                       break;
                                 case MODE_MACHINE:
                               machineControlledMode(renderer, font, hoverSound, clickSound);
                                       break;
                                 case MODE_PLAYER_VS_MACHINE:
                                  playerVsMachineMode(renderer, font, hoverSound, clickSound);
                                       break;
                            default:
                           // If game mode is invalid, return to menu
                              printf("Invalid game mode loaded\n");
                                menu = true;
                             break;
                                         }
                             menu = true;  // Return to menu after game ends
                                                          }
                                                            }
                        }
                    }
                }
            }
// Draw the menu
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderClear(renderer);
            DesignAnimatedTitle(renderer, font, "Number Slide", WINDOW_WIDTH / 2, 50, 50, SDL_GetTicks());
            for (int i = 0; i < buttonCount; i++) {
                bool isHovered = checkMouseOverButton(mouseX, mouseY, buttons[i]);
               DesignButton(renderer, font, buttons[i], isHovered);

                if (isHovered && lastHoveredButton != i) {
                    Mix_PlayChannel(-1, hoverSound, 0);
                    lastHoveredButton = i;
                }
            }
            SDL_RenderPresent(renderer);
        } else {
            // Handle game events
        }
    }

    // Cleanup and exit
   freeResources(window, renderer, font, hoverSound, clickSound);
    return 0;
}
