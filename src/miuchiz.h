#ifndef MIUCHIZ_HEADER
#define MIUCHIZ_HEADER
#define NO_STDIO_REDIRECT
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <sys/types.h>
#include <sys/stat.h>
#endif

#define MIUCHIZ_WIDTH 98
#define MIUCHIZ_HEIGHT 67

struct cpu_state {
  uint8_t a;
  uint8_t x;
  uint8_t y;
  uint8_t s;
  uint8_t flags;
  uint16_t pc;
  int waiting;
  int cycles;
  void *hardware;
  void (*write)(void*, uint16_t, uint8_t);
  uint8_t (*read)(void*, uint16_t);
};


extern int ScreenWidth, ScreenHeight, ScreenZoom;
extern SDL_Window *window;
extern SDL_Renderer *ScreenRenderer;
extern int retraces;

void SDL_MessageBox(int Type, const char *Title, SDL_Window *Window, const char *fmt, ...);
void strlcpy(char *Destination, const char *Source, int MaxLength);
SDL_Surface *SDL_LoadImage(const char *FileName, int Flags);
SDL_Texture *LoadTexture(const char *FileName, int Flags);
void rectfill(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2);
void rect(SDL_Renderer *Bmp, int X1, int Y1, int X2, int Y2);
void sblit(SDL_Surface* SrcBmp, SDL_Surface* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blit(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height);
void blitf(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, SDL_RendererFlip Flip);
void blitz(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int SourceX, int SourceY, int DestX, int DestY, int Width, int Height, int Width2, int Height2);
void blitfull(SDL_Texture* SrcBmp, SDL_Renderer* DstBmp, int DestX, int DestY);
