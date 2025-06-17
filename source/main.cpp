// Simple citro2d sprite drawing example
// Images borrowed from:
//   https://kenney.nl/assets/space-shooter-redux
#include <citro2d.h>

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <3ds.h>
#include <random>

#define MAX_SPRITES   768
#define ENEMIES_WIDTH 9
#define ENEMIES_HEIGHT 3
#define ENEMIES_COUNT ENEMIES_WIDTH*ENEMIES_HEIGHT
#define SCREEN_WIDTH_TOP  400
#define SCREEN_HEIGHT_TOP 240
#define SCREEN_WIDTH_BOTTOM  320
#define SCREEN_HEIGHT_BOTTOM 240
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
#define ENEMY_SPAWN_TIME 5
#define SPAWN_ANIM_TIME 100
#define EXPLOSION_DIST 70	

#define SAMPLERATE 22050 //all this audio gartrash
#define SAMPLESPERBUF (SAMPLERATE / 30)
#define BYTESPERSAMPLE 4
#define MAX_SOUNDS 5

class AudioStream {
public:
    FILE* file;
    size_t dataStart;
    u32* buffer;
    ndspWaveBuf waveBuf[2];
    bool fillBlock;
};

AudioStream sounds[MAX_SOUNDS];
int loopPoint = 0;
FILE* musicFile = NULL;  // music files
size_t musicDataStart = 0;

int loopPoint2 = 0;
FILE* musicFile2 = NULL;  // music files
size_t musicDataStart2 = 0;


void WavBufferFill(void *buffer, size_t size, FILE* file) {
	if (!file) return;
	size_t bytesRead = fread(buffer, 1, size, file);
	int loopPoint = 0;
	if (bytesRead < size) {
		// Loop back!
		fseek(file, loopPoint, SEEK_SET);
		fread((u8*)buffer + bytesRead, 1, size - bytesRead, file);
	}
	DSP_FlushDataCache(buffer, size);
}

int parse_wav_header(FILE *f, size_t *dataStart) {
	char chunkId[5] = {0}, format[5] = {0};
	u32 chunkSize;

	fread(chunkId, 1, 4, f);
	fread(&chunkSize, 4, 1, f);
	fread(format, 1, 4, f);

	if (strncmp(chunkId, "RIFF", 4) != 0 || strncmp(format, "WAVE", 4) != 0)
		return -1;

	while (!feof(f)) {
		char subChunkId[5] = {0};
		u32 subChunkSize = 0;

		fread(subChunkId, 1, 4, f);
		fread(&subChunkSize, 4, 1, f);

		if (strncmp(subChunkId, "data", 4) == 0) {
			*dataStart = ftell(f);
			return 0;
		} else {
			fseek(f, subChunkSize, SEEK_CUR);
		}
	}
	return -1;
}

bool loadWav(AudioStream* stream, const char* path, int channel) {
	stream->file = fopen(path, "rb");
	if (!stream->file || parse_wav_header(stream->file, &stream->dataStart) < 0) return false;

	fseek(stream->file, stream->dataStart, SEEK_SET);
	stream->buffer = (u32*)linearAlloc(SAMPLESPERBUF * BYTESPERSAMPLE * 2);
	memset(stream->waveBuf, 0, sizeof(stream->waveBuf));
	stream->waveBuf[0].data_vaddr = &stream->buffer[0];
	stream->waveBuf[1].data_vaddr = &stream->buffer[SAMPLESPERBUF];
	stream->waveBuf[0].nsamples = SAMPLESPERBUF;
	stream->waveBuf[1].nsamples = SAMPLESPERBUF;
	stream->fillBlock = false;

	WavBufferFill(stream->buffer, SAMPLESPERBUF * BYTESPERSAMPLE * 2,stream->file);
	ndspChnWaveBufAdd(channel, &stream->waveBuf[0]);
	ndspChnWaveBufAdd(channel, &stream->waveBuf[1]);

	return true;
}

