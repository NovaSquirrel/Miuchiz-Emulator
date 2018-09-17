#include "miuchiz.h"
#include <math.h>
int ScreenWidth, ScreenHeight, ScreenZoom = 4;

SDL_Window *window = NULL;
SDL_Renderer *ScreenRenderer = NULL;
int quit = 0;
int retraces = 0;

struct miuchiz_hardware {
  uint8_t ram[0x8000]; // 32KB
  uint16_t BRR; // bios bank
  uint16_t PRR; // program bank
  uint16_t DRR; // data bank
  int cursor_x;
  int cursor_y;
  int cursor_odd;
  uint8_t flash[1024 * 1024 * 2];
  uint8_t otp[0x4000];

  uint8_t read_value; // last read value, for open bus
  uint8_t pixel_buffer;
  uint16_t pixels[MIUCHIZ_WIDTH][MIUCHIZ_HEIGHT];
};

uint8_t video_read(struct miuchiz_hardware *hw, uint16_t address) {
  if(address & 1) { // data
    return 0xff;
  } else {          // control
    return 0xca;
  }
}

void video_write(struct miuchiz_hardware *hw, uint16_t address, uint8_t value) {
  if(address & 1) { // data
    // write a pixel
    if((hw->cursor_odd & 1) == 0) {
      hw->pixel_buffer = value;
    } else {
      hw->pixels[hw->cursor_x][hw->cursor_y] = (hw->pixel_buffer << 8) | value;
      hw->cursor_x++;
      if(hw->cursor_x >= MIUCHIZ_WIDTH) {
        hw->cursor_x = 0;
        hw->cursor_y++;
      }
      if(hw->cursor_y >= MIUCHIZ_HEIGHT) {
        hw->cursor_y = 0;
      }
    }

    hw->cursor_odd ^= 1;
  } else {          // control
    // do nothing
  }
}

uint8_t read_handler(void *h, uint16_t address) {
//  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "reading address %.4x", address);
  struct miuchiz_hardware *hw = h;

  // fixed bank of RAM
  if(address >= 0x0080 && address <= 0x1fff) {
    hw->read_value = hw->ram[address];
    return hw->read_value;
  }

  int bank = 0, is_ram = 0, offset = 0;
  if(address >= 0x2000 && address <= 0x3fff) {
    bank = hw->BRR;
    is_ram = hw->BRR & 0x8000;
    offset = address & 0x1fff;
  }
  else if(address >= 0x4000 && address <= 0x7fff) {
    bank = (hw->PRR << 1) & 0x7fff;
    is_ram = hw->PRR & 0x8000;
    offset = address & 0x3fff;
  }
  else if(address >= 0x8000 && address <= 0xffff) {
    bank = (hw->DRR << 2) & 0x7fff;
    is_ram = hw->DRR & 0x8000;
    offset = address & 0x7fff;
  }

  // Handle the external read
  if(is_ram) {
    hw->read_value = hw->ram[address & 0x7fff];
  } else {
    if(((bank & 0x9e00) == 0x0000) || ((bank & 0x9e00) == 0x1e00)) {
    // OTP
      hw->read_value = hw->otp[((bank&1)*8192+offset) & 0x3fff];

    } else if((bank & 0x9f00) == 0x0300) {
    // video
      hw->read_value = video_read(hw, address);

    } else if((bank & 0x9c00) == 0x0400) {
    // flash
      int flash_address = ((bank&0xff)*8192+offset) & 0x1fffff;
      hw->read_value = hw->flash[flash_address];
    }
  }


  return hw->read_value;
}

