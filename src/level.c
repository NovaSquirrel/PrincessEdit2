/*
 * PrincessEdit 2
 *
 * Copyright (C) 2019 NovaSquirrel
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.	If not, see <http://www.gnu.org/licenses/>.
 */
#include "editor.h"

cJSON *LevelJSON = NULL;
int LevelW, LevelH, GridW, GridH, FormatVersion;
int TileW, TileH;
char LevelFilename[50];
char LevelFilenameFull[FILENAME_PATH_LEN];
LayerInfo *LayerInfos = NULL;
LoadedTileTexture *LoadedTileTextures = NULL;
TilesetInfo *TilesetInfos = NULL;
int NumLayers;


#define UNDO_STEPS 10
int UndoIndex = 0;
int UndoLayer[UNDO_STEPS];
LevelRect *UndoRect[UNDO_STEPS];
int RedoLayer;
LevelRect *RedoRect = NULL;

void FreeRectList(LevelRect *R) {
	while(R) {
		if(R->ExtraInfo)
			free(R->ExtraInfo);
		LevelRect *Next = R->Next;
		free(R);
		R = Next;
	}
}

LevelRect *CloneRectList(LevelRect *R) {
	LevelRect *Head = NULL, *Tail = NULL;

	while(R) {
		LevelRect *Copy = (LevelRect*)malloc(sizeof(LevelRect));
		*Copy = *R;

		// Backward link
		Copy->Prev = Tail;
		// Forward link
		if(Tail)
			Tail->Next = Copy;

		// Duplicate the extra information
		if(Copy->ExtraInfo)
			Copy->ExtraInfo = strdup(Copy->ExtraInfo);

		// Establish the new head and tail
		if(!Head) {
			Head = Copy;
		}

		Tail = Copy;
		R = R->Next;
	}

	return Head;
}

void UndoStep(int Layer) {
	// Invalidate redo step
	if(RedoRect) {
		FreeRectList(RedoRect);
		RedoRect = NULL;
	}

	// Move everything back
	if(UndoIndex == UNDO_STEPS) {
		if(UndoRect[0])
			FreeRectList(UndoRect[0]);
		for(int i=0; i<UNDO_STEPS-1; i++) {
			UndoRect[i] = UndoRect[i+1];
			UndoLayer[i] = UndoLayer[i+1];
		}
		UndoRect[UNDO_STEPS-1] = NULL;
		UndoIndex--;
	}

	// Invalidate everything past the new undo point
	for(int i=UndoIndex; i<UNDO_STEPS; i++) {
		if(UndoRect[i]) {
			FreeRectList(UndoRect[i]);
			UndoRect[i] = NULL;
		}
	}

	// Insert the undo information
	UndoLayer[UndoIndex] = Layer;
	UndoRect[UndoIndex] = CloneRectList(LayerInfos[Layer].Rects);
	UndoIndex++;
}

void Undo() {
	if(!UndoIndex)
		return;
	UndoIndex--;

	if(RedoRect) {
		FreeRectList(RedoRect);
		RedoRect = NULL;
	}
	RedoRect = CloneRectList(LayerInfos[UndoLayer[UndoIndex]].Rects);
	RedoLayer = UndoLayer[UndoIndex];

	// Get rid of old list before replacing it
	FreeRectList(LayerInfos[UndoLayer[UndoIndex]].Rects);
	LayerInfos[UndoLayer[UndoIndex]].Rects = CloneRectList(UndoRect[UndoIndex]);
}

void Redo() {
	if(!RedoRect)
		return;
	LevelRect *R = RedoRect;
	RedoRect = NULL;
	UndoStep(RedoLayer);
	LayerInfos[RedoLayer].Rects = R;
	RedoRect = NULL;
}

/*
int TilesetLookupIdToIndex(int Layer, int Id) {
	for(int i=0; LayerInfos[Layer].Tileset->TilesetLookup[i].Id >= 0; i++)
		if(LayerInfos[Layer].Tileset->TilesetLookup[i].Id == Id)
			return i;
	return -1;
}
*/

