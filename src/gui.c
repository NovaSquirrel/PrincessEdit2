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
#include <math.h>

int ScreenWidth = 800, ScreenHeight = 600;
int quit = 0;
int MapViewX = 10, MapViewY = 10, MapViewWidth = 20, MapViewHeight = 20, MapViewWidthP, MapViewHeightP;

SDL_Window *window = NULL;
SDL_Renderer *ScreenRenderer = NULL;
SDL_Cursor *SystemCursors[SYSCURSOR_MAX];
SDL_Surface *WindowIcon = NULL;
SDL_Texture *UITexture = NULL;
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
int SavedTPX = -1, SavedTPY = -1;
LevelRect *AvailableRect = NULL; // available level rectangle

SDL_Texture *TPTexture  = NULL;
SDL_Rect TPRect = {0, 0, 0, 0};
SDL_Rect TPCategoryNameRect = {0, 0, 0, 0};
SDL_Rect TPCategoriesRect = {0, 0, 0, 0};
SDL_Rect TPHotkeysRect = {0, 0, 0, 0};
SDL_Rect TPBlockNameRect = {0, 0, 0, 0};
SDL_Rect TPBlocksRect = {0, 0, 0, 0};
#define TP_TEXT_HEIGHT 20
#define TP_CATEGORY_HEIGHT 36
#define TP_HOTKEYS_HEIGHT 18
#define TP_BLOCKS_PER_ROW 20
int TPRows = 16;
char TPSearchText[64] = "";

int TPBlocksInCategory[MAX_TILESET_SIZE];
int TPBlocksInCategoryCount;
int TPBlocksYScroll = 0;

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
	SDL_GetMouseState(&SavedTPX, &SavedTPY);
	SDL_WarpMouseInWindow(window, MapViewX+CursorX*TileW+TileW/2, MapViewY+CursorY*TileH+TileH/2);
}

void WarpForTP() {
	if(SavedTPX != -1 && SavedTPY != -1)
		SDL_WarpMouseInWindow(window, SavedTPX, SavedTPY);
	else
		SDL_WarpMouseInWindow(window, TPRect.x+TPRect.w/2, TPRect.y+TPRect.h/2);
}

int MatchesSearch(const char *name) {
	if(!TPSearchText[0])
		return 1;
	char lower[strlen(name)+1];
	for(int i=0; name[i]; i++) {
		lower[i] = tolower(name[i]);
	}
	lower[strlen(name)] = 0;

	return strstr(lower, TPSearchText) != NULL;
}

void UpdateTPRect() {
	TPRect.w = 360;
	TPCategoryNameRect.w = TPRect.w;
	TPCategoriesRect.w   = TPRect.w;
	TPHotkeysRect.w      = TPRect.w;
	TPBlockNameRect.w    = TPRect.w;
	TPBlocksRect.w       = TPRect.w;

	TPRect.x = ScreenWidth/2-TPRect.w/2;
	TPCategoryNameRect.x = TPRect.x;
	TPCategoriesRect.x   = TPRect.x;
	TPHotkeysRect.x      = TPRect.x;
	TPBlockNameRect.x    = TPRect.x;
	TPBlocksRect.x       = TPRect.x;

	// Calculate Y positions
	TPCategoryNameRect.y = 0;
	TPCategoryNameRect.h = TP_TEXT_HEIGHT;
	TPCategoriesRect.y   = TPCategoryNameRect.y + TPCategoryNameRect.h + 1;
	TPCategoriesRect.h   = TP_CATEGORY_HEIGHT;
	TPHotkeysRect.y      = TPCategoriesRect.y + TPCategoriesRect.h + 1;
	TPHotkeysRect.h      = TP_HOTKEYS_HEIGHT;
	TPBlockNameRect.y    = TPHotkeysRect.y + TPHotkeysRect.h + 1;
	TPBlockNameRect.h    = TP_TEXT_HEIGHT;
	TPBlocksRect.y       = TPBlockNameRect.y + TPBlockNameRect.h + 1;
	TPBlocksRect.h       = 18 * TPRows+1;

	// Center it on the screen
	TPRect.h = TPBlocksRect.y + TPBlocksRect.h;
	TPRect.y = ScreenHeight/2-TPRect.h/2;

	// Put everything on the window with the new calculated "center of screen" position
	TPCategoryNameRect.y += TPRect.y;
	TPCategoriesRect.y   += TPRect.y;
	TPHotkeysRect.y      += TPRect.y;
	TPBlockNameRect.y    += TPRect.y;
	TPBlocksRect.y       += TPRect.y;

	// Find out what stuff is in the current category...
	int last_category_index = NUM_SPECIAL_CATEGORIES + LayerInfos[CurLayer].Tileset->CategoryCount - 1;

	if(LayerInfos[CurLayer].CurrentCategory == 0) { // Everything
		TPBlocksInCategoryCount = 0;
		for(int i=0; i<LayerInfos[CurLayer].Tileset->TileCount; i++)
			if(MatchesSearch(LayerInfos[CurLayer].Tileset->TilesetLookup[i].Name))
				TPBlocksInCategory[TPBlocksInCategoryCount++] = i;
	} else if(LayerInfos[CurLayer].CurrentCategory == last_category_index) { // Recent
		TPBlocksInCategoryCount = 0;
	} else if(LayerInfos[CurLayer].CurrentCategory == last_category_index-1) { // Favorites
		TPBlocksInCategoryCount = 0;
	} else { // Everything else
		TPBlocksInCategoryCount = 0;
		int category_mask = 1 << (LayerInfos[CurLayer].CurrentCategory - 1); // Skip the "everything" category
		for(int i=0; i<LayerInfos[CurLayer].Tileset->TileCount; i++) {
			if((LayerInfos[CurLayer].Tileset->TilesetLookup[i].Categories & category_mask)
				&& MatchesSearch(LayerInfos[CurLayer].Tileset->TilesetLookup[i].Name)) {
				TPBlocksInCategory[TPBlocksInCategoryCount++] = i;
			}
		}
	}
}

