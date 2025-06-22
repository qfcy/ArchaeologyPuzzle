@echo off
rem 需要使用build -mwindows，去掉控制台窗口
windres game.rc -o bin/game_res.o
g++ libs/stb_image/stbimage_lib.cpp -o bin/stb_image.dll -shared -fPIC -O2 -s -Wl,--out-implib,lib/stb_image.a -Iinclude
g++ game.cpp bin/game_res.o -o bin/ArchaeologyPuzzle -s -O2 -Wall -Wno-reorder -Iinclude -Llib -lfreeglut -lopengl32 -lglu32 -lwinmm -l:stb_image.a %*