int TilesetLookupStringToIndex(int Layer, const char *String) {
	for(int i=0; LayerInfos[Layer].Tileset->TilesetLookup[i].Style != STYLE_END; i++) {
		if(!strcmp(LayerInfos[Layer].Tileset->TilesetLookup[i].Name, String))
			return i;
	}
	return -1;
}

// Go through the list of rectangles for a layer and convert it into a 2D array of tiles
void RenderLevelRects(int Layer) {
	memset(LayerInfos[Layer].Map, 0, sizeof(LevelTile)*LayerInfos[Layer].LayerWidth*LayerInfos[Layer].LayerHeight);
	LevelRect *Rect = LayerInfos[Layer].Rects;
	while(Rect) {
		// Get the TilesetEntry that corresponds to this rectangle's type
		struct TilesetEntry *DrawInfo = &LayerInfos[Layer].Tileset->TilesetLookup[Rect->Type];

		int Tiles[4] = {0, 0, 0, 0};
		switch(DrawInfo->Style) {
			case STYLE_SINGLE:
				Tiles[0] = DrawInfo->Single.Tile + 0x0000;
				Tiles[1] = DrawInfo->Single.Tile + 0x0001;
				Tiles[2] = DrawInfo->Single.Tile + 0x0100;
				Tiles[3] = DrawInfo->Single.Tile + 0x0101;
				break;
			case STYLE_QUAD:
				Tiles[0] = DrawInfo->Quad.Tiles[0];
				Tiles[1] = DrawInfo->Quad.Tiles[1];
				Tiles[2] = DrawInfo->Quad.Tiles[2];
				Tiles[3] = DrawInfo->Quad.Tiles[3];
				break;
			case STYLE_RIGHT_TRIANGLE:
				break;
		}
		if(Rect->Flips & SDL_FLIP_HORIZONTAL) {
			int temp = Tiles[0];
			Tiles[0] = Tiles[1] ^ 0x4000;
			Tiles[1] = temp ^ 0x4000;

			temp = Tiles[2];
			Tiles[2] = Tiles[3] ^ 0x4000;
			Tiles[3] = temp ^ 0x4000;
		}
		if(Rect->Flips & SDL_FLIP_VERTICAL) {
			int temp = Tiles[0];
			Tiles[0] = Tiles[2] ^ 0x8000;
			Tiles[2] = temp ^ 0x8000;

			temp = Tiles[1];
			Tiles[1] = Tiles[3] ^ 0x8000;
			Tiles[3] = temp ^ 0x8000;
		}

		DrawInfo = &LayerInfos[Layer].Tileset->TilesetLookup[Rect->Type];
		for(int x=0; x<Rect->W; x++)
			for(int y=0; y<Rect->H; y++) {
				int RealX = Rect->X + x;
				int RealY = Rect->Y + y;
				if(RealX >= 0 && RealX < LayerInfos[Layer].LayerWidth && RealY >= 0 && RealY < LayerInfos[Layer].LayerHeight) {
					int Index = RealY*LayerInfos[Layer].LayerWidth+RealX;
					LayerInfos[Layer].Map[Index].Flips    = Rect->Flips;
					LayerInfos[Layer].Map[Index].Rect     = Rect;
					LayerInfos[Layer].Map[Index].Texture  = DrawInfo->Texture;
					LayerInfos[Layer].Map[Index].Tiles[0] = Tiles[0];
					LayerInfos[Layer].Map[Index].Tiles[1] = Tiles[1];
					LayerInfos[Layer].Map[Index].Tiles[2] = Tiles[2];
					LayerInfos[Layer].Map[Index].Tiles[3] = Tiles[3];
					LayerInfos[Layer].Map[Index].DrawInfo = DrawInfo;
				}
			}
		Rect = Rect->Next;
	}
}