inline SDL_RendererFlip FlipsFromTile(uint16_t tile) {
	return ((tile&0x4000)?SDL_FLIP_HORIZONTAL:0) | ((tile&0x8000)?SDL_FLIP_VERTICAL:0);
}

inline int YFromTile(uint16_t tile) {
	return (tile & 0x3f00) >> 8;
}

inline int XFromTile(uint16_t tile) {
	return tile & 0x00ff;
}

void DrawTilesetEntry(TilesetEntry *tile, int drawX, int drawY, SDL_RendererFlip flip) {
	switch(tile->Style) {
		case STYLE_SINGLE:
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Single.Tile)*TileW/2, YFromTile(tile->Single.Tile)*TileH/2,
				drawX, drawY, TileW, TileH, flip ^ FlipsFromTile(tile->Single.Tile));
			break;
		case STYLE_QUAD:
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Quad.Tiles[0])*TileW/2,
				YFromTile(tile->Quad.Tiles[0])*TileH/2,
				drawX+((flip&SDL_FLIP_HORIZONTAL)?8:0), drawY+((flip&SDL_FLIP_VERTICAL)?8:0),
				TileW/2, TileH/2, flip ^ FlipsFromTile(tile->Quad.Tiles[0]));
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Quad.Tiles[1])*TileW/2,
				YFromTile(tile->Quad.Tiles[1])*TileH/2,
				drawX+((flip&SDL_FLIP_HORIZONTAL)?0:8), drawY+((flip&SDL_FLIP_VERTICAL)?8:0),
				TileW/2, TileH/2, flip ^ FlipsFromTile(tile->Quad.Tiles[1]));
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Quad.Tiles[2])*TileW/2,
				YFromTile(tile->Quad.Tiles[2])*TileH/2,
				drawX+((flip&SDL_FLIP_HORIZONTAL)?8:0), drawY+((flip&SDL_FLIP_VERTICAL)?0:8),
				TileW/2, TileH/2, flip ^ FlipsFromTile(tile->Quad.Tiles[2]));
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Quad.Tiles[3])*TileW/2,
				YFromTile(tile->Quad.Tiles[3])*TileH/2,
				drawX+((flip&SDL_FLIP_HORIZONTAL)?0:8), drawY+((flip&SDL_FLIP_VERTICAL)?0:8),
				TileW/2, TileH/2, flip ^ FlipsFromTile(tile->Quad.Tiles[3]));
			break;
		case STYLE_RIGHT_TRIANGLE:
			blitf(tile->Texture, ScreenRenderer,
				XFromTile(tile->Shape.Tile)*TileW/2, YFromTile(tile->Shape.Tile)*TileH/2,
				drawX, drawY, TileW, TileH, flip ^ FlipsFromTile(tile->Shape.Tile));
			break;
	}

}

