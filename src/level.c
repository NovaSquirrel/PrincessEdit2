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

#define TEXTURE_SEARCH_PATH_COUNT 20
char TextureSearchPaths[TEXTURE_SEARCH_PATH_COUNT][FILENAME_PATH_LEN] = {{0}};

SDL_Texture *TextureFromName(const char *Name) {
	for(LoadedTileTexture *t = LoadedTileTextures; t; t=t->Next) {
		if(!strcmp(t->Name, Name))
			return t->Texture;
	}

	// Try and find the texture???
	char Path[FILENAME_PATH_LEN];
	SDL_Texture *texture = NULL;
	for(int i=0; i<TEXTURE_SEARCH_PATH_COUNT; i++) {
		if(!TextureSearchPaths[i][0])
			break;
		sprintf(Path, "%s%s.png", TextureSearchPaths[i], Name);
		texture = LoadTexture(Path, LOAD_TEXTURE_NO_ERROR);
		if(texture)
			break;
	}
	if(!texture) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't find texture \"%s\"", Name);
		exit(0);
	}

	LoadedTileTexture *texture_info = (LoadedTileTexture*)malloc(sizeof(LoadedTileTexture));
	strlcpy(texture_info->Name, Name, sizeof(texture_info->Name));
	SDL_QueryTexture(texture, NULL, NULL, &texture_info->TextureW, &texture_info->TextureH);
	texture_info->Texture = texture;
	texture_info->Next = LoadedTileTextures;
	LoadedTileTextures = texture_info;
	return texture;
}

int StartsWith(char *string, const char *starts_with, char **write_to) {
	int length = strlen(starts_with);
	if(!memcmp(string, starts_with, length)) {
		if(write_to) {
			*write_to = string + length;
		}
		return 1;
	}
	return 0;
}

uint32_t ParseCategoryString(TilesetInfo *tileset, char *string) {
	int categories = 0;

	char *s = strtok (string, ",");
	while (s != NULL) {
		// Try to look up the name first
		int found = 0;
		for(int i=0; i<MAX_CATEGORIES; i++) {
			if(!strcmp(tileset->Categories[i], s)) {
				categories |= 1 << i;
				found = 1;
				break;
			}
		}
		// Find a free slot to create it
		if(!found) {
			int i = tileset->CategoryCount++;
			strlcpy(tileset->Categories[i], s, sizeof(tileset->Categories[i]));
			categories |= 1 << i;
		}

		s = strtok(NULL, ",");
	}

	return categories;
}

int ParseNumber(const char *Number) {
	if(Number[0] == '$')
		return strtol(Number+1, NULL, 16);
	return strtol(Number, NULL, 10);
}

uint16_t ParseNova2Tile(const char *Tile) {
	// Syntax: [base:]x[,y][_][h][v]
	while(*Tile == ' ')
		Tile++;
	// TODO: Properly handle constructs like 0:0
	if(!memcmp(Tile, "0:0", 3))
		return 0xffff; // Make it invisible at least

	int flips = 0;
	char *end = strrchr(Tile, 0);
	if(end[-1] == 'v') {
		flips |= 0x8000;
		end--;
	}
	if(end[-1] == 'h') {
		flips |= 0x4000;
		end--;
	}

	// Ignore the base
	const char *number_start = strchr(Tile, ':');
	if(!number_start)
		number_start = Tile;
	else
		number_start++;

	// Read tile position
	int x = ParseNumber(number_start);
	int y = 0;
	const char *comma = strchr(number_start, ',');
	if(comma)
		y = ParseNumber(comma+1);

	y += (x / 16) * 16;
	x = x % 16;
	return flips | x | (y << 8);
}