LevelRect *JSONtoLevelRect(cJSON *JSON, int Layer) {
	LevelRect *Out = (LevelRect*)calloc(1, sizeof(LevelRect));
	if(!Out)
		return NULL;
	Out->W = Out->H = 1;
	cJSON *Try;
	if((Try = cJSON_GetObjectItem(JSON, "Extra"))) {
		Out->ExtraInfo = strdup(Try->valuestring);
	}
	if((Try = cJSON_GetObjectItem(JSON, "Id"))) {
		Out->Type = TilesetLookupStringToIndex(Layer, Try->valuestring);
		if(Out->Type == -1)
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Unknown block type name: %s", Try->valuestring);
	}
	if((Try = cJSON_GetObjectItem(JSON, "X")))
		Out->X = Try->valueint;
	if((Try = cJSON_GetObjectItem(JSON, "Y")))
		Out->Y = Try->valueint;
	if((Try = cJSON_GetObjectItem(JSON, "W")))
		Out->W = Try->valueint;
	if((Try = cJSON_GetObjectItem(JSON, "H")))
		Out->H = Try->valueint;
	if((Try = cJSON_GetObjectItem(JSON, "XFlip")))
		if(Try->type == cJSON_True)
			Out->Flips |= SDL_FLIP_HORIZONTAL;
	if((Try = cJSON_GetObjectItem(JSON, "YFlip")))
		if(Try->type == cJSON_True)
			Out->Flips |= SDL_FLIP_VERTICAL;
	return Out;
}

void FreeLayers() {
	if(!LayerInfos) return;
	for(int i=0;i<NumLayers;i++) {
		free(LayerInfos[i].Map);
		LevelRect *r = LayerInfos[i].Rects;
		while(r) {
			LevelRect *next = r->Next;
			if(r->ExtraInfo)
				free(r->ExtraInfo);
			free(r);
			r = next;
		}
	}
	free(LayerInfos);
}

/*
void LoadTilesetTextures() {
	for(LoadedTileTexture *t = LoadedTileTextures; t; t=t->Next) {
		// Try different directories??
		char Path[260]; // make this safer later
		sprintf(Path, "%sdata/tiles/%s.png", BasePath, LayerInfos[i].TilesetName);
		LayerInfos[i].Texture = LoadTexture(Path, 0);

		SDL_QueryTexture(t->Texture, NULL, NULL, &t->TextureW, &t->TextureH);
	}
}
*/

SDL_Texture *TextureFromName(const char *Name) {
	for(LoadedTileTexture *t = LoadedTileTextures; t; t=t->Next) {
		if(!strcmp(t->Name, Name))
			return t->Texture;
	}

	// Try and find the texture???
	char Path[FILENAME_PATH_LEN];
	sprintf(Path, "%sdata/tiles/%s.png", BasePath, Name);
	SDL_Texture *texture = LoadTexture(Path, 0); //LOAD_TEXTURE_NO_ERROR);
	if(!texture)
		return NULL;

	LoadedTileTexture *texture_info = (LoadedTileTexture*)malloc(sizeof(LoadedTileTexture));
	strlcpy(texture_info->Name, Name, sizeof(texture_info->Name));
	SDL_QueryTexture(texture, NULL, NULL, &texture_info->TextureW, &texture_info->TextureH);
	texture_info->Texture = texture;
	texture_info->Next = LoadedTileTextures;
	LoadedTileTextures = texture_info;
	return texture;
}