void PressedDigit(int which) {
	if(EditorMode == MODE_LEVEL) {
		if(NumLayers >= which + 1) {
			CurLayer = which;
			Redraw = 1;
			UpdateTPRect();
		}
	} else if(EditorMode == MODE_PICKER) {
		if(SDL_GetModState() & KMOD_CTRL) { // Set
			int hovered_block = (TPCursorY+TPBlocksYScroll) * TP_BLOCKS_PER_ROW + TPCursorX;
			if(hovered_block < 0 && hovered_block >= TPBlocksInCategoryCount)
				return;
			LayerInfos[CurLayer].Tileset->HotbarCategory[which] = LayerInfos[CurLayer].CurrentCategory;
			LayerInfos[CurLayer].Tileset->HotbarX[which] = TPCursorX;
			LayerInfos[CurLayer].Tileset->HotbarY[which] = TPCursorY;
			LayerInfos[CurLayer].Tileset->HotbarScroll[which] = TPBlocksYScroll;
			LayerInfos[CurLayer].Tileset->HotbarType[which] = TPBlocksInCategory[hovered_block];
			Redraw = 1;
		} else { // Get
			LayerInfos[CurLayer].CurrentCategory = LayerInfos[CurLayer].Tileset->HotbarCategory[which];
			TPCursorX = LayerInfos[CurLayer].Tileset->HotbarX[which];
			TPCursorY = LayerInfos[CurLayer].Tileset->HotbarY[which];
			TPBlocksYScroll = LayerInfos[CurLayer].Tileset->HotbarScroll[which];
			Redraw = 1;
			UpdateTPRect();
			SDL_WarpMouseInWindow(window, TPBlocksRect.x+1+TPCursorX*18+TileW/2, TPBlocksRect.y+1+TPCursorY*18+TileH/2);
		}
	}
}

