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

int ScreenWidth = 800, ScreenHeight = 600;
int quit = 0;
int MapViewX = 10, MapViewY = 10, MapViewWidth = 20, MapViewHeight = 20, MapViewWidthP, MapViewHeightP;

SDL_Window *window = NULL;
SDL_Renderer *ScreenRenderer = NULL;
SDL_Cursor *SystemCursors[SYSCURSOR_MAX];
SDL_Surface *WindowIcon = NULL;
int retraces = 0, SetACursor, CurLayer = 0;
int Redraw = 1, RerenderMap = 0;
FontSet MainFont, TinyFont, VeryTinyFont;
char TempText[100];

int CameraX = 0, CameraY = 0;
int DraggingMove = 0, DraggingResize = 0, DraggingSelect = 0;
int DraggingSelectX, DraggingSelectY;
int ResizeUp = 0, ResizeLeft = 0, ResizeRight = 0, ResizeDown = 0, ResizeOffer = 0;
int CursorX = 0, CursorY = 0, CursorShown = 0;
int TPCursorX = 0, TPCursorY = 0, TPTilesWide, TPTilesTall;
LevelRect *AvailableRect = NULL; // available level rectangle

SDL_Texture *TPTexture  = NULL;
SDL_Rect TPRect = {0, 0, 0, 0};

enum {
	MODE_LEVEL,
	MODE_PICKER
};
int EditorMode = MODE_LEVEL;

void GUI_SetCursor(int CursorNum) {
	static int WhatCursor = 0;
	if(CursorNum != WhatCursor)
		SDL_SetCursor(SystemCursors[CursorNum]);
	WhatCursor = CursorNum;
	SetACursor = 1;
};

SDL_Rect MakeSelectRect(LevelRect *Rect, int Offset) {
	SDL_Rect Select = {MapViewX+(Rect->X-CameraX)*TileW-Offset, MapViewY+(Rect->Y-CameraY)*TileH-Offset, Rect->W*TileW+Offset*2, Rect->H*TileH+Offset*2};
	return Select;
}

void WarpForLevelEditor() {
	SDL_WarpMouseInWindow(window, MapViewX+CursorX*TileW+TileW/2, MapViewY+CursorY*TileH+TileH/2);
}

void WarpForTP() {
	SDL_WarpMouseInWindow(window, TPRect.x+TPCursorX*(TileW+2)+(TileW+2)/2, TPRect.y+TPCursorY*(TileH+2)+(TileH+2)/2);
}

void UpdateTPRect() {
	int TPW, TPH;
	TPTexture = LayerInfos[CurLayer].Texture;
	SDL_QueryTexture(TPTexture, NULL, NULL, &TPW, &TPH);

	TPTilesWide = TPW / TileW;
	TPTilesTall = TPH / TileH;
				
	TPRect.x = ScreenWidth/2-(TPTilesWide*(TileW+2))/2;
	TPRect.y = ScreenHeight/2-(TPTilesTall*(TileH+2))/2;
	TPRect.w = TPTilesWide*(TileW+2);
	TPRect.h = TPTilesTall*(TileH+2);
}

