// Simple citro2d sprite drawing example
// Images borrowed from:
//   https://kenney.nl/assets/space-shooter-redux
#include <citro2d.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX_SPRITES   768
#define ENEMIES_WIDTH 5
#define ENEMIES_HEIGHT 2
#define ENEMIES_COUNT ENEMIES_WIDTH*ENEMIES_HEIGHT
#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define ENEMY_WIDTH 0.625
#define ENEMY_HEIGHT 0.625
#define ROTATION_FACTOR 0.02
#define ROTATION_SPEED 2

static C2D_SpriteSheet spriteSheet;
int tickCount;

// Simple sprite struct
typedef struct
{
	C2D_Sprite spr;
	float dx, dy; // velocity
} Sprite;

class Enemy {
	public: 
		float x;
		float y;
		float sx;
		float sy;
		bool hit;
		float angle;
		int hp;
		int type;
		int timeSinceHit;
		C2D_Sprite spr;
	// Enemy(int spawnX, int spawnY, int spawnType){
	// 	x = spawnX;
	// 	y = spawnY;
	// 	type = spawnType;
	// }

};
Enemy enemies[ENEMIES_HEIGHT*ENEMIES_WIDTH];




//---------------------------------------------------------------------------------
// static void initSprites() {
// //---------------------------------------------------------------------------------
// 	size_t numImages = C2D_SpriteSheetCount(spriteSheet);
// 	srand(time(NULL));

// 	for (size_t i = 0; i < MAX_SPRITES; i++)
// 	{
// 		Sprite* sprite = &sprites[i];

// 		// Random image, position, rotation and speed
// 		C2D_SpriteFromSheet(&sprite.spr, spriteSheet, rand() % numImages);
// 		C2D_SpriteSetCenter(&sprite.spr, 0.5f, 0.5f);
// 		C2D_SpriteSetPos(&sprite.spr, rand() % SCREEN_WIDTH, rand() % SCREEN_HEIGHT);
// 		C2D_SpriteSetRotation(&sprite.spr, C3D_Angle(rand()/(float)RAND_MAX));
// 		sprite.dx = rand()*4.0f/RAND_MAX - 2.0f;
// 		sprite.dy = rand()*4.0f/RAND_MAX - 2.0f;
// 	}
// }

static void spawnEnemies(){
	for (int x = 0; x<ENEMIES_WIDTH;x++){
		for (int y = 0; y<ENEMIES_HEIGHT;y++){
			Enemy* currentEnemy = &enemies[x+y*ENEMIES_WIDTH];
			currentEnemy->x = x*40+20;
			currentEnemy->y = y*40+20;
			C2D_SpriteFromSheet(&currentEnemy->spr, spriteSheet, 0);
			C2D_SpriteSetCenter(&currentEnemy->spr, 0.5f, 0.5f);
 			C2D_SpriteSetPos(&currentEnemy->spr, currentEnemy->x, currentEnemy->y);
			C2D_SpriteSetScale(&currentEnemy->spr, ENEMY_WIDTH, ENEMY_HEIGHT);
		}
	}
}

//---------------------------------------------------------------------------------
static void updateEnemies() {
//---------------------------------------------------------------------------------
	for (int i = 0;i<ENEMIES_COUNT;i++){
		Enemy* currentEnemy = &enemies[i];
		C2D_Sprite* sprite = &currentEnemy->spr;
		C2D_SpriteSetPos(sprite,currentEnemy->x,currentEnemy->y);
		C2D_SpriteSetRotation(&currentEnemy->spr, sin(tickCount*ROTATION_SPEED)*ROTATION_FACTOR);
	}
}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	tickCount = 0;
	// Init libs
	romfsInit();
	gfxInitDefault();
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	consoleInit(GFX_BOTTOM, NULL);

	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);

	// Load graphics
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	// Initialize sprites
	spawnEnemies();

	printf("\x1b[8;1HPress Up to increment sprites");
	printf("\x1b[9;1HPress Down to decrement sprites");

	// Main loop
	while (aptMainLoop())
	{
		tickCount ++;

		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu


		updateEnemies();

		//printf("\x1b[1;1HSprites: %zu/%u\x1b[K", numSprites, MAX_SPRITES);
		printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime()*6.0f);
		printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime()*6.0f);
		printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage()*100.0f);

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
		C2D_SceneBegin(top);
		for (size_t i = 0; i < ENEMIES_COUNT; i ++)
			C2D_DrawSprite(&enemies[i].spr);
		C3D_FrameEnd(0);
	}

	// Delete graphics
	C2D_SpriteSheetFree(spriteSheet);

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
