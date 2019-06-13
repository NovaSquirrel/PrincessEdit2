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

int IsInsideRect(int X1, int Y1, int X2, int Y2, int W, int H) {
	if(X1<X2) return 0;
	if(Y1<Y2) return 0;
	if(X1>=(X2+W)) return 0;
	if(Y1>=(Y2+H)) return 0;
	return 1;
}

char *ReadTextFile(const char *Filename) {
	FILE *File = fopen(Filename, "rb");
	if(!File) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s not found", Filename);
		return NULL;
	}
	fseek(File, 0, SEEK_END);
	unsigned long FileSize = ftell(File);
	rewind(File);
	char *Buffer = (char*)malloc(sizeof(char)*(FileSize+1));
	if(!Buffer || (FileSize != fread(Buffer,1,FileSize,File))) {
		fclose(File);
		return NULL;
	}
	Buffer[FileSize] = 0;
	fclose(File);
	return Buffer;
}

FILE *fopen_with_basepath(const char *Path, const char *Modes) {
	char RealPath[strlen(Path)+strlen(BasePath)+1];
	sprintf(RealPath, "%s%s", Path, BasePath);
	return fopen(RealPath, Modes);
}

int cJSON_Length(cJSON *Array) {
	if(!Array)
		return 0;
	int out = 1;
	while(Array->next) {
		out++;
		Array = Array->next;
	}
	return out;
}

int cJSON_IntValue(cJSON *JSON, const char *Var, int Default) {
	cJSON *Target = cJSON_GetObjectItem(JSON, Var);
	if(!Target) return Default;
	if(Target->type == cJSON_Number) return Target->valueint;
	if(Target->type == cJSON_False) return 0;
	if(Target->type == cJSON_True) return 1;
	return Default;
}

void cJSON_IntValueSet(cJSON *JSON, const char *Var, int Value) {
	cJSON *Target = cJSON_GetObjectItem(JSON, Var);
	if(Target->type == cJSON_Number) {
		Target->valueint = Value;
		Target->valuedouble = Value;
	}
	if(Target->type == cJSON_False || Target->type == cJSON_True)
		Target->type = Value?cJSON_True:cJSON_False;
}

cJSON *cJSON_Load(const char *Filename) {
	char *Buffer = ReadTextFile(Filename);
	cJSON *JSON = cJSON_Parse(Buffer);
	free(Buffer);
	if(!JSON) {
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s failed to load", Filename);
		char Temp[100];
		strlcpy(Temp, cJSON_GetErrorPtr(), sizeof(Temp));
		SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Error pointer: %s", Temp);
	}
	return JSON;
}

cJSON *cJSON_Search(cJSON *Root, char *SearchString) {
// SearchString is formatted like "Path1\\Path2\\Path3". Individual paths are like a/b/c/d/e

	char *Paths = SearchString;
	do {
		char *ThisPath = Paths;
		Paths = strchr(ThisPath, '\\');
		if(Paths) *(Paths++) = 0;

		cJSON *Find = Root;
		while(1) {
			char *ThisItem = ThisPath;
			ThisPath = strchr(ThisItem, '/');
			if(ThisPath) *(ThisPath++) = 0;
			Find = cJSON_GetObjectItem(Find,ThisItem);
			if(!Find) // try another path then
				break;
			if(!ThisPath) // no more items?
				return Find; // this is the right item
		}
	} while (Paths);
	return NULL;
}

const char *GetLevelStr(const char *Default, const char *s,...) {
	char Path[512]; va_list vl; va_start(vl, s);
	vsprintf(Path, s, vl); va_end(vl);
	cJSON *Find = cJSON_Search(LevelJSON, Path);
	if(Find) return Find->valuestring;
	return Default;
}

int GetLevelInt(int Default, const char *s,...) {
	char Path[512]; va_list vl; va_start(vl, s);
	vsprintf(Path, s, vl); va_end(vl);
	cJSON *Find = cJSON_Search(LevelJSON, Path);
	if(Find) return Find->valueint;
	return Default;
}

void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...) {
	va_list argp;
	va_start(argp, fmt);
	char Buffer[512];
	vsprintf(Buffer, fmt, argp);
	SDL_ShowSimpleMessageBox(Type, Title, Buffer, Window);
	va_end(argp);
}