void KeyDown(SDL_Keysym key) {
	int UndoMade = 0;

	switch(key.scancode) {
		case SDL_SCANCODE_1: CurLayer = 0; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_2: if(NumLayers >= 2) CurLayer = 1; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_3: if(NumLayers >= 3) CurLayer = 2; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_4: if(NumLayers >= 4) CurLayer = 3; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_5: if(NumLayers >= 5) CurLayer = 4; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_6: if(NumLayers >= 6) CurLayer = 5; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_7: if(NumLayers >= 7) CurLayer = 6; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_8: if(NumLayers >= 8) CurLayer = 7; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_9: if(NumLayers >= 9) CurLayer = 8; Redraw = 1; UpdateTPRect(); break;
		case SDL_SCANCODE_0: if(NumLayers >= 10) CurLayer = 9; Redraw = 1; UpdateTPRect(); break;
		default:
			break;
	}
	switch(key.sym) {
		case SDLK_z: // Undo
			Undo();
			RerenderMap = 1;
			Redraw = 1;
			break;
		case SDLK_y: // Redo
			Redo();
			RerenderMap = 1;
			Redraw = 1;
			break;

		case SDLK_e: // Toggle the picker
			if(EditorMode == MODE_LEVEL) {
				for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
					if(R->Selected) {
						int Type = LayerInfos[CurLayer].TilesetLookup[R->Type].Id;
						TPCursorX = Type & 0xff;
						TPCursorY = Type >> 8;
					}
				}

				UpdateTPRect();

				EditorMode = MODE_PICKER;
				WarpForTP();
			} else {
				EditorMode = MODE_LEVEL;
				WarpForLevelEditor();
			}
			Redraw = 1;
			break;

		case SDLK_s: // Save
			if(key.mod & KMOD_CTRL)
				SaveLevel();
			break;
		case SDLK_DELETE:
			AvailableRect = NULL;


			// Erase all selected rectangles
			LevelRect *R = LayerInfos[CurLayer].Rects;
			while(R) {
				if(R->Selected) {
					if(!UndoMade) {
						UndoMade = 1;
						UndoStep(CurLayer);
					}
					if(R->ExtraInfo)
						free(R->ExtraInfo);
					LevelRect *Prev = R->Prev;
					LevelRect *Next = R->Next;

					if(Prev)
						Prev->Next = R->Next;
					if(Next)
						Next->Prev = R->Prev;
					if(R == LayerInfos[CurLayer].Rects)
						LayerInfos[CurLayer].Rects = Next;
					free(R);
					R = Next;
					continue;
				}
				R = R->Next;
			}
			DraggingSelect = 0;
			RerenderMap = 1;
			Redraw = 1;
			break;
		default:
			break;
	}
}