void KeyDown(SDL_Keysym key) {
	int UndoMade = 0;

	switch(key.scancode) {
		case SDL_SCANCODE_1: PressedDigit(0); break;
		case SDL_SCANCODE_2: PressedDigit(1); break;
		case SDL_SCANCODE_3: PressedDigit(2); break;
		case SDL_SCANCODE_4: PressedDigit(3); break;
		case SDL_SCANCODE_5: PressedDigit(4); break;
		case SDL_SCANCODE_6: PressedDigit(5); break;
		case SDL_SCANCODE_7: PressedDigit(6); break;
		case SDL_SCANCODE_8: PressedDigit(7); break;
		case SDL_SCANCODE_9: PressedDigit(8); break;
		case SDL_SCANCODE_0: PressedDigit(9); break;
		default:
			break;
	}
	switch(key.sym) {
		case SDLK_z: // Undo
			if((SDL_GetModState() & KMOD_CTRL) == 0)
				break;
			Undo();
			RerenderMap = 1;
			Redraw = 1;
			break;
		case SDLK_y: // Redo
			if((SDL_GetModState() & KMOD_CTRL) == 0)
				break;
			Redo();
			RerenderMap = 1;
			Redraw = 1;
			break;

		case SDLK_e: // Toggle the picker
			if(EditorMode == MODE_LEVEL) {
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
	} else if(EditorMode == MODE_PICKER){
		if(IsInsideRect(x, y, TPBlocksRect.x, TPBlocksRect.y, TPBlocksRect.w, TPBlocksRect.h)) {
			TPCursorY = (y-TPBlocksRect.y) / (TileH+2);
			TPCursorX = (x-TPBlocksRect.x) / (TileW+2);
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

		LevelRect *End = LevelEndRect(CurLayer);
		LevelRect *Copy = (LevelRect*)calloc(1, sizeof(LevelRect));

		int index = (TPBlocksYScroll + TPCursorY) * TP_BLOCKS_PER_ROW + TPCursorX;
		if(index < 0 || index >= TPBlocksInCategoryCount)
			return;

		Copy->Selected = 1;
		Copy->W = 1;
		Copy->H = 1;
		Copy->X = CursorX + CameraX;
		Copy->Y = CursorY + CameraY;
		Copy->Type = TPBlocksInCategory[index];

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
	LevelRect *End, *Insert, *Scan;
	int Temp, Temp2;

	switch(Key) {
		case '-': // Background
			Scan = LayerInfos[CurLayer].Rects;
			if(!Scan)
				break;
			Scan = Scan->Next; // Skip the first

			while(Scan) {
				LevelRect *After = Scan->Next;
				if(Scan->Selected) {
					SDL_Rect Bounds = {Scan->X, Scan->Y, Scan->W, Scan->H};
					// Seek backwards until an overlapping rectangle is found
					for(LevelRect *V = Scan->Prev; V; V=V->Prev) {
						SDL_Rect Bounds2 = {V->X, V->Y, V->W, V->H};
						if(SDL_HasIntersection(&Bounds, &Bounds2)) {
							// Remove from the old location
							if(Scan->Prev)
								Scan->Prev->Next = Scan->Next;
							if(Scan->Next)
								Scan->Next->Prev = Scan->Prev;

							// Insert into the new one
							Scan->Prev = V->Prev;
							Scan->Next = V;
							if(Scan->Prev)
								Scan->Prev->Next = Scan;
							V->Prev = Scan;

							// Found one, so stop looking for one
							break;
						}
					}
				}
				Scan = After;
			}

			Redraw = 1;
			RerenderMap = 1;
			break;

		case '=': // Foreground
			End = LevelEndRect(CurLayer);
			Insert = End;
			Scan = End;

			while(Scan) {
				LevelRect *After = Scan->Prev;
				if(Scan->Selected) {
					SDL_Rect Bounds = {Scan->X, Scan->Y, Scan->W, Scan->H};
					// Seek forwards until an overlapping rectangle is found
					for(LevelRect *V = Scan->Next; V; V=V->Next) {
						SDL_Rect Bounds2 = {V->X, V->Y, V->W, V->H};
						if(SDL_HasIntersection(&Bounds, &Bounds2)) {
							// Remove from the old location
							if(Scan == LayerInfos[CurLayer].Rects)
								LayerInfos[CurLayer].Rects = Scan->Next;
							if(Scan->Prev)
								Scan->Prev->Next = Scan->Next;
							if(Scan->Next)
								Scan->Next->Prev = Scan->Prev;

							// Insert into the new one
							Scan->Next = V->Next;
							Scan->Prev = V;
							if(Scan->Next)
								Scan->Next->Prev = Scan;
							V->Next = Scan;

							// Found one, so stop looking for one
							break;
						}
					}
				}
				Scan = After;
			}

			Redraw = 1;
			RerenderMap = 1;
			break;

		case '_': // Background (all the way)
			Scan = LayerInfos[CurLayer].Rects;
			if(!Scan)
				break;
			Scan = Scan->Next; // Skip the first

			while(Scan) {
				LevelRect *After = Scan->Next;
				if(Scan->Selected) {
					// Remove from the old location
					if(Scan->Prev)
						Scan->Prev->Next = Scan->Next;
					if(Scan->Next)
						Scan->Next->Prev = Scan->Prev;

					// Insert into the new one
					Scan->Prev = NULL;
					Scan->Next = LayerInfos[CurLayer].Rects;
					LayerInfos[CurLayer].Rects->Prev = Scan;
					LayerInfos[CurLayer].Rects = Scan;
				}
				Scan = After;
			}

			Redraw = 1;
			RerenderMap = 1;
			break;

		case '+': // Foreground (all the way)
			End = LevelEndRect(CurLayer);
			Insert = End;
			Scan = End;
			if(!Scan)
				break;
			Scan = Scan->Prev; // Skip the last

			while(Scan) {
				LevelRect *After = Scan->Prev;
				if(Scan->Selected) {
					// Remove from the old location
					if(Scan == LayerInfos[CurLayer].Rects)
						LayerInfos[CurLayer].Rects = Scan->Next;
					if(Scan->Prev)
						Scan->Prev->Next = Scan->Next;
					if(Scan->Next)
						Scan->Next->Prev = Scan->Prev;

					// Insert into the new one
					Insert->Next = Scan;
					Scan->Prev = Insert;
					Scan->Next = NULL;
					Insert = Scan;
				}
				Scan = After;
			}

			Redraw = 1;
			RerenderMap = 1;
			break;

		case 'a':
			if(EditorMode == MODE_LEVEL) {
				CameraX--; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				if(LayerInfos[CurLayer].CurrentCategory == 0) {
					LayerInfos[CurLayer].CurrentCategory = LayerInfos[CurLayer].Tileset->CategoryCount + NUM_SPECIAL_CATEGORIES - 1;
				} else {
					LayerInfos[CurLayer].CurrentCategory--;
				}
				TPBlocksYScroll = 0;
				UpdateTPRect();
				Redraw = 1;
			}
			break;
		case 's':
			if(EditorMode == MODE_LEVEL) {
				CameraY++; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				TPBlocksYScroll++;
				Redraw = 1;
			}
			break;
		case 'd':
			if(EditorMode == MODE_LEVEL) {
				CameraX++; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				if(LayerInfos[CurLayer].CurrentCategory == LayerInfos[CurLayer].Tileset->CategoryCount + NUM_SPECIAL_CATEGORIES - 1) {
					LayerInfos[CurLayer].CurrentCategory = 0;
				} else {
					LayerInfos[CurLayer].CurrentCategory++;
				}
				TPBlocksYScroll = 0;
				UpdateTPRect();
				Redraw = 1;
			}
			break;
		case 'w':
			if(EditorMode == MODE_LEVEL) {
				CameraY--; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				if(TPBlocksYScroll)
					TPBlocksYScroll--;
				Redraw = 1;
			}
			break;
		case 'A':
			if(EditorMode == MODE_LEVEL) {
				CameraX-=10; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				LayerInfos[CurLayer].CurrentCategory = 0;
				UpdateTPRect();
				Redraw = 1;
			}
			break;
		case 'S':
			if(EditorMode == MODE_LEVEL) {
				CameraY+=10; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				TPBlocksYScroll += 10;
				Redraw = 1;
			}
			break;
		case 'D':
			if(EditorMode == MODE_LEVEL) {
				CameraX+=10; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				LayerInfos[CurLayer].CurrentCategory = NUM_SPECIAL_CATEGORIES + LayerInfos[CurLayer].Tileset->CategoryCount - 1;
				UpdateTPRect();
				Redraw = 1;
			}
			break;
		case 'W':
			if(EditorMode == MODE_LEVEL) {
				CameraY-=10; Redraw = 1;
			} else if(EditorMode == MODE_PICKER) {
				TPBlocksYScroll -= 10;
				if(TPBlocksYScroll < 0)
					TPBlocksYScroll = 0;
				Redraw = 1;
			}
			break;

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

		case 'X':
			Temp = LayerInfos[CurLayer].LayerWidth;
			Temp2 = 0;
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected) {
					if(R->X < Temp)
						Temp = R->X;
					if((R->X + R->W - 1) > Temp2)
						Temp2 = (R->X + R->W - 1);
				}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected) {
					R->X = Temp2-(R->X-Temp)-R->W+1;
				}
			Redraw = 1;
			RerenderMap = 1;
			break;
		case 'Y':
			Temp = LayerInfos[CurLayer].LayerHeight;
			Temp2 = 0;
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected) {
					if(R->Y < Temp)
						Temp = R->Y;
					if((R->Y + R->H - 1) > Temp2)
						Temp2 = (R->Y + R->H - 1);
				}
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected) {
					R->Y = Temp2-(R->Y-Temp)-R->H+1;
				}
			Redraw = 1;
			RerenderMap = 1;
			break;

		case 'O':
			for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next)
				if(R->Selected)
					R->Flips = 0;
			Redraw = 1;
			RerenderMap = 1;
			break;

		case 'f':
			if(EditorMode == MODE_LEVEL) {
				for(LevelRect *R = LayerInfos[CurLayer].Rects; R; R=R->Next) {
					if(R->Selected) {
						*TempText = 0;
						if(R->ExtraInfo) {
							strcpy(TempText, R->ExtraInfo);
							free(R->ExtraInfo);
							R->ExtraInfo = NULL;
						}
						InputLine("Rectangle extra info", TempText, sizeof(TempText));
						if(*TempText)
							R->ExtraInfo = strdup(TempText);
						Redraw = 1;
						break;
					}
				}
			} else if(EditorMode == MODE_PICKER) {
				TPSearchText[0] = 0;
				InputLine("Search for...", TPSearchText, sizeof(TPSearchText));
				UpdateTPRect();
				Redraw = 1;
			}
			break;
	}
}