void write_handler(void *h, uint16_t address, uint8_t value) {
///  SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "writing %.2x to %.4x", value, address);
  struct miuchiz_hardware *hw = h;

  // fixed bank of RAM
  if(address >= 0x0080 && address <= 0x1fff) {
    hw->ram[address] = value;
    return;
  }

  int bank = 0, is_ram = 0;
  if(address >= 0x2000 && address <= 0x3fff) {
    bank = hw->BRR;
    is_ram = hw->BRR & 0x8000;
  }
  else if(address >= 0x4000 && address <= 0x7fff) {
    bank = (hw->PRR << 1) & 0x7fff;
    is_ram = hw->PRR & 0x8000;
  }
  else if(address >= 0x8000 && address <= 0xffff) {
    bank = (hw->DRR << 2) & 0x7fff;
    is_ram = hw->DRR & 0x8000;
  }

  // Handle the external write
  if(is_ram) {
    hw->ram[address & 0x7fff] = value;
  } else {
    if(((bank & 0x9e00) == 0x0000) || ((bank & 0x9e00) == 0x1e00)) {
    // OTP

    } else if((bank & 0x9f00) == 0x0300) {
    // video
      video_write(hw, address, value);
    } else if((bank & 0x9c00) == 0x0400) {
    // flash

    }
  }

}

void update_screen(struct miuchiz_hardware *hw) {
  for(int x = 0; x < MIUCHIZ_WIDTH; x++) {
    for(int y = 0; y < MIUCHIZ_HEIGHT; y++) {
       uint16_t pixel = hw->pixels[x][y];
       int r = (pixel >> 8) & 0xf;
       int g = (pixel >> 4) & 0xf;
       int b = (pixel >> 0) & 0xf;
       // extend 5 into 55 and so on
       SDL_SetRenderDrawColor(ScreenRenderer, r|(r<<4), g|(g<<4), b|(b<<4), 255);
       SDL_Rect rect = {x*ScreenZoom, y*ScreenZoom, ScreenZoom, ScreenZoom};
       SDL_RenderFillRect(ScreenRenderer, &rect);
    }
  }
}

struct cpu_state cpu;
struct miuchiz_hardware hw;
void run_instruction(struct cpu_state *s);

int main(int argc, char *argv[]) {
  // Initialize the hardware
  memset(&cpu, 0, sizeof(cpu));
  memset(&hw, 0, sizeof(hw));
  cpu.hardware = &hw;
  cpu.read = read_handler;
  cpu.write = write_handler;
  hw.PRR = 0x7202;
  hw.BRR = 0xe000;
  hw.DRR = 0x78c0;
  cpu.pc = 0x4000;
  cpu.s = 0xff;

  // read OTP
  FILE *file = fopen("data/otp.dat", "rb");
  if(file == NULL) {
    puts("Can't open OTP");
    return -1;
  }
  fread(hw.otp, 1, sizeof(hw.otp), file);
  fclose(file);

  // read flash
  file = fopen("data/flash.dat", "rb");
  if(file == NULL) {
    puts("Can't open flash");
    return -1;
  }
  fread(hw.flash, 1, sizeof(hw.flash), file);
  fclose(file);
  // ------------------------------------------------------

  if(SDL_Init(SDL_INIT_VIDEO) < 0){
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    return -1;
  }
  ScreenWidth = MIUCHIZ_WIDTH * ScreenZoom;
  ScreenHeight = MIUCHIZ_HEIGHT * ScreenZoom;
  window = SDL_CreateWindow("Miuchiz emulator?", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ScreenWidth, ScreenHeight, SDL_WINDOW_SHOWN);
  if(!window) {
     SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "Window could not be created! SDL_Error: %s", SDL_GetError());
     return -1;
  }
  if(!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)){
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_image could not initialize! SDL_image Error: %s", IMG_GetError());
    return -1;
  }
  if( TTF_Init() == -1 ) {
    SDL_MessageBox(SDL_MESSAGEBOX_ERROR, "Error", NULL, "SDL_ttf could not initialize! SDL_ttf Error: %s", TTF_GetError());
    return -1;
  }
  ScreenRenderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  // ------------------------------------------------------

  SDL_Event e;
  while(!quit) {
    while(SDL_PollEvent(&e) != 0) {
      if(e.type == SDL_QUIT)
        quit = 1;
    }

    for(int i=0; i<1000; i++)
      run_instruction(&cpu);

    update_screen(&hw);
    SDL_RenderPresent(ScreenRenderer);

    SDL_Delay(17);
    retraces++;
  }
  SDL_Quit();

  return 0;
}
