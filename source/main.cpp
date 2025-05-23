// Simple citro2d sprite drawing example
// Images borrowed from:
//   https://kenney.nl/assets/space-shooter-redux
#include <citro2d.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <random>

#define MAX_SPRITES   768
#define ENEMIES_WIDTH 9
#define ENEMIES_HEIGHT 3
#define ENEMIES_COUNT ENEMIES_WIDTH*ENEMIES_HEIGHT
#define SCREEN_WIDTH  400
#define SCREEN_HEIGHT 240
#define ENEMY_WIDTH 0.625
#define ENEMY_HEIGHT 0.625
#define ROTATION_FACTOR 0.02
#define ROTATION_SPEED 2
#define BALL_SIZE 20.0
#define PADDLE_SIZE 64.0
#define HORIZONTAL_PADDING 20
#define VERTICAL_PADDING 20
#define BOUNCE_SIZE 0.2
#define BOUNCE_SPEED 0.5



static C2D_SpriteSheet spriteSheet;
static C2D_SpriteSheet backgroundSheet;
//static C2D_SpriteSheet numberSheet;
int tickCount;

float randRange(float l,float u){
	srand(time(0)+tickCount-sin(tickCount)+tan(tickCount));
	float n = ((float)(rand()%32767))/(32767.0);
	n*= (u-l);
	n += l;
	return n;
}
float dist(float x, float y){
	return (float)sqrt(x*x+y*y);
}

// Simple sprite struct
typedef struct
{
	C2D_Sprite spr;
	float dx, dy; // velocity
} Sprite;

class Ball{
	public:
		float x;
		float y;
		float sx;
		float sy;
		C2D_Sprite spr;
	Ball(float ix, float iy, float isx, float isy){
		x = ix;
		y = iy;
		sx = isx;
		sy = isy;
	}
};

class Paddle{
	public:
	float x;
	float y;
	float sx;
	float sy;
	float speed;
	C2D_Sprite spr;
	Paddle(float iSpeed){
		x = SCREEN_WIDTH/2;
		y = SCREEN_HEIGHT-20;
		speed = iSpeed;
	}
};