#define MAX_TILESET_SIZE 2048
TilesetInfo *LoadTileset(const char *TilesetName) {
	char Path[FILENAME_PATH_LEN];
	sprintf(Path, "%sdata/tiles/%s.txt", BasePath, TilesetName);
	char *Buffer = ReadTextFile(Path);

	if(!Buffer) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s can't load?", Path);
		exit(0);
		return NULL;
	}

	// Set up the tileset
	TilesetInfo *tileset = (TilesetInfo*)calloc(1, sizeof(TilesetInfo));
	strlcpy(tileset->Name, TilesetName, sizeof(tileset->Name));
	tileset->TileWidth = TileW;
	tileset->TileHeight = TileH;
	tileset->TilesetLookup = (TilesetEntry*)calloc(MAX_TILESET_SIZE, sizeof(TilesetEntry));
	SDL_Texture *base_texture = TextureFromName(TilesetName);

	int Count = 0;
	char *Peek, *Next; // For parsing

	Peek = Buffer;
	for(int j=0;Peek;j++,Count++) {
		// Find the next line and terminate the line's string
		Next = strchr(Peek, '\n');
		if(!Next) break;
		if(Next[-1] == '\r') Next[-1] = 0;
		*Next = 0;

		// write to the array
		int SheetPosition = strtol(Peek, NULL, 16);
		Peek = strchr(Peek, ' ') + 1;
		strlcpy(tileset->TilesetLookup[j].Name, Peek, sizeof(tileset->TilesetLookup[j].Name));
		tileset->TilesetLookup[j].Texture = base_texture;
		tileset->TilesetLookup[j].Style = STYLE_SINGLE;
		tileset->TilesetLookup[j].Single.Tile = SheetPosition * 2; // Shift both X and Y by 2

		// detect extra parameters
		Peek = strchr(tileset->TilesetLookup[j].Name, ' ');
		if(Peek) {
			*Peek = 0;
			Peek++;
			// Skip to the metadata
			while(*Peek == ' ')
				Peek++;
			// Triangle
			if(*Peek == 'T') {
				tileset->TilesetLookup[j].Style = STYLE_RIGHT_TRIANGLE;
				int Color = strtol(Peek+1, &Peek, 16);
				tileset->TilesetLookup[j].Shape.R = (Color>>16) & 255;
				tileset->TilesetLookup[j].Shape.G = (Color>>8)  & 255;
				tileset->TilesetLookup[j].Shape.B = (Color>>0)  & 255;

				// Skip to the additional flags, in this case what width/height ratios are allowed
				while(*Peek == ' ')
					Peek++;
				while(*Peek && *Peek != ' ') {
					if(isdigit(*Peek))
						tileset->TilesetLookup[j].Shape.Var |= 1 << (*Peek - '1');
					Peek++;
				}
			}				
		}

		// Next line
		Peek = Next+1;
	}

	tileset->TilesetLookup[Count].Style = STYLE_END;
	tileset->TilesetLookup = realloc(tileset->TilesetLookup, sizeof(TilesetEntry)*(Count+1));
	free(Buffer);
	return tileset;
}

TilesetInfo* TilesetInfoFromName(const char *Name) {
	for(TilesetInfo *t = TilesetInfos; t; t=t->Next) {
		if(!strcmp(t->Name, Name))
			return t;
	}
	TilesetInfo *tileset = LoadTileset(Name);
	if(tileset) {
		tileset->Next = TilesetInfos;
		TilesetInfos = tileset;
	}
	return tileset;
}

int UpdateJSONFromLevel() {
	cJSON *Layers = cJSON_GetObjectItem(LevelJSON, "Layers");
	if(!Layers)
		return 0;
	int LayerNum = 0;
	for(Layers=Layers->child; Layers; Layers=Layers->next, LayerNum++) {
		cJSON *Data = cJSON_GetObjectItem(Layers, "Data");
		if(!Data)
			continue;
		cJSON_Delete(Data->child);

		// make a JSON array from the rectangles
		cJSON *Prev = NULL;
		for(LevelRect *Rect = LayerInfos[LayerNum].Rects; Rect; Rect=Rect->Next) {
			cJSON *New = cJSON_CreateObject();
			cJSON_AddItemToObject(New, "Id", cJSON_CreateString(LayerInfos[LayerNum].Tileset->TilesetLookup[Rect->Type].Name));
			cJSON_AddItemToObject(New, "X", cJSON_CreateNumber(Rect->X));
			cJSON_AddItemToObject(New, "Y", cJSON_CreateNumber(Rect->Y));
			cJSON_AddItemToObject(New, "W", cJSON_CreateNumber(Rect->W));
			cJSON_AddItemToObject(New, "H", cJSON_CreateNumber(Rect->H));
			if((Rect->Flips&SDL_FLIP_HORIZONTAL)!=0)
				cJSON_AddItemToObject(New, "XFlip", cJSON_CreateTrue());
			if((Rect->Flips&SDL_FLIP_VERTICAL)!=0)
				cJSON_AddItemToObject(New, "YFlip", cJSON_CreateTrue());
			if(Rect->ExtraInfo)
				cJSON_AddItemToObject(New, "Extra", cJSON_CreateString(Rect->ExtraInfo));
			if(!Prev) {
				Data->child = New;
			} else {
				New->prev = Prev;
				Prev->next = New;
			}
			Prev = New;
		}
	}
	return 1;
}