int PathIsSafe(const char *Path) {
	if(strstr(Path, "..")) return 0; // no parent directories
	if(Path[0] == '/') return 0;		 // no root
	if(strchr(Path, ':')) return 0;	// no drives
	return 1;
}

void TextInterpolate(char *Out, const char *In, char Prefix, const char *ReplaceThis, const char *ReplaceWith[]) {
	while(*In) {
		if(*In != Prefix)
			*(Out++) = *(In++);
		else {
			In++;
			char *Find = strchr(ReplaceThis, *(In++));
			if(Find) {
				int This = Find - ReplaceThis;
				strcpy(Out, ReplaceWith[This]);
				Out += strlen(ReplaceWith[This]);
			} else {
				*(Out++) = Prefix;
				*(Out++) = In[-1];
			}
		}
	}
	*Out = 0;
}

#ifndef strlcpy
void strlcpy(char *Destination, const char *Source, int MaxLength) {
	// MaxLength is directly from sizeof() so it includes the zero
	int SourceLen = strlen(Source);
	if((SourceLen+1) < MaxLength)
		MaxLength = SourceLen + 1;
	memcpy(Destination, Source, MaxLength-1);
	Destination[MaxLength-1] = 0;
}
#endif

int memcasecmp(const char *Text1, const char *Text2, int Length) {
	for(;Length;Length--)
		if(tolower(*(Text1++)) != tolower(*(Text2++)))
			return 1;
	return 0;
}

const char *FilenameOnly(const char *Path) {
	const char *Temp = strrchr(Path, '/');
	if(!Temp) Temp = strrchr(Path, '\\');
	if(!Temp) return Path;
	return Temp+1;
}

SDL_Surface *SDL_LoadImage(const char *FileName, int Flags) {
	SDL_Surface* loadedSurface = IMG_Load(FileName);
	if(loadedSurface == NULL) {
		SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", window, "Unable to load image %s! SDL Error: %s", FileName, SDL_GetError());
		return NULL;
	}
	if(Flags & 1)
		SDL_SetColorKey(loadedSurface, SDL_TRUE, SDL_MapRGB(loadedSurface->format, 255, 0, 255));
	return loadedSurface;
}

SDL_Texture *LoadTexture(const char *FileName, int Flags) {
	SDL_Surface *Surface = SDL_LoadImage(FileName, Flags);
	if(!Surface) return NULL;
	SDL_Texture *Texture = SDL_CreateTextureFromSurface(ScreenRenderer, Surface);
	SDL_FreeSurface(Surface);
	return Texture;
}

// drawing functions
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2) {
	SDL_Rect Temp = {X1, Y1, abs(X2-X1)+1, abs(Y2-Y1)+1};
	SDL_RenderFillRect(Bmp,	&Temp);
}

void rect(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2) {
	SDL_Rect Temp = {X1, Y1, abs(X2-X1)+1, abs(Y2-Y1)+1};
	SDL_RenderDrawRect(Bmp, &Temp);
}

void sblit(SDL_Surface* SrcBmp, SDL_Surface* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height) {
	SDL_Rect Src = {SourceX, SourceY, Width, Height};
	SDL_Rect Dst = {DestX, DestY};
	SDL_BlitSurface(SrcBmp, &Src, DstBmp, &Dst);
}

void blit(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height) {
	SDL_Rect Src = {SourceX, SourceY, Width, Height};
	SDL_Rect Dst = {DestX, DestY, Width, Height};
	SDL_RenderCopy(DstBmp,	SrcBmp, &Src, &Dst);
}

void blitf(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, SDL_RendererFlip Flip) {
	SDL_Rect Src = {SourceX, SourceY, Width, Height};
	SDL_Rect Dst = {DestX, DestY, Width, Height};
	SDL_RenderCopyEx(DstBmp,	SrcBmp, &Src, &Dst, 0, NULL, Flip);
}

void blitz(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, int Width2, int Height2) {
	SDL_Rect Src = {SourceX, SourceY, Width, Height};
	SDL_Rect Dst = {DestX, DestY, Width2, Height2};
	SDL_RenderCopy(DstBmp,	SrcBmp, &Src, &Dst);
}

void blitfull(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int DestX, int DestY) {
	SDL_Rect Dst = {DestX, DestY};
	SDL_RenderCopy(DstBmp,	SrcBmp, NULL, &Dst);
}

