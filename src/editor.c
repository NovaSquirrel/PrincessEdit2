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

void run_gui();
char *PrefPath = NULL;
char *BasePath = NULL;

int main(int argc, char *argv[]) {
	if(argc != 2) {
		 SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Open what level?");
		 return 0;
	}

	if(SDL_Init(SDL_INIT_VIDEO) < 0){
		printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
		return 0;
	}
	SDL_SetHint("SDL_HINT_VIDEO_ALLOW_SCREENSAVER", "1");

	PrefPath = SDL_GetPrefPath("Bushytail Software","PrincessEdit2");
	BasePath = SDL_GetBasePath();
	if(!BasePath)
		BasePath = SDL_strdup("./");

	FILE *Test = fopen(argv[1], "rb");
	if(Test) {
		fclose(Test);
		if(!LoadLevel(argv[1])) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't load %s", argv[1]);
			return 0;
		}
	} else {
		char Temp[250];
		sprintf(Temp, "%sdata/default.json", BasePath);
		if(!LoadLevel(Temp)) {
			SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "Can't load default level");
			return 0;
		}
		strlcpy(LevelFilename, FilenameOnly(argv[1]), sizeof(LevelFilename));
		strlcpy(LevelFilenameFull, argv[1], sizeof(LevelFilenameFull));
	}

	run_gui();

	if(PrefPath) SDL_free(PrefPath);
	if(BasePath) SDL_free(BasePath);
	SDL_Quit();

	return 0;
}