void MouseMove(int x, int y) {
	SetACursor = 0;
	AvailableRect = NULL;

	if(EditorMode == MODE_LEVEL) {
		// If the mouse is inside the map view
		if(IsInsideRect(x, y, MapViewX, MapViewY, MapViewWidthP, MapViewHeightP)) {
            int NewX = (x - MapViewX) / TileW;
            int NewY = (y - MapViewY) / TileH;
			if(NewY < 0) return;

			// Offer a rectangle to click on
			AvailableRect = LayerInfos[CurLayer].Map[(NewY+CameraY)*LayerInfos[CurLayer].LayerWidth+(NewX+CameraX)].Rect;
			if(AvailableRect)
				GUI_SetCursor(SYSCURSOR_HAND);
			if(AvailableRect && AvailableRect->Selected)
				AvailableRect = NULL;

			// Check if dragging to resize should be offered
			if(!DraggingMove && !DraggingSelect && !DraggingResize) {
				ResizeOffer = 0;
				for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
					if(!R->Selected)
						continue;
					SDL_Rect Select1 = MakeSelectRect(R, 12);
					SDL_Rect Select2 = MakeSelectRect(R, 0);

					if(IsInsideRect(x, y, Select1.x, Select1.y, Select1.w, Select1.h) &&
						!IsInsideRect(x, y, Select2.x, Select2.y, Select2.w, Select2.h)) {
						ResizeUp    = y < Select2.y;
						ResizeDown  = y > (Select2.y + Select2.h);
						ResizeLeft  = x < Select2.x;
						ResizeRight = x > (Select2.x + Select2.w);
						ResizeOffer = 1;
					}

				}
			}

			// Resize cursor
			if(DraggingResize || ResizeOffer) {
				if((ResizeUp || ResizeDown) && !(ResizeLeft||ResizeRight))
					GUI_SetCursor(SYSCURSOR_SIZE_NS);
				if((ResizeLeft || ResizeRight) && !(ResizeUp||ResizeDown))
					GUI_SetCursor(SYSCURSOR_SIZE_EW);
				if((ResizeUp && ResizeLeft) || (ResizeDown && ResizeRight))
					GUI_SetCursor(SYSCURSOR_SIZE_NWSE);
				if((ResizeUp && ResizeRight) || (ResizeDown && ResizeLeft))
					GUI_SetCursor(SYSCURSOR_SIZE_NESW);
			}

			// Moved to another map tile
			if(NewX != CursorX || NewY != CursorY || !CursorShown) {
				Redraw = 1;
				int DiffX = NewX-CursorX;
				int DiffY = NewY-CursorY;

				// Drag selected rectangles
				if(DraggingMove) {
					for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
						if(R->Selected) {
							R->X += DiffX;
							R->Y += DiffY;
						}
					}
					RerenderMap = 1;
				}

				// Resize selected rectangles
				if(DraggingResize) {
					for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
						if(R->Selected) {
							if(ResizeUp) {
								R->Y += DiffY;
								R->H -= DiffY;
								if(R->Y < 0)
									R->Y = 0;
							}
							if(ResizeLeft) {
								R->X += DiffX;
								R->W -= DiffX;
							}
							if(ResizeDown)
								R->H += DiffY;
							if(ResizeRight)
								R->W += DiffX;
							if(R->W < 1) R->W = 1;
							if(R->H < 1) R->H = 1;
						}
					}
					RerenderMap = 1;
				}

				CursorX = NewX;
				CursorY = NewY;

				// Update the selections
				if(DraggingSelect) {
					// Start with nothing selected
					for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
						R->Selected = 0;
					}
					// Make sure these are all within range!
					int X1 = (CursorX < DraggingSelectX) ? CursorX : DraggingSelectX;
					int Y1 = (CursorY < DraggingSelectY) ? CursorY : DraggingSelectY;
					int X2 = (CursorX > DraggingSelectX) ? CursorX : DraggingSelectX;
					int Y2 = (CursorY > DraggingSelectY) ? CursorY : DraggingSelectY;
					// Mark everything inside the selection as selected
					for(int x=X1; x<=X2; x++) {
						for(int y=Y1; y<=Y2; y++) {
							LevelRect *R = LayerInfos[CurLayer].Map[(y+CameraY)*LayerInfos[CurLayer].LayerWidth+(x+CameraX)].Rect;
							if(R)
								R->Selected = 1;
						}
					}

				}
			}
			// Cursor is shown
			CursorShown = 1;
		} else {
			// Cursor is not in the map area, so don't draw it
			if(CursorShown) Redraw = 1;
			CursorShown = 0;
		}
	} else {
		if(IsInsideRect(x, y, TPRect.x, TPRect.y, TPRect.w, TPRect.h)) {
			TPCursorY = (y-TPRect.y) / (TileH+2);
			TPCursorX = (x-TPRect.x) / (TileW+2);
			Redraw = 1;
		}
	}

	// Normal cursor if none was set otherwise
	if(!SetACursor)
		GUI_SetCursor(SYSCURSOR_NORMAL);
}