int UpdateLevelFromJSON() {
	LevelW = GetLevelInt(256, "Meta/Width");
	LevelH = GetLevelInt(16, "Meta/Height");
	GridW = GetLevelInt(0, "Meta/GridWidth");
	GridH = GetLevelInt(0, "Meta/GridHeight");
	TileW = GetLevelInt(16, "Meta/TileWidth");
	TileH = GetLevelInt(16, "Meta/TileHeight");
	FormatVersion = GetLevelInt(-1, "Meta/FormatVersion");

	// update layers
	cJSON *Layers = cJSON_GetObjectItem(LevelJSON, "Layers");
	if(!Layers) return 0;
	NumLayers = cJSON_Length(Layers->child);
	FreeLayers();
	LayerInfos = (LayerInfo*)calloc(NumLayers, sizeof(LayerInfo));
	Layers = Layers->child;
	int i = 0;
	while(Layers) {
		strlcpy(LayerInfos[i].Name, cJSON_GetObjectItem(Layers, "Name")->valuestring, sizeof(LayerInfos[i].Name));
		const char *Type = cJSON_GetObjectItem(Layers, "Type")->valuestring;
		if(!strcasecmp(Type, "Rectangle"))
			LayerInfos[i].Type = LAYER_RECTANGLE;
		if(!strcasecmp(Type, "Sprite"))
			LayerInfos[i].Type = LAYER_SPRITE;
		if(!strcasecmp(Type, "Tilemap"))
			LayerInfos[i].Type = LAYER_TILEMAP;
		if(!strcasecmp(Type, "Control"))
			LayerInfos[i].Type = LAYER_CONTROL;

		LayerInfos[i].Map = calloc(LevelW*LevelH, sizeof(LevelTile));
		LayerInfos[i].JSON = Layers;
		LayerInfos[i].LayerWidth = LevelW;
		LayerInfos[i].LayerHeight = LevelH;
		LayerInfos[i].Tileset = TilesetInfoFromName(cJSON_GetObjectItem(Layers, "Tileset")->valuestring);

		// Parse all of the rectangles in the JSON and allocate a list
		cJSON *JSONRect = cJSON_GetObjectItem(Layers, "Data");
		LevelRect *PrevRect = NULL;
		if(JSONRect && JSONRect->child) {
			for(JSONRect = JSONRect->child; JSONRect; JSONRect = JSONRect->next) {
				LevelRect *CurRect = JSONtoLevelRect(JSONRect, i);
				if(PrevRect)
					PrevRect->Next = CurRect;
				else
					LayerInfos[i].Rects = CurRect;
				CurRect->Prev = PrevRect;
				PrevRect = CurRect;
			}
		}
		RenderLevelRects(i);

		Layers = Layers->next;
		i++;
	}
	return 1;
}

int LoadLevel(const char *Path) {
	memset(UndoRect, 0, sizeof(UndoRect));
	strlcpy(LevelFilename, FilenameOnly(Path), sizeof(LevelFilename));
	strlcpy(LevelFilenameFull, Path, sizeof(LevelFilenameFull));
	LevelJSON = cJSON_Load(Path);
	if(!LevelJSON) return 0;
	if(!UpdateLevelFromJSON()) return 0;
	return 1;
}

void SaveLevel() {
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Attempting to save %s", LevelFilename);
	if(!UpdateJSONFromLevel())
		return;
	char *Rendered=cJSON_Print(LevelJSON);
	FILE *Out = fopen(LevelFilenameFull, "wb");
	if(!Out) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Could not open \"%s\" for writing", LevelFilenameFull);
		return;
	}
	fprintf(Out, "%s", Rendered);
	fclose(Out);
	free(Rendered);
}

LevelRect *LevelEndRect(int layer) {
	LevelRect *End = LayerInfos[layer].Rects;
	if(End != NULL)
		while(End->Next)
			End = End->Next;
	return End;
}