static C2D_SpriteSheet spriteSheet;
static C2D_SpriteSheet backgroundSheet;
static C2D_Sprite bgEnemy;
static C2D_Sprite titleSprite;
static C2D_Sprite lifeSprite;
int levelNo;
C2D_TextBuf g_staticBuf;
C2D_Text g_staticText[10];
C2D_Font gameFont;
//static C2D_SpriteSheet numberSheet;
int tickCount;
int gameState;
int defeatedCount;
bool endless;
int randCount = 0;
bool paused;
int loseTime;

float randRange(float l,float u){
	//srand(time(0)+tickCount-sin((float)tickCount-4*(randCount))+tan((float)tickCount+4*(randCount))+randCount);
	float n = ((float)(rand()%32767))/(32767.0);
	randCount++;
	n*= (u-l);
	n += l;
	return n;
}
float dist(float x, float y){
	return (float)sqrt(x*x+y*y);
}
int sign(float x) {
	return fabs(x)/x;
  }
int clamp(float val, float minVal, float maxVal) {
    if (val < minVal)
		val = minVal;
	if (val > maxVal)
		val = maxVal;
	return val;
}
void updateLevelText(){
	C2D_TextBufClear(g_staticBuf);
	std::string str = "Level "+ std::to_string(levelNo);
	C2D_TextFontParse (&g_staticText[0],gameFont, g_staticBuf, str.c_str());
	str = "Pause";
	C2D_TextFontParse (&g_staticText[1],gameFont, g_staticBuf, str.c_str());
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
		int timer;
		int lives;
	Ball(float ix, float iy, float isx, float isy){
		x = ix;
		y = iy;
		sx = isx;
		sy = isy;
		timer = -1;
		lives = 5;
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
		x = SCREEN_WIDTH_TOP/2;
		y = SCREEN_HEIGHT_TOP-20;
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
		int spawnTime;
		int transformTime;
		C2D_Sprite spr;
	// Enemy(int spawnX, int spawnY, int spawnType){
	// 	x = spawnX;
	// 	y = spawnY;
	// 	type = spawnType;
	// }

};
Enemy enemies[ENEMIES_HEIGHT*ENEMIES_WIDTH];
Ball playerBall = Ball(SCREEN_WIDTH_TOP/2,SCREEN_HEIGHT_TOP-10,0,3);
Paddle playerPaddle = Paddle(1);
C2D_Sprite backgroundSprite;

static void spawnEnemies(){
	srand(time(NULL));
	for (int x = 0; x<ENEMIES_WIDTH;x++){
		for (int y = 0; y<ENEMIES_HEIGHT;y++){
			Enemy* currentEnemy = &enemies[x+y*ENEMIES_WIDTH];
			currentEnemy->x = x*40+20+HORIZONTAL_PADDING;
			currentEnemy->y = y*40+20+VERTICAL_PADDING;
			currentEnemy->sx = 0;
			currentEnemy->sy = 0;
			currentEnemy->hp = 1;
			currentEnemy->spawnTime = -x*ENEMY_SPAWN_TIME;
			currentEnemy->hit = false;
			currentEnemy->angle = 0;
			currentEnemy->rotationSpeed = 0;
			currentEnemy->timeSinceHit = 100;
			currentEnemy->type = 0;
			currentEnemy->transformTime = -1;
			if (levelNo>1){
				if (rand()%1000>920){
					currentEnemy->type = 1;
				}
			}
			if (levelNo>3){
				if (rand()%1000>900){
					currentEnemy->type = 2;
				}
			}
			if (levelNo>9){
				if (rand()%1000>900){
					currentEnemy->type = 3;
				}
			}
			int spriteToUse = 3;
			switch (currentEnemy->type){
				case 0:
					spriteToUse = 3;
					break;
				case 1:
					spriteToUse = 4;
					break;
				case 2:
					spriteToUse = 5;
					currentEnemy->hp = 2;
					break;
				case 3:
					spriteToUse = 6;
					break;
			}
			C2D_SpriteFromSheet(&currentEnemy->spr, spriteSheet, spriteToUse);
			C2D_SpriteSetCenter(&currentEnemy->spr, 0.5f, 0.5f);
 			C2D_SpriteSetPos(&currentEnemy->spr, currentEnemy->x, currentEnemy->y);
			C2D_SpriteSetScale(&currentEnemy->spr, ENEMY_WIDTH, ENEMY_HEIGHT);
		}
	}
}

//---------------------------------------------------------------------------------
static void updateEnemies() {
//---------------------------------------------------------------------------------
	defeatedCount = 0;
	for (int i = 0;i<ENEMIES_COUNT;i++){
		Enemy* currentEnemy = &enemies[i];
		currentEnemy->spawnTime ++;
		currentEnemy->timeSinceHit ++;
		currentEnemy->x+= currentEnemy->sx;
		currentEnemy->y+= currentEnemy->sy;
		currentEnemy->angle+= currentEnemy->rotationSpeed;
		C2D_Sprite* sprite = &currentEnemy->spr;
		C2D_SpriteSetPos(sprite,currentEnemy->x,currentEnemy->y);
		C2D_SpriteSetRotation(&currentEnemy->spr, sin((float)tickCount*ROTATION_SPEED+i)*ROTATION_FACTOR+currentEnemy->angle);
		float bounceFactor = (BOUNCE_SIZE/(1 + ((float)currentEnemy->timeSinceHit)*((float)BOUNCE_SPEED) )) ;
		if (currentEnemy->spawnTime>0){
			float spawnFactor = sqrt(currentEnemy->spawnTime*0.1);
			if (spawnFactor>1){
				spawnFactor = 1;
			}
			if (spawnFactor<0){
				spawnFactor = 0;
			}
			C2D_SpriteSetScale(&currentEnemy->spr, (ENEMY_WIDTH-bounceFactor)*spawnFactor, (ENEMY_HEIGHT-bounceFactor)*spawnFactor);
		}
		if (currentEnemy->hp<1){
			if (currentEnemy->y>SCREEN_HEIGHT_TOP+100){
				defeatedCount++;
			}
			if (!currentEnemy->hit){
			 	currentEnemy->hit = true;
				currentEnemy->rotationSpeed = randRange(-0.05,0.05);
				currentEnemy->sx = randRange(-2,2);
				currentEnemy->sy = randRange(-2,2);
				switch (currentEnemy->type){
					case 1:
						for (int x = 0;x<ENEMIES_COUNT;x++){
							Enemy* comparedEnemy = &enemies[x];
							float x1 = currentEnemy->x;
							float y1 = currentEnemy->y;
							float x2 = comparedEnemy->x;
							float y2 = comparedEnemy->y;
							if (sqrt(pow(x1-x2,2)+pow(y1-y2,2))<EXPLOSION_DIST){
								if (comparedEnemy->type == 3){
									comparedEnemy->transformTime = 60;
								}
								else{
									comparedEnemy->hp -=5;
								}
							}
						}
				}
			}
			currentEnemy->sy +=0.25;
		}
		if (currentEnemy->transformTime>0){
			currentEnemy->transformTime --;
			switch (currentEnemy->type){
				case 3:
					C2D_SpriteSetRotation(&currentEnemy->spr, sin((float)tickCount*ROTATION_SPEED+i)*ROTATION_FACTOR*1/(0.01*currentEnemy->transformTime)+currentEnemy->angle);
					break;
			}
		}
			if (currentEnemy->transformTime==0){
				switch (currentEnemy->type){
					case 3:
						C2D_SpriteFromSheet(&currentEnemy->spr, spriteSheet, 3);
						C2D_SpriteSetPos(&currentEnemy->spr,currentEnemy->x,currentEnemy->y);
						C2D_SpriteSetRotation(&currentEnemy->spr, sin((float)tickCount*ROTATION_SPEED+i)*ROTATION_FACTOR+currentEnemy->angle);
						C2D_SpriteSetCenter(&currentEnemy->spr, 0.5f, 0.5f);
						currentEnemy->type = 0;
						break;
			}
		}
	}
	if (defeatedCount==ENEMIES_COUNT){
		levelNo++;
		spawnEnemies();
		updateLevelText();
	}
}

bool hitEnemy(int index){
	if (enemies[index].type == 3){
		if (rand()%200>197){
			enemies[index].transformTime = 60;
		}
		return false;
	}
	else{
		enemies[index].hp --;
		enemies[index].timeSinceHit = 0;
		return true;
	}	
}

static void updatePlayer() {
	playerBall.x += playerBall.sx;
	playerBall.y += playerBall.sy;
	if (playerBall.x-BALL_SIZE/2 < HORIZONTAL_PADDING&&playerBall.sx<0){
		playerBall.sx *=-1;
	}
	if (playerBall.x+BALL_SIZE/2 > SCREEN_WIDTH_TOP-HORIZONTAL_PADDING&&playerBall.sx>0){
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
			if (!enemies[i].hit&&enemies[i].spawnTime>0){
				float x = enemies[i].x;
				float y = enemies[i].y;
				bool alreadyFlipped = false;
				if (fabs(x-playerBall.x)<20){
					if (fabs(y-(playerBall.y+playerBall.sy))<20){
						if (hitEnemy(i)){
							playerBall.sy *=-1;
							alreadyFlipped = true;
						}
					}
				}
				if (fabs(y-playerBall.y)<20&&!alreadyFlipped){
					if (fabs(x-(playerBall.x+playerBall.sx))<20){
						if (hitEnemy(i)){
							playerBall.sx *=-1;
							alreadyFlipped = true;
						}
					}
				}
			}
		}
		if (playerBall.y+BALL_SIZE/2>playerPaddle.y&&playerBall.y<playerPaddle.y+16){
			if (fabs(playerBall.x-playerPaddle.x)<PADDLE_SIZE/2&&playerBall.sy > 0){
				float mag = dist(playerBall.sx,playerBall.sy);
				playerBall.sy*=-1;
				playerBall.sx *=0.6;
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
		if (playerPaddle.x+PADDLE_SIZE/2>SCREEN_WIDTH_TOP-HORIZONTAL_PADDING){
			playerPaddle.x = SCREEN_WIDTH_TOP-(HORIZONTAL_PADDING+PADDLE_SIZE/2);
			playerPaddle.sx *=-0.7;
		}
		if ((playerBall.y>SCREEN_HEIGHT_TOP+20)&&(playerBall.lives>0)){
			C2D_SpriteSetPos(&playerBall.spr, playerPaddle.x, playerPaddle.y-20);
		}
		if (playerBall.y>SCREEN_HEIGHT_TOP+200){
			playerBall.y = playerPaddle.y-20;
			playerBall.x = playerPaddle.x;
			playerBall.sx = 0;
			playerBall.sy = 3;
			playerBall.lives --;
			if (playerBall.lives<0){
				loseTime = tickCount*0.2+20;
			}
		}


	}

	void InitGame(){
		tickCount = 0;
		gameState = 0;
		levelNo = 1;

		C2D_SpriteFromSheet(&bgEnemy, spriteSheet, 3);
		C2D_SpriteSetCenter(&bgEnemy, 0.5f, 0.5f);
		C2D_SpriteSetPos(&bgEnemy, 0, 0);
		C2D_SpriteSetScale(&bgEnemy, 0.7, 0.7);
		C2D_SpriteFromSheet(&titleSprite, spriteSheet, 2);
		C2D_SpriteSetCenter(&titleSprite, 0.5f, 0.5f);
		C2D_SpriteSetPos(&titleSprite, SCREEN_WIDTH_TOP/2, SCREEN_HEIGHT_TOP/2-50);
		C2D_SpriteSetScale(&titleSprite, 1.8, 1.8);

		// Initialize sprites
		spawnEnemies();

		C2D_SpriteFromSheet(&backgroundSprite,backgroundSheet,0);
		C2D_SpriteFromSheet(&lifeSprite,spriteSheet,0);
		C2D_SpriteSetCenter(&lifeSprite, 0.5f, 0.5f);
		C2D_SpriteSetPos(&lifeSprite, SCREEN_WIDTH_TOP/2,SCREEN_HEIGHT_TOP/2);
		C2D_SpriteSetScale(&lifeSprite, 0.5f, 0.5f);

		//create player ball
		C2D_SpriteFromSheet(&playerBall.spr, spriteSheet, 0);
		C2D_SpriteSetCenter(&playerBall.spr, 0.5f, 0.5f);
		C2D_SpriteSetPos(&playerBall.spr, playerBall.x, playerBall.y);
		C2D_SpriteSetScale(&playerBall.spr, (BALL_SIZE/64.0), (BALL_SIZE/64.0));
		//C2D_SpriteSetScale(&playerBall.spr, 1, 1);

		//create player paddle
		C2D_SpriteFromSheet(&playerPaddle.spr, spriteSheet, 1);
		C2D_SpriteSetCenter(&playerPaddle.spr, 0.5f, 0.5f);
		C2D_SpriteSetPos(&playerPaddle.spr, playerPaddle.x, playerPaddle.y);
		C2D_SpriteSetScale(&playerPaddle.spr, (PADDLE_SIZE/128.0),(PADDLE_SIZE/128.0));
	}



//---------------------------------------------------------------------------------
int main(int argc, char* argv[]) {
//---------------------------------------------------------------------------------

	// Init libs
	srand(time(0));

	romfsInit();
	gfxInitDefault();
	cfguInit(); // Allow C2D_FontLoadSystem to work
	C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
	C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
	C2D_Prepare();
	ndspWaveBuf waveBuf[2];
	ndspWaveBuf waveBuf2[2];

	//consoleInit(GFX_BOTTOM, NULL);

	// Create screens
	C3D_RenderTarget* top = C2D_CreateScreenTarget(GFX_TOP, GFX_LEFT);
	C3D_RenderTarget* bottom = C2D_CreateScreenTarget(GFX_BOTTOM, GFX_LEFT);

	// Load graphics
	spriteSheet = C2D_SpriteSheetLoad("romfs:/gfx/sprites.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);
	backgroundSheet = C2D_SpriteSheetLoad("romfs:/gfx/backgrounds.t3x");
	if (!spriteSheet) svcBreak(USERBREAK_PANIC);

	g_staticBuf  = C2D_TextBufNew(4096);
	gameFont = C2D_FontLoad("romfs:/gfx/roboto.bcfnt");
	C2D_TextFontParse(&g_staticText[0], gameFont, g_staticBuf, "Press A to start");
	C2D_TextOptimize(&g_staticText[0]);
	C2D_TextFontParse(&g_staticText[2], gameFont, g_staticBuf, "YOU LOSE");
	C2D_TextOptimize(&g_staticText[2]);

	//load audio

	loadWav(&sounds[0], "romfs:/audio/hit1.wav", 0);
	loadWav(&sounds[1], "romfs:/audio/hit2.wav", 1);
	loadWav(&sounds[2], "romfs:/audio/hit3.wav", 2);
	musicFile = fopen("romfs:/audio/bgm.wav", "rb");
	if (!musicFile || parse_wav_header(musicFile, &musicDataStart) < 0)
	{
		printf("Failed to open or parse WAV file.\n");
		gfxExit();
		romfsExit();
		return 1;
	}
	fseek(musicFile, musicDataStart, SEEK_SET);
	musicFile2 = fopen("romfs:/audio/bgm2.wav", "rb");
	if (!musicFile2 || parse_wav_header(musicFile2, &musicDataStart2) < 0)
	{
		printf("Failed to open or parse WAV file.\n");
		gfxExit();
		romfsExit();
		return 1;
	}
	fseek(musicFile2, musicDataStart2, SEEK_SET);


	u32* audioBuffer = (u32*)linearAlloc(SAMPLESPERBUF * BYTESPERSAMPLE * 2); //buffers
	memset(waveBuf, 0, sizeof(waveBuf));
	waveBuf[0].data_vaddr = &audioBuffer[0];
	waveBuf[1].data_vaddr = &audioBuffer[SAMPLESPERBUF];
	waveBuf[0].nsamples = SAMPLESPERBUF;
	waveBuf[1].nsamples = SAMPLESPERBUF;
	u32* audioBuffer2 = (u32*)linearAlloc(SAMPLESPERBUF * BYTESPERSAMPLE * 2); //buffers
	memset(waveBuf2, 0, sizeof(waveBuf2));
	waveBuf2[0].data_vaddr = &audioBuffer2[0];
	waveBuf2[1].data_vaddr = &audioBuffer2[SAMPLESPERBUF];
	waveBuf2[0].nsamples = SAMPLESPERBUF;
	waveBuf2[1].nsamples = SAMPLESPERBUF;

	// int numSounds = 3; // or however many you want
	ndspInit();
	// for (int i = 0; i < numSounds; i++) {
    // ndspChnReset(i);
    // ndspChnSetInterp(i, NDSP_INTERP_LINEAR);
    // ndspChnSetRate(i, 22050); // Or whatever
    // ndspChnSetFormat(i, NDSP_FORMAT_STEREO_PCM16);
    // float mix[12] = {1.0f, 1.0f}; // Left/Right
    // ndspChnSetMix(i, mix);
	// }

	WavBufferFill(audioBuffer, SAMPLESPERBUF * BYTESPERSAMPLE * 2,musicFile);//fill i up the buffer
	ndspChnWaveBufAdd(0, &waveBuf[0]);
	ndspChnWaveBufAdd(0, &waveBuf[1]);
	WavBufferFill(audioBuffer2, SAMPLESPERBUF * BYTESPERSAMPLE * 2,musicFile2);//fill i up the buffer
	ndspChnWaveBufAdd(1, &waveBuf2[0]);
	ndspChnWaveBufAdd(1, &waveBuf2[1]);

	bool fillBlock = false;
	bool fillBlock2 = false;

	InitGame();


	// Main loop
	while (aptMainLoop())
	{
		ndspChnSetRate(0, SAMPLERATE);
		tickCount ++;

		hidScanInput();

		// Respond to user input
		u32 kDown = hidKeysDown();
		u32 kHeld = hidKeysHeld();
		if (kDown & KEY_START && kHeld & KEY_SELECT)
		 	break; // break in order to return to hbmenu

		if (waveBuf[fillBlock].status == NDSP_WBUF_DONE) {
			WavBufferFill(waveBuf[fillBlock].data_pcm16,
								waveBuf[fillBlock].nsamples * BYTESPERSAMPLE,musicFile);
			ndspChnWaveBufAdd(0, &waveBuf[fillBlock]);
			fillBlock = !fillBlock;
		}
		if (waveBuf2[fillBlock2].status == NDSP_WBUF_DONE) {
			WavBufferFill(waveBuf2[fillBlock2].data_pcm16,
								waveBuf2[fillBlock2].nsamples * BYTESPERSAMPLE,musicFile2);
			ndspChnWaveBufAdd(1, &waveBuf2[fillBlock2]);
			fillBlock2 = !fillBlock2;
		}
		// for (int i = 0; i < numSounds; i++) {
		// 	if (sounds[i].waveBuf[sounds[i].fillBlock].status == NDSP_WBUF_DONE) {
		// 		WavBufferFill(sounds[i].waveBuf[sounds[i].fillBlock].data_pcm16,
		// 							SAMPLESPERBUF * BYTESPERSAMPLE,sounds[i].file);
		// 		ndspChnWaveBufAdd(i, &sounds[i].waveBuf[sounds[i].fillBlock]);
		// 		sounds[i].fillBlock = !sounds[i].fillBlock;
		// 	}
		// }

		if (gameState == 0){
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
			C2D_SceneBegin(top);
			for (int x = -1;x<11;x++){
				for (int y = -1;y<7;y++){
					C2D_SpriteSetPos(&bgEnemy, x*40+20, y*40+20);
					C2D_SpriteSetRotation(&bgEnemy, sin((float)tickCount/10+(float)x/2+(float)y/4)*0.1);
					C2D_ImageTint enemyTint;
					float fadeTime = 1-(tickCount*0.4-x*10-10)*0.01;
					if (fadeTime<0.5)
						fadeTime = 0.5;
					C2D_PlainImageTint(&enemyTint,C2D_Color32f(0.0,0.0,0.0,1.0),fadeTime);
					C2D_DrawSpriteTinted(&bgEnemy,&enemyTint);
				}
			}
			float fadeTime = 1-(tickCount*0.01-3);
			if (fadeTime<0){fadeTime = 0;}
			C2D_ImageTint titleTint;
			C2D_PlainImageTint(&titleTint,C2D_Color32f(0.0,0.0,0.0,1.0),fadeTime);
			C2D_DrawSpriteTinted(&titleSprite,&titleTint);

			C2D_TargetClear(bottom, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
			C2D_SceneBegin(bottom);
			
			fadeTime = (float)tickCount/500.0;
			if (fadeTime>1){fadeTime = 1;}
			u32 topColor = C2D_Color32f((130.0/255.0)*0.4*fadeTime,(102.0/255.0)*0.4*fadeTime,(152.0/255.0)*0.4*fadeTime,1.0);
			u32 bottomColor = C2D_Color32f((130.0/255.0)*0.2*fadeTime,(102.0/255.0)*0.2*fadeTime,(152.0/255.0)*0.2*fadeTime,1.0);
			C2D_DrawRectangle(0, 0, 0, SCREEN_WIDTH_BOTTOM, SCREEN_WIDTH_BOTTOM, topColor, topColor, bottomColor, bottomColor);
			
			//float sintime = sin((float)tickCount/10);
			//sintime = 1;
			fadeTime = ((float)tickCount/1000-0.4);
			float tickCountMask = tickCount*0.2-100;
			if (tickCountMask > 0){
				tickCountMask = 0;
			}
			if (fadeTime>1){fadeTime = 1;}
			C2D_DrawText(&g_staticText[0],C2D_WithColor|C2D_AlignCenter , (SCREEN_WIDTH_BOTTOM/2), tickCountMask*tickCountMask+100, 1.0f, 1.0f, 1.0f,C2D_Color32f(1,1,1,1));
			C3D_FrameEnd(0);
			if (kDown & KEY_A){
				gameState = 1;
				updateLevelText();
			}
		}
		else if (gameState == 1){
			if (kHeld&KEY_B&&kDown&KEY_A){
				for (int i = 0;i<ENEMIES_COUNT;i++){
					enemies[i].hp = 0;
				}
			}
			if (!paused){
				updateEnemies();
				updatePlayer();
				if (kHeld&KEY_DLEFT||kHeld&KEY_CPAD_LEFT){
					playerPaddle.sx -= playerPaddle.speed;
				}
				if (kHeld&KEY_DRIGHT||kHeld&KEY_CPAD_RIGHT){
					playerPaddle.sx += playerPaddle.speed;
				}
			}
			if (kDown&KEY_START){
				paused = !paused;
			}

			//printf("\x1b[1;1HSprites: %zu/%u\x1b[K", numSprites, MAX_SPRITES);
			printf("\x1b[2;1HCPU:     %6.2f%%\x1b[K", C3D_GetProcessingTime()*6.0f);
			printf("\x1b[3;1HGPU:     %6.2f%%\x1b[K", C3D_GetDrawingTime()*6.0f);
			printf("\x1b[4;1HCmdBuf:  %6.2f%%\x1b[K", C3D_GetCmdBufUsage()*100.0f);
			printf("\x1b[5;1HSX		  %6.2f%%\x1b[K", playerBall.sx);
			printf("\x1b[6;1HSY		  %6.2f%%\x1b[K", playerBall.sy);
			printf("\x1b[7;1HX		  %6.2f%%\x1b[K", playerBall.x);
			printf("\x1b[8;1HY		  %6.2f%%\x1b[K", playerBall.y);
			printf("\x1b[9;1HDEFEATED		  %6.2f%%\x1b[K", (double)defeatedCount);
			//printf("\x1b[16;20HHello World!");


			// Render the scene
			C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
			C2D_TargetClear(top, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
			C2D_SceneBegin(top);
			C2D_DrawSprite(&backgroundSprite);
			for (size_t i = 0; i < ENEMIES_COUNT; i ++){
				if (enemies[i].spawnTime>0){
					C2D_DrawSprite(&enemies[i].spr);
				}
			}
			C2D_ImageTint playerTint;
			if (playerBall.y>SCREEN_HEIGHT_TOP+20){
				C2D_PlainImageTint(&playerTint,C2D_Color32f(1.0,1.0,1.0,0.2),1.0);

			}else{
				C2D_PlainImageTint(&playerTint,C2D_Color32f(0.0,0.0,0.0,1.0),0.0);
			}
			if ((playerBall.lives>=0)){
				C2D_DrawSpriteTinted(&playerBall.spr,&playerTint);
			}
			C2D_DrawSprite(&playerPaddle.spr);
			
			float sintime = sin((float)tickCount/10);
			if (paused&!(playerBall.lives<0)){
				u32 pauseColor = C2D_Color32f(0,0,0,0.5);
				C2D_DrawRectangle(0, 0, 0, SCREEN_WIDTH_TOP, SCREEN_HEIGHT_TOP, pauseColor, pauseColor, pauseColor, pauseColor);
				C2D_DrawText(&g_staticText[1],C2D_WithColor|C2D_AlignCenter , (SCREEN_WIDTH_TOP/2), 60, 1.0f, 2.0f, 2.0f,C2D_Color32f(sintime*0.25+0.75,sintime*0.25+0.75,sintime*0.25+0.75,1.0f));
			}

			float tickCountMask = tickCount*0.2-loseTime;
			if (tickCountMask > 0){
				tickCountMask = 0;
			}
			if (playerBall.lives<0){
				updateEnemies();
				paused = true;
				C2D_DrawText(&g_staticText[2],C2D_WithColor|C2D_AlignCenter , (SCREEN_WIDTH_TOP/2), 80-tickCountMask*tickCountMask, 1.0f, 2.0f, 2.0f,C2D_Color32f(1,1,1,1));
				if (kDown & KEY_START || kDown & KEY_A){
					paused = false;
					InitGame();
					playerBall = Ball(SCREEN_WIDTH_TOP/2,SCREEN_HEIGHT_TOP-10,0,3);
					playerPaddle = Paddle(1);
					gameState = 1;
					updateLevelText();
				}
			}
			
			C2D_TargetClear(bottom, C2D_Color32f(0.0f, 0.0f, 0.0f, 1.0f));
			C2D_SceneBegin(bottom);
			u32 topColor = C2D_Color32f((130.0/255.0)*0.4,(102.0/255.0)*0.4,(152.0/255.0)*0.4,1.0);
			u32 bottomColor = C2D_Color32f((130.0/255.0)*0.2,(102.0/255.0)*0.2,(152.0/255.0)*0.2,1.0);
			C2D_DrawRectangle(0, 0, 0, SCREEN_WIDTH_BOTTOM, SCREEN_WIDTH_BOTTOM, topColor, topColor, bottomColor, bottomColor);

			C2D_DrawText(&g_staticText[0],C2D_WithColor|C2D_AlignCenter , (SCREEN_WIDTH_BOTTOM/2), 10, 1.0f, 1.0f, 1.0f,C2D_Color32f(sintime*0.25+0.75,sintime*0.25+0.75,sintime*0.25+0.75,1.0f));
			C2D_SpriteSetRotation(&lifeSprite, sin(float(tickCount)/10)*0.6);
			for (int i = 0;i<playerBall.lives;i++){
				float g = (float)i-(float)playerBall.lives/2.0+0.5f;
				C2D_SpriteSetPos(&lifeSprite,SCREEN_WIDTH_BOTTOM/2+g*30,70);
				C2D_DrawSprite(&lifeSprite);
			}
			C3D_FrameEnd(0);
		}
	}

	// Delete graphics
	C2D_SpriteSheetFree(spriteSheet);
	C2D_SpriteSheetFree(backgroundSheet);

	// Deinit libs
	C2D_Fini();
	C3D_Fini();
	gfxExit();
	romfsExit();
	ndspExit();
	// for (int i = 0; i < numSounds; i++) {
	// 	if (sounds[i].file) fclose(sounds[i].file);
	// 	if (sounds[i].buffer) linearFree(sounds[i].buffer);
	// }
	if (musicFile) fclose(musicFile);
	return 0;
}