void LeftClick() {
	if(EditorMode == MODE_LEVEL) {
		if(CursorShown) {
			if(ResizeOffer && !DraggingResize) {
				DraggingResize = 1;
				return;
			}

			LevelRect *Under = LayerInfos[CurLayer].Map[(CursorY+CameraY)*LayerInfos[CurLayer].LayerWidth+(CursorX+CameraX)].Rect;
			// Ctrl-click
			if(SDL_GetModState() & KMOD_CTRL) {
				// Ctrl causes the rectangle's selected flag to toggle
				if(Under)
					Under->Selected ^= 1;
			} else {
				// No Ctrl causes the thing clicked on to override everything else
				// but only if the thing being clicked on isn't selected.
				if(!Under || !Under->Selected) {					
					for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
						R->Selected = 0;
					}
				}
				if(Under) {
					Under->Selected = 1;
					// Start a drag
					DraggingMove = 1;
				} else {
					// Start a select
					DraggingSelectX = CursorX;
					DraggingSelectY = CursorY;
					DraggingSelect = 1;
				}
			}

			Redraw = 1;
		}
	} else {
		// Unselect all first
		for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
			R->Selected = 0;
		}

		LevelRect *End = LayerInfos[CurLayer].Rects;
		if(End != NULL)
			while(End->Next)
				End = End->Next;
		LevelRect *Copy = (LevelRect*)calloc(1, sizeof(LevelRect));

		int Type = TilesetLookupIdToIndex(CurLayer, (TPCursorY<<8)|TPCursorX);
		if(Type == - 1)
			return;

		Copy->Selected = 1;
		Copy->W = 1;
		Copy->H = 1;
		Copy->X = CursorX + CameraX;
		Copy->Y = CursorY + CameraY;
		Copy->Type = Type;

		// Set the links
		if(End == NULL) {
			LayerInfos[CurLayer].Rects = Copy;
		} else {
			Copy->Prev = End;
			Copy->Next = NULL;
			End->Next = Copy;
		}

		DraggingMove = 1;
		Redraw = 1;
		RerenderMap = 1;
		EditorMode = MODE_LEVEL;
		WarpForLevelEditor();
	}
}

void RightClick() {
	LevelRect *Base = NULL;

	// Right click copies all selected stuff
	LevelRect *End = LayerInfos[CurLayer].Rects;
	if(!End)
		return;

	// Simultaneously find the end as well as the base position to copy to
	while(1) {
		if(End->Selected) {
			if(!Base || Base->X > End->X || (Base->X == End->Y || Base->Y > End->Y)) {
				Base = End;
			}
		}

		if(End->Next)
			End = End->Next;
		else
			break;
	}

	// Nothing to copy?
	if(Base == NULL)
		return;

	LevelRect *Tail = End;

	// Paste all the selected rectangles to the end of the list
	for(LevelRect *R = LayerInfos[CurLayer].Rects; R!=End->Next; R=R->Next) {
		if(R->Selected) {
			LevelRect *Copy = (LevelRect*)malloc(sizeof(LevelRect));
			*Copy = *R;

			Copy->X = Copy->X - Base->X + CursorX + CameraX;
			Copy->Y = Copy->Y - Base->Y + CursorY + CameraY;
			if(Copy->ExtraInfo)
				Copy->ExtraInfo = strdup(Copy->ExtraInfo);

			// Fix up the links
			Copy->Prev = Tail;
			Copy->Next = NULL;
			Tail->Next = Copy;
			Tail = Copy;
			
			// New one is selected, old one no longer is
			R->Selected = 0;
		}
	}

	DraggingMove = 1;
	Redraw = 1;
	RerenderMap = 1;
}

void TextInput(char Key) {
	switch(Key) {
		case 'a': CameraX--; Redraw = 1; break;
		case 's': CameraY++; Redraw = 1; break;
		case 'd': CameraX++; Redraw = 1; break;
		case 'w': CameraY--; Redraw = 1; break;
		case 'A': CameraX-=10; Redraw = 1; break;
		case 'S': CameraY+=10; Redraw = 1; break;
		case 'D': CameraX+=10; Redraw = 1; break;
		case 'W': CameraY-=10; Redraw = 1; break;

		case 'i':
			if(EditorMode == MODE_PICKER) {
				TPCursorY = (TPCursorY-1) & 15;
				WarpForTP();
				Redraw = 1;
				return;
			}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Y--;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'j':
			if(EditorMode == MODE_PICKER) {
				TPCursorX = (TPCursorX-1) & 15;
				WarpForTP();
				Redraw = 1;
				return;
			}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->X--;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'k':
			if(EditorMode == MODE_PICKER) {
				TPCursorY = (TPCursorY+1) & 15;
				WarpForTP();
				Redraw = 1;
				return;
			}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Y++;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'l':
			if(EditorMode == MODE_PICKER) {
				TPCursorX = (TPCursorX+1) & 15;
				WarpForTP();
				Redraw = 1;
				return;
			}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->X++;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'I':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected && R->H > 1)
					R->H--;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'J':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected && R->W > 1)
					R->W--;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'K':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->H++;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'L':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->W++;
			Redraw = 1;
			RerenderMap = 1;
			break;

		case 'x':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Flips ^= SDL_FLIP_HORIZONTAL;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'y':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Flips ^= SDL_FLIP_VERTICAL;
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'Y':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Flips = 0;
			Redraw = 1;
			RerenderMap = 1;
			break;


	}
}

