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

enum LayerTypes {
  LAYER_RECTANGLE,
  LAYER_SPRITE,
  LAYER_TILEMAP,
  LAYER_CONTROL
};

// A rectangle belonging to a layer
typedef struct LevelRect {
	short Type, X, Y, W, H;
	char Flips;	// Any flips
	char Selected; // Currently selected?
	char *ExtraInfo;
	struct LevelRect *Prev, *Next;
} LevelRect;

// A level layer's rectangle list gets rendered to a 2D array of this structure
// for faster drawing and other things.
typedef struct LevelTile {
	short Graphic;      // -1 if no tile
	LevelRect *Rect;    // So you can click on a tile and get a rectangle
	char Flips;         // SDL_FLIP_NONE, SDL_FLIP_HORIZONTAL, SDL_FLIP_VERTICAL
	struct TilesetEntry *DrawInfo;
} LevelTile;

// shapes for tile types to take
enum {
	STYLE_RECTANGLE,
	STYLE_RIGHT_TRIANGLE
};

// For matching tileset names to coordinates on the tilesheet
typedef struct TilesetEntry { // convert to some sort of hash table later
	int Id;
	char Name[28];
	uint8_t Style, R, G, B, Var; // For alternative shapes
} TilesetEntry;

// Info about each layer of the level
typedef struct LayerInfo {
	SDL_Texture *Texture; // tileset texture
	int TextureW, TextureH;
	int Type;
	char LayerHidden;
	char Name[32];
	char TilesetName[32];
	TilesetEntry *TilesetLookup;
	unsigned int TileWidth, TileHeight, LayerWidth, LayerHeight;
	LevelTile *Map;   // Rendered from Rects
	LevelRect *Rects; // The actual level data
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

typedef struct FontSet {
  TTF_Font *Font[4];
  int Width, Height;
} FontSet;

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
extern LayerInfo *LayerInfos;
extern int NumLayers;

extern SDL_Color FGColor;
extern SDL_Color BGColor;
extern SDL_Color SelectColor;
extern SDL_Color AvailableColor;
extern SDL_Color GridColor;

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
void SaveLevel();
void LoadTilesets();
void RenderSimpleText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, int Flags, const char *Text);
void RenderFormatText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, int Flags, const char *Text, ...);
int Load_FontSet(FontSet *Fonts, int Size, const char *Font1, const char *Font2, const char *Font3, const char *Font4);
void Free_FontSet(FontSet *Fonts);
void RenderLevelRects(int Layer);
int IsInsideRect(int X1, int Y1, int X2, int Y2, int W, int H);
int TilesetLookupIdToIndex(int Layer, int Id);
LevelRect *LevelEndRect(int layer);
char *InputLine(const char *Prompt, char *Buffer, int BufferSize);

void Undo();
void Redo();
void UndoStep(int Layer);