void ImportNova2Tileset(TilesetInfo *Tileset, int *Count, const char *TilesetName) {
	char Path[FILENAME_PATH_LEN];
	sprintf(Path, "%s%s", LevelDirectory, TilesetName);
	char *Buffer = ReadTextFile(Path);
	if(!Buffer) {
		sprintf(Path, "%sdata/tiles/%s", BasePath, TilesetName);
		Buffer = ReadTextFile(Path);

		if(!Buffer) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s can't load?", Path);
			exit(0);
			return;
		}
	}

	char *Peek = Buffer, *Next; // For parsing
	SDL_Texture *texture = NULL;
	int category = 0;
	int defined_anything = 0;

	uint16_t this_tile_index = 0;

	while(Peek) {
		// Find the next line and terminate the current line's string
		Next = strchr(Peek, '\n');
		if(!Next) break;
		if(Next[-1] == '\r') Next[-1] = 0;
		*Next = 0;

		// TODO
		if(*Peek == '#') { // Comment
			Peek = Next+1;
			continue;
		} else if(*Peek == '+') {
			if(defined_anything)
				(*Count)++;
			strlcpy(Tileset->TilesetLookup[*Count].Name, Peek+1, sizeof(Tileset->TilesetLookup[*Count].Name));
			Tileset->TilesetLookup[*Count].Categories = category;
			Tileset->TilesetLookup[*Count].Texture = texture;
			defined_anything = 1;
			this_tile_index = 0;
		} else if(Peek[0] == 't' && Peek[1] == ' ') {
			// TODO: protect against buffer overflow?
			Tileset->TilesetLookup[*Count].Style = STYLE_QUAD;
			char *space = strchr(Peek+2, ' ');
			if(space)
				*space = 0;
			Tileset->TilesetLookup[*Count].Quad.Tiles[this_tile_index++] = ParseNova2Tile(Peek+2);
			if(space) {
				Tileset->TilesetLookup[*Count].Quad.Tiles[this_tile_index++] = ParseNova2Tile(space+1);
			}
		} else if(Peek[0] == 'w' && Peek[1] == ' ') {
			uint16_t tile = ParseNova2Tile(Peek+2);
			Tileset->TilesetLookup[*Count].Style = STYLE_QUAD;
			Tileset->TilesetLookup[*Count].Quad.Tiles[0] = tile + 0;
			Tileset->TilesetLookup[*Count].Quad.Tiles[1] = tile + 1;
			Tileset->TilesetLookup[*Count].Quad.Tiles[2] = tile + 2;
			Tileset->TilesetLookup[*Count].Quad.Tiles[3] = tile + 3;
		} else if(Peek[0] == 'q' && Peek[1] == ' ') {
			Tileset->TilesetLookup[*Count].Style = STYLE_SINGLE;
			Tileset->TilesetLookup[*Count].Single.Tile = ParseNova2Tile(Peek+2);
		} else if(StartsWith(Peek, "tileset ", &Peek)) {
			texture = TextureFromName(Peek);
		} else if(StartsWith(Peek, "editor_category ", &Peek)) {
			category = ParseCategoryString(Tileset, Peek);
		} else if(StartsWith(Peek, "editor_uncategorized", &Peek)) {
			category = 0;
		} else if(StartsWith(Peek, "editor_hide", &Peek)) {
			(*Count)--;
		}

		// Next line
		Peek = Next+1;
	}
	if(defined_anything)
		(*Count)++;

	free(Buffer);
}