void DrawSelectionBox(int x, int y, int w, int h, SDL_Color *Color) {
	SDL_Rect Select = {x, y, w, h};
	SDL_SetRenderDrawColor(ScreenRenderer, Color->r, Color->g, Color->b, 255);
	SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_MOD);
	SDL_RenderFillRect(ScreenRenderer, &Select);
	SDL_SetRenderDrawBlendMode(ScreenRenderer, SDL_BLENDMODE_NONE);
	SDL_RenderDrawRect(ScreenRenderer, &Select);
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
		char Temp2[50] = "";
		if(SelectedRect->W + SelectedRect->H != 2)
			sprintf(Temp2, "<%ix%i> ", SelectedRect->W, SelectedRect->H);

		// Check for slope stuff
		TilesetEntry *DrawInfo = &LayerInfos[CurLayer].Tileset->TilesetLookup[SelectedRect->Type];
		if(DrawInfo->Style == STYLE_RIGHT_TRIANGLE) {
			double Ratio = (double)SelectedRect->W / (double)SelectedRect->H;
			sprintf(TempText, "%.3f", Ratio);

			if(floorf(Ratio) == Ratio && Ratio) { // Integer ratio
				if(DrawInfo->Shape.Var & (1 << ((int)Ratio-1)))
					strcat(TempText, " \xE2\x9C\x93 "); // Checkmark
			}
			strcat(Temp2, TempText);
		}

		DrawTilesetEntry(&LayerInfos[CurLayer].Tileset->TilesetLookup[SelectedRect->Type], 5, 5+MainFont.Height*1, SelectedRect->Flips);
		RenderFormatText(ScreenRenderer, &MainFont, 5+TileW+5, 5+MainFont.Height*1, 0, "%s %s%s", LayerInfos[CurLayer].Tileset->TilesetLookup[SelectedRect->Type].Name, Temp2, SelectedRect->ExtraInfo?SelectedRect->ExtraInfo:"");
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
					if(Tile.DrawInfo != NULL) {
						if(Tile.DrawInfo->Style != STYLE_RIGHT_TRIANGLE) { // Regular rectangles
							blitf(Tile.Texture, ScreenRenderer,
								XFromTile(Tile.Tiles[0])*TileW/2,
								YFromTile(Tile.Tiles[0])*TileH/2,
								MapViewX+x*TileW, MapViewY+y*TileH,
								TileW/2, TileH/2, FlipsFromTile(Tile.Tiles[0]));
							blitf(Tile.Texture, ScreenRenderer,
								XFromTile(Tile.Tiles[1])*TileW/2,
								YFromTile(Tile.Tiles[1])*TileH/2,
								MapViewX+x*TileW+TileW/2, MapViewY+y*TileH,
								TileW/2, TileH/2, FlipsFromTile(Tile.Tiles[1]));
							blitf(Tile.Texture, ScreenRenderer,
								XFromTile(Tile.Tiles[2])*TileW/2,
								YFromTile(Tile.Tiles[2])*TileH/2,
								MapViewX+x*TileW, MapViewY+y*TileH+TileH/2,
								TileW/2, TileH/2, FlipsFromTile(Tile.Tiles[2]));
							blitf(Tile.Texture, ScreenRenderer,
								XFromTile(Tile.Tiles[3])*TileW/2,
								YFromTile(Tile.Tiles[3])*TileH/2,
								MapViewX+x*TileW+TileW/2, MapViewY+y*TileH+TileH/2,
								TileW/2, TileH/2, FlipsFromTile(Tile.Tiles[3]));
						} else { // Nonstandard shapes
							if(Tile.DrawInfo->Style == STYLE_RIGHT_TRIANGLE) {
								SDL_Rect Clip = {MapViewX+x*TileW, MapViewY+y*TileH, TileW, TileH};
								SDL_RenderSetClipRect(ScreenRenderer, &Clip);
								SDL_SetRenderDrawColor(ScreenRenderer, Tile.DrawInfo->Shape.R, Tile.DrawInfo->Shape.G, Tile.DrawInfo->Shape.B, 255);

								LevelRect *R = Tile.Rect;
								int RectBaseX = MapViewX + (R->X * TileW) - (CameraX * TileW);
								int RectBaseY = MapViewY + (R->Y * TileH) - (CameraY * TileH);
								int RectEndX = RectBaseX + (R->W * TileW) - 1;
								int RectEndY = RectBaseY + (R->H * TileH) - 1;

								// Diagonal
								if(Tile.Flips == SDL_FLIP_HORIZONTAL || Tile.Flips == SDL_FLIP_VERTICAL)
									SDL_RenderDrawLine(ScreenRenderer, RectBaseX, RectBaseY, RectEndX, RectEndY);
								else
									SDL_RenderDrawLine(ScreenRenderer, RectBaseX, RectEndY, RectEndX, RectBaseY);

								// Horizontal
								if(Tile.Flips & SDL_FLIP_VERTICAL)
									SDL_RenderDrawLine(ScreenRenderer, RectBaseX, RectBaseY, RectEndX, RectBaseY);
								else
									SDL_RenderDrawLine(ScreenRenderer, RectBaseX, RectEndY, RectEndX, RectEndY);

								// Vertical
								if(Tile.Flips & SDL_FLIP_HORIZONTAL)
									SDL_RenderDrawLine(ScreenRenderer, RectBaseX, RectBaseY, RectBaseX, RectEndY);
								else
									SDL_RenderDrawLine(ScreenRenderer, RectEndX, RectBaseY, RectEndX, RectEndY);

								SDL_RenderSetClipRect(ScreenRenderer, NULL);
							}
						}
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
	rect(ScreenRenderer, MapViewX, MapViewY, MapViewWidthP+1, MapViewHeightP+1);

	// Draw the tile picker
	if(EditorMode == MODE_PICKER) {
		SDL_SetRenderDrawColor(ScreenRenderer, 255, 255, 255, 255);
		SDL_RenderFillRect(ScreenRenderer, &TPRect);

		// Cursor
		DrawSelectionBox(TPBlocksRect.x+TPCursorX*(TileW+2), TPBlocksRect.y+TPCursorY*(TileH+2), TileW+2, TileH+2, &SelectColor);

		// Backgrounds behind text inputs
		SDL_SetRenderDrawColor(ScreenRenderer, BGColor.r, BGColor.g, BGColor.b, 255);
		SDL_RenderFillRect(ScreenRenderer, &TPCategoryNameRect);
		SDL_RenderFillRect(ScreenRenderer, &TPBlockNameRect);

		// Draw the outline
		SDL_SetRenderDrawColor(ScreenRenderer, FGColor.r, FGColor.g, FGColor.b, 255);
		int left = TPRect.x;
		int right = TPRect.x+TPRect.w-1;
		SDL_RenderDrawLine(ScreenRenderer, left, TPCategoryNameRect.y+TPCategoryNameRect.h, right, TPCategoryNameRect.y+TPCategoryNameRect.h);
		SDL_RenderDrawLine(ScreenRenderer, left, TPCategoriesRect.y+TPCategoriesRect.h, right, TPCategoriesRect.y+TPCategoriesRect.h);
		SDL_RenderDrawLine(ScreenRenderer, left, TPHotkeysRect.y+TPHotkeysRect.h, right, TPHotkeysRect.y+TPHotkeysRect.h);
		SDL_RenderDrawLine(ScreenRenderer, left, TPBlockNameRect.y+TPBlockNameRect.h, right, TPBlockNameRect.y+TPBlockNameRect.h);
		rect(ScreenRenderer, TPRect.x-1, TPRect.y-1, TPRect.w+2, TPRect.h+2);

		// Display hotkeys
		for(int i=0; i<9; i++) {
			rect(ScreenRenderer, TPHotkeysRect.x+35+i*36, TPHotkeysRect.y, 2, TPHotkeysRect.h);
		}
		for(int i=0; i<10; i++) {
			RenderFormatText(ScreenRenderer, &MainFont, TPHotkeysRect.x+1+i*36, TPHotkeysRect.y+1, SIMPLE_TEXT_ON_WHITE, "%d", (i+1)%10);
			DrawTilesetEntry(&LayerInfos[CurLayer].Tileset->TilesetLookup[LayerInfos[CurLayer].Tileset->HotbarType[i]],
				TPHotkeysRect.x+14+i*36, TPHotkeysRect.y+1, SDL_FLIP_NONE);
		}
		fflush(stdout);
		rect(ScreenRenderer, TPRect.x-1, TPRect.y-1, TPRect.w+2, TPRect.h+2);

		// Display available categories
		int last_category_index = NUM_SPECIAL_CATEGORIES + LayerInfos[CurLayer].Tileset->CategoryCount - 1;
		for(int i=0; i<LayerInfos[CurLayer].Tileset->CategoryCount + NUM_SPECIAL_CATEGORIES; i++) {
			int x = TPCategoriesRect.x + (i%TP_BLOCKS_PER_ROW) * 18 + 1;
			int y = TPCategoriesRect.y + (i/TP_BLOCKS_PER_ROW) * 18 + 1;
			if(i == 0) {
				blit(UITexture, ScreenRenderer, 0, 0, x, y, 16, 16);
			} else if(i == last_category_index) {
				blit(UITexture, ScreenRenderer, 16, 0, x, y, 16, 16);
			} else if(i == last_category_index - 1) {
				blit(UITexture, ScreenRenderer, 32, 0, x, y, 16, 16);
			} else {
				if(LayerInfos[CurLayer].Tileset->CategoryIcons[i-1])
					DrawTilesetEntry(&LayerInfos[CurLayer].Tileset->TilesetLookup[LayerInfos[CurLayer].Tileset->CategoryIcons[i-1]],
					x, y, SDL_FLIP_NONE);
				else
					blit(UITexture, ScreenRenderer, (i%8)*16, 16, x, y, 16, 16);
			}
			if(LayerInfos[CurLayer].CurrentCategory == i)
				DrawSelectionBox(x, y, 16, 16, &SelectColor);
		}

		// Display available blocks
		for(int i=TPBlocksYScroll * TP_BLOCKS_PER_ROW, index_in_picker=0; i<TPBlocksInCategoryCount; i++, index_in_picker++) {
			int x = index_in_picker % TP_BLOCKS_PER_ROW;
			int y = index_in_picker / TP_BLOCKS_PER_ROW;
			DrawTilesetEntry(&LayerInfos[CurLayer].Tileset->TilesetLookup[TPBlocksInCategory[i]], TPBlocksRect.x+x*(TileW+2)+1, TPBlocksRect.y+y*(TileH+2)+1, SDL_FLIP_NONE);
		}

		const char *category_name = "";
		if(LayerInfos[CurLayer].CurrentCategory == 0) {
			category_name = "Everything";
		} else if(LayerInfos[CurLayer].CurrentCategory == last_category_index) {
			category_name = "Recent tiles";
		} else if(LayerInfos[CurLayer].CurrentCategory == last_category_index - 1) {
			category_name = "Starred tiles";
		} else {
			category_name = LayerInfos[CurLayer].Tileset->Categories[LayerInfos[CurLayer].CurrentCategory - 1];
		}
		if(TPSearchText[0]) {
			sprintf(TempText, "%s (%s)", category_name, TPSearchText);
			category_name = TempText;
		}
		RenderSimpleText(ScreenRenderer, &MainFont, TPRect.x, TPCategoryNameRect.y+2, 0, category_name);

		int hovered_block = (TPCursorY+TPBlocksYScroll) * TP_BLOCKS_PER_ROW + TPCursorX;
		if(hovered_block >= 0 && hovered_block < TPBlocksInCategoryCount)
			RenderSimpleText(ScreenRenderer, &MainFont, TPRect.x, TPBlockNameRect.y+2, 0, LayerInfos[CurLayer].Tileset->TilesetLookup[TPBlocksInCategory[hovered_block]].Name);
	}

	SDL_RenderPresent(ScreenRenderer);
}