class Enemy {
	public: 
		float x;
		float y;
		float sx;
		float sy;
		bool hit;
		float angle;
		float rotationSpeed;
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
Ball playerBall = Ball(SCREEN_WIDTH/2,SCREEN_HEIGHT/2,0,3);
Paddle playerPaddle = Paddle(1);
C2D_Sprite backgroundSprite;

static void spawnEnemies(){
	for (int x = 0; x<ENEMIES_WIDTH;x++){
		for (int y = 0; y<ENEMIES_HEIGHT;y++){
			Enemy* currentEnemy = &enemies[x+y*ENEMIES_WIDTH];
			currentEnemy->x = x*40+20+HORIZONTAL_PADDING;
			currentEnemy->y = y*40+20+VERTICAL_PADDING;
			currentEnemy->hp = 1;
			currentEnemy->hit = false;
			currentEnemy->angle = 0;
			currentEnemy->rotationSpeed = 0;
			currentEnemy->timeSinceHit = 100;
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
		currentEnemy->timeSinceHit ++;
		currentEnemy->x+= currentEnemy->sx;
		currentEnemy->y+= currentEnemy->sy;
		currentEnemy->angle+= currentEnemy->rotationSpeed;
		C2D_Sprite* sprite = &currentEnemy->spr;
		C2D_SpriteSetPos(sprite,currentEnemy->x,currentEnemy->y);
		C2D_SpriteSetRotation(&currentEnemy->spr, sin(tickCount*ROTATION_SPEED)*ROTATION_FACTOR+currentEnemy->angle);
		float bounceFactor = (BOUNCE_SIZE/(1 + ((float)currentEnemy->timeSinceHit)*((float)BOUNCE_SPEED) )) ;
		C2D_SpriteSetScale(&currentEnemy->spr, ENEMY_WIDTH-bounceFactor, ENEMY_HEIGHT-bounceFactor);
		if (currentEnemy->hp<1){
			if (!currentEnemy->hit){
			 	currentEnemy->hit = true;
				currentEnemy->rotationSpeed = randRange(-0.05,0.05);
				currentEnemy->sx = randRange(-2,2);
				currentEnemy->sy = randRange(-2,2);
				currentEnemy->timeSinceHit = 0;
			}
			currentEnemy->sy +=0.25;
		}
	}
}

static void updatePlayer() {
	playerBall.x += playerBall.sx;
	playerBall.y += playerBall.sy;
	if (playerBall.x-BALL_SIZE/2 < HORIZONTAL_PADDING&&playerBall.sx<0){
		playerBall.sx *=-1;
	}
	if (playerBall.x+BALL_SIZE/2 > SCREEN_WIDTH-HORIZONTAL_PADDING&&playerBall.sx>0){
		playerBall.sx *=-1;
	}
	if (playerBall.y-BALL_SIZE/2 < 20&&playerBall.sy < 0){
		playerBall.sy *=-1;
	}
	playerPaddle.x += playerPaddle.sx;
	playerPaddle.sx *=0.8;


	C2D_SpriteSetPos(&playerPaddle.spr, playerPaddle.x, playerPaddle.y);
	C2D_SpriteSetPos(&playerBall.spr, playerBall.x, playerBall.y);
	C2D_SpriteSetRotation(&playerBall.spr, (float)tickCount/10);

	//---------------------------------------------------------------------------------
		for (int i = 0;i<ENEMIES_COUNT;i++){
			if (!enemies[i].hit){
				float x = enemies[i].x;
				float y = enemies[i].y;
				bool alreadyFlipped = false;
				if (fabs(x-playerBall.x)<20){
					if (fabs(y-(playerBall.y+playerBall.sy))<20){
						playerBall.sy *=-1;
						alreadyFlipped = true;
						enemies[i].hp --;
					}
				}
				if (fabs(y-playerBall.y)<20&&!alreadyFlipped){
					if (fabs(x-(playerBall.x+playerBall.sx))<20){
						playerBall.sx *=-1;
						enemies[i].hp --;
					}
				}
			}
		}
		if (playerBall.y+BALL_SIZE/2>playerPaddle.y&&playerBall.y<playerPaddle.y+16){
			if (fabs(playerBall.x-playerPaddle.x)<PADDLE_SIZE/2&&playerBall.sy > 0){
				float mag = dist(playerBall.sx,playerBall.sy);
				playerBall.sy*=-1;
				playerBall.sx += playerPaddle.sx*0.6;
				float ballAngle = atan2(playerBall.sy,playerBall.sx);
				playerBall.sx = cos(ballAngle)*mag;
				playerBall.sy = sin(ballAngle)*mag;
			}
		}
		if (playerPaddle.x-PADDLE_SIZE/2<HORIZONTAL_PADDING){
			playerPaddle.x = HORIZONTAL_PADDING+PADDLE_SIZE/2;
			playerPaddle.sx *=-0.7;
		}
		if (playerPaddle.x+PADDLE_SIZE/2>SCREEN_WIDTH-HORIZONTAL_PADDING){
			playerPaddle.x = SCREEN_WIDTH-(HORIZONTAL_PADDING+PADDLE_SIZE/2);
			playerPaddle.sx *=-0.7;
		}


	}

//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------
	tickCount = 0;
	// Init libs
	srand(time(0));

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
	backgroundSheet = C2D_SpriteSheetLoad("romfs:/gfx/backgrounds.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	// Initialize sprites
	spawnEnemies();

	C2D_SpriteFromSheet(&backgroundSprite,backgroundSheet,0);

	//create player ball
	C2D_SpriteFromSheet(&playerBall.spr, spriteSheet, 1);
	C2D_SpriteSetCenter(&playerBall.spr, 0.5f, 0.5f);
	C2D_SpriteSetPos(&playerBall.spr, playerBall.x, playerBall.y);
	C2D_SpriteSetScale(&playerBall.spr, (BALL_SIZE/64.0), (BALL_SIZE/64.0));
	//C2D_SpriteSetScale(&playerBall.spr, 1, 1);

	//create player paddle
	C2D_SpriteFromSheet(&playerPaddle.spr, spriteSheet, 2);
	C2D_SpriteSetCenter(&playerPaddle.spr, 0.5f, 0.5f);
	C2D_SpriteSetPos(&playerPaddle.spr, playerPaddle.x, playerPaddle.y);
	C2D_SpriteSetScale(&playerPaddle.spr, (PADDLE_SIZE/128.0),(PADDLE_SIZE/128.0));


	// Main loop
	while (aptMainLoop())
	{
		tickCount ++;

		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START)
			break; // break in order to return to hbmenu


		updateEnemies();
		updatePlayer();
		if (kHeld&KEY_DLEFT||kHeld&KEY_CPAD_LEFT){
			playerPaddle.sx -= playerPaddle.speed;
		}
		if (kHeld&KEY_DRIGHT||kHeld&KEY_CPAD_RIGHT){
			playerPaddle.sx += playerPaddle.speed;
		}

		//printf("\x1b[1;1HSprites: %zu/%u\x1b[K", numSprites, MAX_SPRITES);
		printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime()*6.0f);
		printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime()*6.0f);
		printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage()*100.0f);
		printf("\x1b[5;1HSX		  %6.2f%%\x1b[K", playerBall.sx);
		printf("\x1b[6;1HSY		  %6.2f%%\x1b[K", playerBall.sy);
		printf("\x1b[8;1HSY		  %6.2f%%\x1b[K", randRange(-0.01,0.01));
		

		// Render the scene
		C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
		C2D_TargetClear(top, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
		C2D_SceneBegin(top);
		C2D_DrawSprite(&backgroundSprite);
		for (size_t i = 0; i < ENEMIES_COUNT; i ++)
			C2D_DrawSprite(&enemies[i].spr);
		C2D_DrawSprite(&playerBall.spr);
		C2D_DrawSprite(&playerPaddle.spr);
		C3D_FrameEnd(0);
	}

	// Delete graphics
	C2D_SpriteSheetFree(spriteSheet);
	C2D_SpriteSheetFree(backgroundSheet);

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	return 0;
}
int sign(float x) {
	return fabs(x)/x;
  }