TilesetInfo *LoadTileset(const char *TilesetName) {
	char Path[FILENAME_PATH_LEN];
	sprintf(Path, "%s%s.txt", LevelDirectory, TilesetName);
	char *Buffer = ReadTextFile(Path);

	if(!Buffer) {
		sprintf(Path, "%sdata/tiles/%s.txt", BasePath, TilesetName);
		Buffer = ReadTextFile(Path);

		if(!Buffer) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s can't load?", Path);
			exit(0);
			return NULL;
		}
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

	// Parsing state
	int category = 0;
	SDL_Texture *current_texture = base_texture;

	Peek = Buffer;
	while(Peek) {
		// Find the next line and terminate the current line's string
		Next = strchr(Peek, '\n');
		if(!Next) break;
		if(Next[-1] == '\r') Next[-1] = 0;
		*Next = 0;

		if(strlen(Peek) <= 1 || *Peek == '#') { // Comment
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "category ", &Peek)) { // Mark upcoming tiles under a list of categories
			category = ParseCategoryString(tileset, Peek);
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "uncategorized", NULL)) {
			category = 0;
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "texture ", &Peek)) {
			current_texture = TextureFromName(Peek);
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "default_texture", NULL)) {
			current_texture = base_texture;
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "look_like ", &Peek)) {
			char *equals = strchr(Peek, '=');
			if(equals) {
				*equals = 0;				
				int tile1 = -1, tile2 = -1;
				for(int i=0; i<Count; i++) {
					if(!strcmp(tileset->TilesetLookup[i].Name, Peek)) {
						tile1 = i;
						break;
					}
				}
				for(int i=0; i<Count; i++) {
					if(!strcmp(tileset->TilesetLookup[i].Name, equals+1)) {
						tile2 = i;
						break;
					}
				}
				if(tile1 != -1 && tile2 != -1) {
					tileset->TilesetLookup[tile1].Style = tileset->TilesetLookup[tile2].Style;
					tileset->TilesetLookup[tile1].Texture = tileset->TilesetLookup[tile2].Texture;
					if(tileset->TilesetLookup[tile1].Style == STYLE_SINGLE) {
						tileset->TilesetLookup[tile1].Single = tileset->TilesetLookup[tile2].Single;
					} else if(tileset->TilesetLookup[tile1].Style == STYLE_QUAD) {
						tileset->TilesetLookup[tile1].Quad = tileset->TilesetLookup[tile2].Quad;

					}
				} else {
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can't find %s and/or %s for a look_like command", Peek, equals+1);
					exit(0);
					return NULL;
				}
			}
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "import_nova2 ", &Peek)) {
			ImportNova2Tileset(tileset, &Count, Peek);
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "texture_path ", &Peek)) {
			for(int i=0; i<TEXTURE_SEARCH_PATH_COUNT; i++) {
				if(!TextureSearchPaths[i][0]) {
					sprintf(TextureSearchPaths[i], "%s%s", LevelDirectory, Peek);
					// Make sure it ends in a slash
					char *Last = strrchr(TextureSearchPaths[i], 0);
					if(Last[-1] != '/' && Last[-1] != '\\') {
						Last[0] = '/';
						Last[1] = 0;
					}
					break;
				}
			}
			Peek = Next+1;
			continue;
		} else if(StartsWith(Peek, "category_icon ", &Peek)) {
			char *equals = strchr(Peek, '=');
			if(equals) {
				*equals = 0;				
				int cat = -1, tile = -1;
				for(int i=0; i<tileset->CategoryCount; i++) {
					if(!strcmp(tileset->Categories[i], Peek)) {
						cat = i;
						break;
					}
				}
				for(int i=0; i<Count; i++) {
					if(!strcmp(tileset->TilesetLookup[i].Name, equals+1)) {
						tile = i;
						break;
					}
				}
				if(cat != -1 && tile != -1) {
					tileset->CategoryIcons[cat] = tile;
				} else {
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Can't find %s and/or %s for a category_icon command", Peek, equals+1);
					exit(0);
					return NULL;
				}
			}
			Peek = Next+1;
			continue;
		}

		// Assume this is a tile, and write it the array
		char *after = Peek;
		int SheetPosition = strtol(Peek, &after, 16);
		if(after == Peek) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't understand tileset line: %s", Peek);
			exit(0);
			return NULL;
		}
		Peek = strchr(Peek, ' ') + 1;

		// If the name is already defined, overwrite the old one's appearance
		int insert_at = Count;
		int added_one = 1;
		for(int i=0; i<Count; i++) {
			if(!strcmp(tileset->TilesetLookup[i].Name, Peek)) {
				insert_at = i;
				added_one = 0;
				break;
			}
		}
		if(added_one)
			strlcpy(tileset->TilesetLookup[insert_at].Name, Peek, sizeof(tileset->TilesetLookup[insert_at].Name));
		tileset->TilesetLookup[insert_at].Categories |= category;
		tileset->TilesetLookup[insert_at].Texture = current_texture;
		tileset->TilesetLookup[insert_at].Style = STYLE_SINGLE;
		tileset->TilesetLookup[insert_at].Single.Tile = SheetPosition * 2; // Shift both X and Y by 2

		// detect extra parameters
		Peek = strchr(tileset->TilesetLookup[insert_at].Name, ' ');
		if(Peek) {
			*Peek = 0;
			Peek++;
			// Skip to the metadata
			while(*Peek == ' ')
				Peek++;
			// Triangle
			if(*Peek == 'T') {
				tileset->TilesetLookup[insert_at].Style = STYLE_RIGHT_TRIANGLE;
				int Color = strtol(Peek+1, &Peek, 16);
				tileset->TilesetLookup[insert_at].Shape.R = (Color>>16) & 255;
				tileset->TilesetLookup[insert_at].Shape.G = (Color>>8)  & 255;
				tileset->TilesetLookup[insert_at].Shape.B = (Color>>0)  & 255;

				// Skip to the additional flags, in this case what width/height ratios are allowed
				while(*Peek == ' ')
					Peek++;
				while(*Peek && *Peek != ' ') {
					if(isdigit(*Peek))
						tileset->TilesetLookup[insert_at].Shape.Var |= 1 << (*Peek - '1');
					Peek++;
				}
			}				
		}
		if(added_one)
			Count++;

		// Next line
		Peek = Next+1;
	}

	tileset->TileCount = Count;
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
