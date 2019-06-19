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
char LevelFilenameFull[260];
LayerInfo *LayerInfos = NULL;
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

int TilesetLookupIdToIndex(int Layer, int Id) {
	for(int i=0; LayerInfos[Layer].TilesetLookup[i].Id >= 0; i++)
		if(LayerInfos[Layer].TilesetLookup[i].Id == Id)
			return i;
	return -1;
}

int TilesetLookupStringToIndex(int Layer, const char *String) {
	for(int i=0; LayerInfos[Layer].TilesetLookup[i].Id >= 0; i++)
		if(!strcmp(LayerInfos[Layer].TilesetLookup[i].Name, String))
			return i;
	return -1;
}

// Go through the list of rectangles for a layer and convert it into a 2D array of tiles
void RenderLevelRects(int Layer) {
	memset(LayerInfos[Layer].Map, 0, sizeof(LevelTile)*LayerInfos[Layer].LayerWidth*LayerInfos[Layer].LayerHeight);
	LevelRect *Rect = LayerInfos[Layer].Rects;
	while(Rect) {
		int Graphic = LayerInfos[Layer].TilesetLookup[Rect->Type].Id;
		for(int x=0; x<Rect->W; x++)
			for(int y=0; y<Rect->H; y++) {
				int RealX = Rect->X + x;
				int RealY = Rect->Y + y;
				if(RealX >= 0 && RealX < LayerInfos[Layer].LayerWidth && RealY >= 0 && RealY < LayerInfos[Layer].LayerHeight) {
					 int Index = RealY*LayerInfos[Layer].LayerWidth+RealX;
					 LayerInfos[Layer].Map[Index].Graphic = Graphic;
					 LayerInfos[Layer].Map[Index].Flips = Rect->Flips;
					 LayerInfos[Layer].Map[Index].Rect = Rect;
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
		SDL_DestroyTexture(LayerInfos[i].Texture);
		free(LayerInfos[i].Map);
		free(LayerInfos[i].TilesetLookup);
	}
	free(LayerInfos);
}

void LoadTilesets() {
	for(int i=0;i<NumLayers;i++) {
		char Path[260]; // make this safer later
		sprintf(Path, "%sdata/tiles/%s.png", BasePath, LayerInfos[i].TilesetName);
		LayerInfos[i].Texture = LoadTexture(Path, 0);
		SDL_QueryTexture(LayerInfos[i].Texture, NULL, NULL, &LayerInfos[i].TextureW, &LayerInfos[i].TextureH);
	}
}

void LoadTilesetInitial(int i) {
	char Path[260];
	sprintf(Path, "%sdata/tiles/%s.txt", BasePath, LayerInfos[i].TilesetName);
	char *Buffer = ReadTextFile(Path);
	if(Buffer) {
		int Count = 0;
		char *Peek, *Next;
		for(Peek = Buffer; *Peek; Peek++)
			if(*Peek == '\n')
				Count++;
		 LayerInfos[i].TilesetLookup = (TilesetEntry*)malloc(sizeof(TilesetEntry)*(Count+1));
		 Peek = Buffer;
		for(int j=0;Peek;j++) {
			 // find the next
			 Next = strchr(Peek, '\n');
			 if(!Next) break;
			 if(Next[-1] == '\r') Next[-1] = 0;
			 *Next = 0;

			 // write to the array
			 LayerInfos[i].TilesetLookup[j].Id = strtol(Peek, NULL, 16);
			 Peek = strchr(Peek, ' ') + 1;
			 strlcpy(LayerInfos[i].TilesetLookup[j].Name, Peek, sizeof(LayerInfos[i].TilesetLookup[j].Name));

			 Peek = Next+1;
		}
		LayerInfos[i].TilesetLookup[Count].Id = -1;
		free(Buffer);
	} else {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s can't load?", Path);
		exit(0);
	}
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
			cJSON_AddItemToObject(New, "Id", cJSON_CreateString(LayerInfos[LayerNum].TilesetLookup[Rect->Type].Name));
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
		const char *Tileset = cJSON_GetObjectItem(Layers, "Tileset")->valuestring;
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
		LayerInfos[i].TileWidth = TileW;
		LayerInfos[i].TileHeight = TileH;
		LayerInfos[i].LayerWidth = LevelW;
		LayerInfos[i].LayerHeight = LevelH;
		strlcpy(LayerInfos[i].TilesetName, Tileset, sizeof(LayerInfos[i].TilesetName));
		LoadTilesetInitial(i);

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
