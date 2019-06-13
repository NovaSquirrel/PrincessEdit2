#ifndef PLATFORMER_HEADER
#define PLATFORMER_HEADER
#define NO_STDIO_REDIRECT
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "cJSON.h"

///////////////////////////////////////////////////////////////////////////////

// shapes for individual "rectangles" to take
enum {
	STYLE_RECTANGLE,
	STYLE_RIGHT_TRIANGLE
};

enum LayerTypes {
  LAYER_RECTANGLE,
  LAYER_SPRITE,
  LAYER_TILEMAP,
  LAYER_CONTROL
};

typedef struct LevelRect {
	char Style;
	short Type, X, Y, W, H;
	char Flips, RectType;
	char *ExtraInfo;
	struct LevelRect *Prev, *Next;
} LevelRect;

typedef struct LevelTile {
  short Graphic;          // -1 if no tile
  LevelRect *Rect;        // or NULL if not Rectangle mode
  signed short Angle;
  char Flips;         // SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL
} LevelTile;

typedef struct TilesetEntry { // convert to some sort of hash table later
  int Id;                     // 32 bytes total
  char Name[28];
} TilesetEntry;

typedef struct LayerInfo {
  SDL_Texture *Texture; // tileset texture
  int TextureW, TextureH;
  int Type;
  char LayerHidden;
  char Name[32];
  char TilesetName[32];
  TilesetEntry *TilesetLookup;
  unsigned int TileWidth, TileHeight, LayerWidth, LayerHeight;
  LevelTile *Map;
  LevelRect *Rects;
  cJSON *JSON;
} LayerInfo;

enum SysCursorId {
  SYSCURSOR_NORMAL,
  SYSCURSOR_WAIT,
  SYSCURSOR_SIZE_NS,
  SYSCURSOR_SIZE_EW,
  SYSCURSOR_SIZE_NWSE,
  SYSCURSOR_SIZE_NESW,
  SYSCURSOR_SIZE_ALL,
  SYSCURSOR_HAND,
  SYSCURSOR_DISABLE,
  SYSCURSOR_MAX
};
extern SDL_Cursor *SystemCursors[SYSCURSOR_MAX];

///////////////////////////////////////////////////////////////////////////////

extern int ScreenWidth, ScreenHeight;
extern SDL_Window *window;
extern SDL_Renderer *ScreenRenderer;
extern int retraces;
extern char *PrefPath;
extern char *BasePath;
extern cJSON *LevelJSON;
extern int LevelW, LevelH, GridW, GridH, TileW, TileH;
extern char LevelFilename[50];
extern char LevelFilenameFull[260];


void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...);
void strlcpy(char *Destination, const char *Source, int MaxLength);
SDL_Surface *SDL_LoadImage(const char *FileName, int Flags);
SDL_Texture *LoadTexture(const char *FileName, int Flags);
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2);
void rect(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2);
void sblit(SDL_Surface* SrcBmp, SDL_Surface* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blit(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blitf(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, SDL_RendererFlip Flip);
void blitz(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, int Width2, int Height2);
void blitfull(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int DestX, int DestY);
const char *GetLevelStr(const char *Default, const char *s,...);
int GetLevelInt(int Default, const char *s,...);
char *ReadTextFile(const char *Filename);
const char *FilenameOnly(const char *Path);
cJSON *cJSON_Load(const char *Filename);
cJSON *cJSON_Search(cJSON *Root, char *SearchString);
int cJSON_Length(cJSON *Array);
int cJSON_IntValue(cJSON *JSON, const char *Var, int Default);
void cJSON_IntValueSet(cJSON *JSON, const char *Var, int Value);
int LoadLevel(const char *Path);