void DrawGUI() {
	if(RerenderMap == 1) { // Rerender the first one
		RenderLevelRects(CurLayer);
	}
	if(RerenderMap == 2) { // Rerender all
		for(int i=0; i<NumLayers; i++)
			RenderLevelRects(CurLayer);
	}
	RerenderMap = 0;

	// Recalculate map size information, just in case it was resized
	MapViewWidth = (ScreenWidth - 30) / TileW;
	MapViewHeight = (ScreenHeight - 80) / TileH;
	if(MapViewWidth > LevelW) MapViewWidth = LevelW;
	if(MapViewHeight > LevelH) MapViewHeight = LevelH;
	MapViewWidthP = MapViewWidth*TileW;
	MapViewHeightP = MapViewHeight*TileH;
	MapViewX = (ScreenWidth / 2) - (MapViewWidthP / 2);
	MapViewY = ((ScreenHeight - 80) / 2) - (MapViewHeightP / 2) + 80;

	if(CameraX < 0) CameraX = 0;
	if(CameraY < 0) CameraY = 0;
	if((CameraX + MapViewWidth) > LevelW) CameraX = LevelW - MapViewWidth;
	if((CameraY + MapViewHeight) > LevelH) CameraY = LevelH - MapViewHeight;
	SDL_SetRenderDrawColor(ScreenRenderer, BGColor.r, BGColor.g, BGColor.b, 255);
	SDL_RenderClear(ScreenRenderer);

	// ------------------------------------------------------------------------

	// Info about the current layer
	char Temp3[256];
	sprintf(Temp3, "Layer %i (%s)", CurLayer+1, LayerInfos[CurLayer].Name);
	// Display the current sprite count if it's a sprite layer
	// since engines may limit that.
	if(LayerInfos[CurLayer].Type == LAYER_SPRITE) {
		int Total = 0;
		for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
			Total++;
		sprintf(TempText, " Count: %i", Total);
		strcat(Temp3, TempText);
	}
	RenderSimpleText(ScreenRenderer, &MainFont, 5, 5+MainFont.Height*0, 0, Temp3);

	// Display info about the currently selected thing(s)
	int SelectedCount = 0;
	LevelRect *SelectedRect = NULL;
	for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
		if(R->Selected) {
			SelectedRect = R;
			SelectedCount++;
		}
	}
	if(SelectedCount == 1) {
		char Temp2[30] = "";
		if(SelectedRect->W + SelectedRect->H != 2)
			sprintf(Temp2, "<%ix%i> ", SelectedRect->W, SelectedRect->H);
		sprintf(TempText, "%s %s%s", LayerInfos[CurLayer].TilesetLookup[SelectedRect->Type].Name, Temp2, SelectedRect->ExtraInfo?SelectedRect->ExtraInfo:"");
		int Graphic = LayerInfos[CurLayer].TilesetLookup[SelectedRect->Type].Id;
		blitf(LayerInfos[CurLayer].Texture, ScreenRenderer, (Graphic&255)*TileW, (Graphic>>8)*TileH, 5, 5+MainFont.Height*1, TileW, TileH, SelectedRect->Flips);
		RenderSimpleText(ScreenRenderer, &MainFont, 5+TileW+5, 5+MainFont.Height*1, 0, TempText);
	} else if(SelectedCount > 1) {
		RenderFormatText(ScreenRenderer, &MainFont, 5+TileW+5, 5+MainFont.Height*1, 0, "(%i selected)", SelectedCount);
	}

	// Replace this with the toolbar later
	RenderSimpleText(ScreenRenderer, &MainFont, 5, 5+MainFont.Height*2, 0, "1-9 change layers, wasd: scroll, ijkl: move, e: insert");

	// Infobar line
	SDL_SetRenderDrawColor(ScreenRenderer, FGColor.r, FGColor.g, FGColor.b, 255);
	SDL_RenderDrawLine(ScreenRenderer, 0, 60, ScreenWidth, 60);


	// Redraw the map 
	for(int L=0;L<NumLayers;L++) {
		if(!LayerInfos[L].LayerHidden)
			for(int y=0;y<MapViewHeight;y++)
				for(int x=0;x<MapViewWidth;x++) {
					int RealX = x+CameraX, RealY = y+CameraY;
					LevelTile Tile = LayerInfos[L].Map[RealY*LayerInfos[L].LayerWidth+RealX];

					// Is there a tile here?
					if(Tile.Graphic != -1) {
						// Draw with flips
						if(Tile.Flips == 0)
							blit(LayerInfos[L].Texture, ScreenRenderer, (Tile.Graphic&255)*TileW, (Tile.Graphic>>8)*TileH, MapViewX+x*TileW, MapViewY+y*TileH, TileW, TileH);
						else
							blitf(LayerInfos[L].Texture, ScreenRenderer, (Tile.Graphic&255)*TileW, (Tile.Graphic>>8)*TileH, MapViewX+x*TileW, MapViewY+y*TileH, TileW, TileH, Tile.Flips);
					}
				}
	}

	// Hovering over a rectangle? Display that
	if(AvailableRect) {
		SDL_SetRenderDrawColor(ScreenRenderer, AvailableColor.r, AvailableColor.g, AvailableColor.b, 255);  
		SDL_Rect Select = MakeSelectRect(AvailableRect, 4);
		SDL_RenderDrawRect(ScreenRenderer, &Select);
	}

	// Show selections
	for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
		if(!R->Selected)
			continue;
		SDL_SetRenderDrawColor(ScreenRenderer, SelectColor.r, SelectColor.g, SelectColor.b, 255);
		SDL_Rect Select = MakeSelectRect(R, 8);
		SDL_RenderDrawRect(ScreenRenderer, &Select);

		if(LayerInfos[CurLayer].Type == LAYER_SPRITE) {
			if(R->Flips == SDL_FLIP_HORIZONTAL)
				SDL_RenderDrawLine(ScreenRenderer, Select.x-1, Select.y+Select.h/2, Select.x-10, Select.y+Select.h/2);
			else
				SDL_RenderDrawLine(ScreenRenderer, Select.x+Select.w, Select.y+Select.h/2, Select.x+Select.w+11, Select.y+Select.h/2);
		}
	}

	// Draw the grid
	if(GridW) {
		SDL_Rect Clip = {MapViewX, MapViewY, MapViewWidthP, MapViewHeightP};
		SDL_RenderSetClipRect(ScreenRenderer, &Clip);
		SDL_SetRenderDrawColor(ScreenRenderer, GridColor.r, GridColor.g, GridColor.b, 255);
		for(int i=0; i*GridW<(MapViewWidth+GridW); i++)
			for(int j=0; j*GridH<(MapViewHeight+GridH); j++) {
				SDL_Rect GridRect = {MapViewX+(i*GridW-CameraX%GridW)*TileW, MapViewY+(j*GridH-CameraY%GridH)*TileH, GridW*TileW, GridH*TileH};
				SDL_RenderDrawRect(ScreenRenderer, &GridRect);
			}
		SDL_RenderSetClipRect(ScreenRenderer, NULL);
	}

	// Put column and row numbers
	for(int n=0;n<MapViewWidth;n++) {
		if(!((n+CameraX)&1)) {
			RenderFormatText(ScreenRenderer, &TinyFont, MapViewX+n*TileW, MapViewY-16, 0, "%.3X", n+CameraX);
			SDL_RenderDrawLine(ScreenRenderer, MapViewX+n*TileW+TileW/2, MapViewY, MapViewX+n*TileW+TileW/2, MapViewY-4);
		} else {
			SDL_RenderDrawLine(ScreenRenderer, MapViewX+n*TileW+TileW/2, MapViewY, MapViewX+n*TileW+TileW/2, MapViewY-16);
		}
	}
	for(int n=0;n<MapViewHeight;n++) {
		RenderFormatText(ScreenRenderer, &TinyFont, MapViewX-15, MapViewY+n*TileH+TileH/2-TinyFont.Height/2, 0, "%.2X", n+CameraY);
	}

	// Draw cursor if applicable
	if(DraggingSelect) {
		int X1 = (CursorX < DraggingSelectX) ? CursorX : DraggingSelectX;
		int Y1 = (CursorY < DraggingSelectY) ? CursorY : DraggingSelectY;
		int X2 = (CursorX > DraggingSelectX) ? CursorX : DraggingSelectX;
		int Y2 = (CursorY > DraggingSelectY) ? CursorY : DraggingSelectY;

		SDL_Rect Select = {MapViewX+X1*TileW, MapViewY+Y1*TileH, (X2-X1+1)*TileW, (Y2-Y1+1)*TileH};
		SDL_SetRenderDrawColor(ScreenRenderer, SelectColor.r, SelectColor.g, SelectColor.b, 255);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_MOD);
		SDL_RenderFillRect(ScreenRenderer, &Select);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_NONE);
		SDL_RenderDrawRect(ScreenRenderer, &Select);
	} else if(CursorShown) {
		SDL_Rect Select = {MapViewX+CursorX*TileW, MapViewY+CursorY*TileH, TileW, TileH};
		SDL_SetRenderDrawColor(ScreenRenderer, SelectColor.r, SelectColor.g, SelectColor.b, 255);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_MOD);
		SDL_RenderFillRect(ScreenRenderer, &Select);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_NONE);
		SDL_RenderDrawRect(ScreenRenderer, &Select);
	}

	// Draw outline around the map edit view
	SDL_SetRenderDrawColor(ScreenRenderer, FGColor.r, FGColor.g, FGColor.b, 255);
	rect(ScreenRenderer, MapViewX, MapViewY, MapViewX+MapViewWidthP, MapViewY+MapViewHeightP);

	// Draw the tile picker
	if(EditorMode == MODE_PICKER) {
		SDL_SetRenderDrawColor(ScreenRenderer, 255, 255, 255, 255);
		SDL_RenderFillRect(ScreenRenderer, &TPRect);

		for(int i=0; i<TPTilesWide; i++) {
			for(int j=0; j<TPTilesTall; j++) {
				blit(TPTexture, ScreenRenderer, i*TileW, j*TileH, TPRect.x+i*(TileW+2)+1, TPRect.y+j*(TileH+2)+1, TileW, TileH);
			}
		}

		// Cursor
		SDL_Rect Select = {TPRect.x+TPCursorX*(TileW+2), TPRect.y+TPCursorY*(TileH+2), TileW+2, TileH+2};
		SDL_SetRenderDrawColor(ScreenRenderer, SelectColor.r, SelectColor.g, SelectColor.b, 255);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_MOD);
		SDL_RenderFillRect(ScreenRenderer, &Select);
		SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_NONE);
		SDL_RenderDrawRect(ScreenRenderer, &Select);

		// Draw the outline
		SDL_SetRenderDrawColor(ScreenRenderer, FGColor.r, FGColor.g, FGColor.b, 255);
		rect(ScreenRenderer, TPRect.x-1, TPRect.y-1, TPRect.x+TPRect.w, TPRect.y+TPRect.h);
	}

	SDL_RenderPresent(ScreenRenderer);
}

