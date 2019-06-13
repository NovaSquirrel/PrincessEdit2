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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "editor.h"

int ScreenWidth, ScreenHeight;
int quit = 0;

SDL_Window *window = NULL;
SDL_Renderer *ScreenRenderer = NULL;
int retraces = 0;

void run_gui() {
	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return;
	}

	ScreenWidth = 640;
	ScreenHeight = 480;

	window = SDL_CreateWindow("Princess Edit 2", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ScreenWidth, ScreenHeight, SDL_WINDOW_SHOWN);
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
	// ------------------------------------------------------

	SDL_Event e;
	while(!quit) {
		while(SDL_PollEvent(&e) != 0) {
			if(e.type == SDL_QUIT)
				quit = 1;
		}

        SDL_SetRenderDrawColor(ScreenRenderer, 255, 255, 255, 255);
        SDL_RenderClear(ScreenRenderer);
        SDL_SetRenderDrawColor(ScreenRenderer, 0, 255, 0, 255);
		rectfill(ScreenRenderer, 10, 10, 50, 50);

		SDL_RenderPresent(ScreenRenderer);

		SDL_Delay(17);
		retraces++;
	}
	SDL_Quit();
}