void run_gui(const char *filename) {
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
		goto exit;
	if (!Load_FontSet(&TinyFont, 10, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
		goto exit;
	if (!Load_FontSet(&VeryTinyFont, 8, "data/font/font.ttf", "data/font/fontb.ttf", "data/font/fonti.ttf", "data/font/fontbi.ttf"))
		goto exit;
	UITexture = LoadTexture("data/ui.png", 0);

	// .-----------------------------------------------------------------------
	// | LOAD THE LEVEL
	// '-----------------------------------------------------------------------

	FILE *Test = fopen(filename, "rb");
	if(Test) {
		fclose(Test);
		if(!LoadLevel(filename)) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't load %s", filename);
			return;
		}
	} else {
		char Temp[250];
		sprintf(Temp, "%sdata/default.json", BasePath);
		if(!LoadLevel(Temp)) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't load default level");
			return;
		}
		strlcpy(LevelFilename, FilenameOnly(filename), sizeof(LevelFilename));
		strlcpy(LevelFilenameFull, filename, sizeof(LevelFilenameFull));
	}

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
exit:
	for(int i=0;i<SYSCURSOR_MAX;i++)
		SDL_FreeCursor(SystemCursors[i]);
	Free_FontSet(&MainFont);
	Free_FontSet(&TinyFont);
	Free_FontSet(&VeryTinyFont);
	if(UITexture)
		SDL_DestroyTexture(UITexture);
}