void run_gui() {
	// .-----------------------------------------------------------------------
	// | INITIALIZE EVERYTHING
	// '-----------------------------------------------------------------------
	window = SDL_CreateWindow("Princess Edit 2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ScreenWidth, ScreenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	if(!window) {
		 SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Window could not be created! SDL_Error: %s", SDL_GetError());
		 return;
	}
	if(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)){
		SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_image could not initialize! SDL_image Error: %s", IMG_GetError());
		return;
	}
	if( TTF_Init() == -1 ) {
		SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_ttf could not initialize! SDL_ttf Error: %s", TTF_GetError());
		return;
	}
	ScreenRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

	// set window icon
	WindowIcon = SDL_LoadImage("data/icon.png", 0);
	SDL_SetWindowIcon(window, WindowIcon);

	// load cursors
	SystemCursors[SYSCURSOR_NORMAL] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	SystemCursors[SYSCURSOR_WAIT] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	SystemCursors[SYSCURSOR_SIZE_NS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	SystemCursors[SYSCURSOR_SIZE_EW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	SystemCursors[SYSCURSOR_SIZE_NWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
	SystemCursors[SYSCURSOR_SIZE_NESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
	SystemCursors[SYSCURSOR_SIZE_ALL] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEALL);
	SystemCursors[SYSCURSOR_HAND] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	SystemCursors[SYSCURSOR_DISABLE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);

	if (!Load_FontSet(&MainFont, 14, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
		return;
	if (!Load_FontSet(&TinyFont, 10, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
		return;
	if (!Load_FontSet(&VeryTinyFont, 8, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
		return;
	LoadTilesets();

	// .-----------------------------------------------------------------------
	// | EVENT LOOP
	// '-----------------------------------------------------------------------

	SDL_Event e;
	while(!quit) {
		while(SDL_PollEvent(&e) != 0) {
			if(e.type == SDL_QUIT) {
				quit = 1;
			} else if(e.type == SDL_MOUSEMOTION) {
				MouseMove(e.motion.x, e.motion.y);
			} else if(e.type == SDL_KEYDOWN) {
				KeyDown(e.key.keysym);
			} else if(e.type == SDL_MOUSEBUTTONDOWN) {
				if(e.button.button == SDL_BUTTON_LEFT)
					LeftClick();
				if(e.button.button == SDL_BUTTON_RIGHT)
					RightClick();
			} else if(e.type == SDL_MOUSEBUTTONUP) {
				if(e.button.button == SDL_BUTTON_LEFT || e.button.button == SDL_BUTTON_RIGHT) {
					DraggingMove = 0;
					DraggingResize = 0;
					if(DraggingSelect)
						Redraw = 1;
					DraggingSelect = 0;
				}
			} else if(e.type == SDL_TEXTINPUT) {
				TextInput(*e.text.text);
			} else if(e.type == SDL_WINDOWEVENT) {
				switch(e.window.event) {
					case SDL_WINDOWEVENT_SIZE_CHANGED:
						ScreenWidth = e.window.data1;
						ScreenHeight = e.window.data2;
					case SDL_WINDOWEVENT_EXPOSED:
						Redraw = 1;
				}
			}
		}
		if(Redraw) {
			DrawGUI();
			Redraw = 0;
		}

		SDL_Delay(17);
		retraces++;
	}
	for(int i=0;i<SYSCURSOR_MAX;i++)
		SDL_FreeCursor(SystemCursors[i]);
	Free_FontSet(&MainFont);
	Free_FontSet(&TinyFont);
	Free_FontSet(&VeryTinyFont);

	SDL_Quit();
}
