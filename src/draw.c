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

SDL_Color FGColor = {0x00, 0x00, 0x00, 255};
SDL_Color BGColor = {0xf0, 0xf0, 0xf0, 255};
SDL_Color SelectColor = {0xff, 0x00, 0xff, 255};
SDL_Color AvailableColor = {0x00, 0xff, 0xff, 255};
SDL_Color GridColor = {0xc0, 0xc0, 0xc0, 255};
SDL_Color WhiteColor = {0xff, 0xff, 0xff, 255};

SDL_Surface *SDL_LoadImage(const char *FileName, int Flags) {
	SDL_Surface* loadedSurface = IMG_Load(FileName);
	if(loadedSurface == NULL) {
		if((Flags & LOAD_TEXTURE_NO_ERROR) == 0)
			SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", window, "Unable to load image %s! SDL Error: %s", FileName, SDL_GetError());
		return NULL;
	}
	if(Flags & LOAD_TEXTURE_COLOR_KEY)
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
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int W, int H) {
	SDL_Rect Temp = {X1, Y1, W, H};
	SDL_RenderFillRect(Bmp,	&Temp);
}

void rect(SDL_Renderer *Bmp, int X1, int Y1, int W, int H) {
	SDL_Rect Temp = {X1, Y1, W, H};
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

void Free_FontSet(FontSet *Fonts) {
	if(Fonts->Font[0]) TTF_CloseFont(Fonts->Font[0]);
	if(Fonts->Font[1]) TTF_CloseFont(Fonts->Font[1]);
	if(Fonts->Font[2]) TTF_CloseFont(Fonts->Font[2]);
	if(Fonts->Font[3]) TTF_CloseFont(Fonts->Font[3]);
}

int Load_FontSet(FontSet *Fonts, int Size, const char *Font1, const char *Font2, const char *Font3, const char *Font4) {
	Fonts->Font[0] = NULL; Fonts->Font[1] = NULL;
	Fonts->Font[2] = NULL; Fonts->Font[3] = NULL;
	Fonts->Width = 0; Fonts->Height = 0;

	Fonts->Font[0] = TTF_OpenFont(Font1, Size);
	Fonts->Font[1] = TTF_OpenFont(Font2, Size);
	Fonts->Font[2] = TTF_OpenFont(Font3, Size);
	Fonts->Font[3] = TTF_OpenFont(Font4, Size);

	if(!Fonts->Font[0] || !Fonts->Font[1] || !Fonts->Font[2] || !Fonts->Font[3]) {
		SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Error loading one or more fonts - SDL_ttf Error: %s", TTF_GetError());
		Free_FontSet(Fonts);
		return 0;
	}

	TTF_SizeText(Fonts->Font[0], "N", &Fonts->Width, &Fonts->Height);
	return 1;
}

void RenderSimpleText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, int Flags, const char *Text) {
	// Skip if no text
	if(!*Text)
		return;
	SDL_Surface *TextSurface2 = TTF_RenderUTF8_Shaded(Font->Font[Flags&3], Text, FGColor, Flags&SIMPLE_TEXT_ON_WHITE?WhiteColor:BGColor);
	SDL_Surface *TextSurface = SDL_ConvertSurfaceFormat(TextSurface2, SDL_PIXELFORMAT_RGBA8888, 0);
	SDL_Texture *Texture;

	Texture = SDL_CreateTextureFromSurface(Renderer, TextSurface);
	blit(Texture, Renderer, 0, 0, X, Y, TextSurface->w, TextSurface->h);

	SDL_FreeSurface(TextSurface);
	SDL_FreeSurface(TextSurface2);
	SDL_DestroyTexture(Texture);
}

void RenderFormatText(SDL_Renderer *Renderer, FontSet *Font, int X, int Y, int Flags, const char *Text, ...) {
	va_list fmtargs;
	static char Buffer[1024];

	va_start(fmtargs, Text);
	vsnprintf(Buffer, sizeof(Buffer)-1, Text, fmtargs);
	va_end(fmtargs);

	RenderSimpleText(Renderer, Font, X, Y, Flags, Buffer);
}
