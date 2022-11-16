#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/select.h>
#include <linux/input.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>
#include <poll.h>
#include <stdint.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>

// file descriptors for joystick and frame buffer(screen)
int fd_joystick,fd_framebuffer; 
// frame buffer pointer
uint16_t *fb_ptr;  
// struct for getting specs of frame buffer for the screen
struct fb_fix_screeninfo fb_fs_info; 
// struct for joystick(peripheral) controls
struct input_event i_event;                                        

// Array of colours to colour the screen
// Some somewhat distinct colours
uint16_t colours [] = {        
    0xFFE0, 
    0x7D5F, 
    0x7ABF,
    0xFFFF, 
    0xF81F, 
    0x001F, 
    0x000D, 
    0x07E0, 
    0x02A0, 
    0x7800, 
    0x5950,
    0x2550,
    0x9cc3,
    0x14B1,
    0xF3FC,
    0x8D30,
    0x3A50
  };
  
int i = 0;
// Function to change the colour of new tile
uint16_t tile_colour (){
  i++;
  if (i==17){
    i = 0;
 }
  return colours[i];
}

// The game state can be used to detect what happens on the playfield
#define GAMEOVER   0
#define ACTIVE     (1 << 0)
#define ROW_CLEAR  (1 << 1)
#define TILE_ADDED (1 << 2)

// If you extend this structure, either avoid pointers or adjust
// the game logic allocate/deallocate and reset the memory
typedef struct {
  bool occupied;
  uint16_t colour;
} tile;

typedef struct {
  unsigned int x;
  unsigned int y;
} coord;

typedef struct {
  coord const grid;                     // playfield bounds
  unsigned long const uSecTickTime;     // tick rate
  unsigned long const rowsPerLevel;     // speed up after clearing rows
  unsigned long const initNextGameTick; // initial value of nextGameTick

  unsigned int tiles; // number of tiles played
  unsigned int rows;  // number of rows cleared
  unsigned int score; // game score
  unsigned int level; // game level

  tile *rawPlayfield; // pointer to raw memory of the playfield
  tile **playfield;   // This is the play field array
  unsigned int state;
  coord activeTile;                       // current tile

  unsigned long tick;         // incremeted at tickrate, wraps at nextGameTick
                              // when reached 0, next game state calculated
  unsigned long nextGameTick; // sets when tick is wrapping back to zero
                              // lowers with increasing level, never reaches 0
} gameConfig;


gameConfig game = {
                   .grid = {8, 8},
                   .uSecTickTime = 10000,
                   .rowsPerLevel = 2,
                    .initNextGameTick = 50,
};

//Filter for entries starting with with "/dev/fb"
int (directory_filter_fb) (const struct dirent *entry){
  return strcmp(entry -> d_name, "/dev/fb") >= 0;
}
//Filter for entries starting with with "/dev/input/event"
int (directory_filter_event) (const struct dirent *entry){
  return strcmp(entry -> d_name, "/dev/input/event") >= 0;
}

//Function to get the file descriptor and appropriate screen(file) required
bool get_framebuffer () {
  
  int fb_count;
  struct dirent **namelist; //Memory allocated(array) for entries
  const char* dirp = "/dev/"; // Directory to look up
  // Get the number of files in the "/dev" directory with path name '/fb'
  fb_count = scandir(dirp, &namelist, &directory_filter_fb, alphasort); 
  
  for (int i = 0; i < fb_count; i++){
    //Store each path name from the sorted directory 'dirp' for each entry
    char path[256];                                             
		snprintf(path, sizeof(path), "%s%s", dirp, namelist[i]->d_name);    
    //Get the file descriptor for the file
		fd_framebuffer = open(path, O_RDWR); 
    //Check that the name of the file is "RPi-Sense FB"
    // and free memory allocated to entry (namelist)
    if ((ioctl(fd_framebuffer, FBIOGET_FSCREENINFO, &fb_fs_info) >= 0)  
        && (strcmp(fb_fs_info.id, "RPi-Sense FB") == 0)){
        free(namelist[i]);
        return true;
        }
    free(namelist[i]);    
  }
  free(namelist);                                                   
  fprintf(stderr,"ERROR: could not find 'RPi-Sense FB' in directory\n"); 
  return false;
}

//Function to get the file descriptor and appropriate joystick/control peripheral(file) required
bool find_joystick () {
  
  int event_count;
  struct dirent **namelist; //Memory allocated(array) for entries
  const char* dirp = "/dev/input/"; // Directory to look up
  // Get the number of files in the "/dev/input" directory with path name 'event'
  event_count = scandir(dirp, &namelist, &directory_filter_event, alphasort); 
  
  for (int i = 0; i < event_count; i++){
    //Store each path name from the sorted directory 'dirp' for each entry
    char path[256];                                              
		snprintf(path, sizeof(path), "%s%s", dirp, namelist[i]->d_name);   
    //Get the file descriptor for the file
		fd_joystick = open(path, O_RDWR); 
    //Check that the name of the file is "Raspberry Pi Sense HAT Joystick"
    // and free memory allocated to entry (namelist)
    char js[256];
    if ((ioctl(fd_joystick, EVIOCGNAME(sizeof(js)), &js) >= 0)   
        && (strcmp(js, "Raspberry Pi Sense HAT Joystick") == 0)){    
        free(namelist[i]);
        return true;
        }
    free(namelist[i]);    
  }
  free(namelist);                                                 
  fprintf(stderr,"ERROR: could not find 'Raspberry Pi Sense HAT Joystick' in directory\n"); 
  return false;
}

// This function is called on the start of your application
// Here you can initialize what ever you need for your task
// return false if something fails, else true
bool initializeSenseHat() {
  //Exit with error if joystick is not found
  if (!find_joystick ()){
  return false;
  }
  //Map to memory the framebuffer device(file)
  if (get_framebuffer ()){
  fb_ptr = mmap(NULL, 64*sizeof(uint16_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_framebuffer, 0);
  return true;
  }
   
  return false;  
}

// This function is called when the application exits
// Here you can free up everything that you might have opened/allocated
void freeSenseHat() {
  memset(fb_ptr,0x0000,64*sizeof(uint16_t)); //Turn off all LEDs before freeing up memory allocated to fb
  munmap(fb_ptr,64*sizeof(uint16_t));// Delete/ Unmap fb from memory
  close(fd_framebuffer); // Close the frame buffer device file descriptor
  close(fd_joystick); // Close the joystick device file descriptor
}

// This function should return the key that corresponds to the joystick press
// KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, with the respective direction
// and KEY_ENTER, when the the joystick is pressed
// !!! when nothing was pressed you MUST return 0 !!!
int readSenseHatJoystick() {
  //Set the file descriptor flag of the joystick to nonblock
  //to ensure game does not 'pause' and tiles behave as expected
  fcntl(fd_joystick, F_SETFL, O_NONBLOCK);
  //Get the entries from the joystick device
  read(fd_joystick, &i_event, sizeof(i_event));
  //From the joystick peripheral device get the events(keys) been pressed
  //Allow continuous events when keys are repeatedly pressed (return[emit] same event)
  if((i_event.type = EV_KEY) &&
   (i_event.value == 1 || i_event.value == 2)){
      return i_event.code;
    }
  return 0;
}


// This function should render the gamefield on the LED matrix. It is called
// every game tick. The parameter playfieldChanged signals whether the game logic
// has changed the playfield
void renderSenseHatMatrix(bool const playfieldChanged) {
  if (!playfieldChanged) {
  return; 
  }
  //Search through memory and assign tile to same coordinate(pixel) as screen
  //The active tile should continue to have assigned colour
  for(int x =0;x < game.grid.x; x++){
    for(int y =0; y <game.grid.y; y++){
      fb_ptr[x + (y * game.grid.y)] = game.playfield[y][x].colour;
  }
}
}



// The game logic uses only the following functions to interact with the playfield.
// if you choose to change the playfield or the tile structure, you might need to
// adjust this game logic <> playfield interface

static inline void newTile(coord const target) {
  game.playfield[target.y][target.x].occupied = true;
  //Assign new colour to new tile
  game.playfield[target.y][target.x].colour = tile_colour();
}

static inline void copyTile(coord const to, coord const from) {
  memcpy((void *) &game.playfield[to.y][to.x], (void *) &game.playfield[from.y][from.x], sizeof(tile));
}

static inline void copyRow(unsigned int const to, unsigned int const from) {
  memcpy((void *) &game.playfield[to][0], (void *) &game.playfield[from][0], sizeof(tile) * game.grid.x);

}

static inline void resetTile(coord const target) {
  memset((void *) &game.playfield[target.y][target.x], 0, sizeof(tile));
}

static inline void resetRow(unsigned int const target) {
  memset((void *) &game.playfield[target][0], 0, sizeof(tile) * game.grid.x);
}

static inline bool tileOccupied(coord const target) {
  return game.playfield[target.y][target.x].occupied;
}

static inline bool rowOccupied(unsigned int const target) {
  for (unsigned int x = 0; x < game.grid.x; x++) {
    coord const checkTile = {x, target};
    if (!tileOccupied(checkTile)) {
      return false;
    }
  }
  return true;
}


static inline void resetPlayfield() {
  for (unsigned int y = 0; y < game.grid.y; y++) {
    resetRow(y);
  }
}

// Below here comes the game logic. Keep in mind: You are not allowed to change how the game works!
// that means no changes are necessary below this line! And if you choose to change something
// keep it compatible with what was provided to you!

bool addNewTile() {
  game.activeTile.y = 0;
  game.activeTile.x = (game.grid.x - 1) / 2;
  if (tileOccupied(game.activeTile))
    return false;
  newTile(game.activeTile);
  return true;
}

bool moveRight() {
  coord const newTile = {game.activeTile.x + 1, game.activeTile.y};
  if (game.activeTile.x < (game.grid.x - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}

bool moveLeft() {
  coord const newTile = {game.activeTile.x - 1, game.activeTile.y};
  if (game.activeTile.x > 0 && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool moveDown() {
  coord const newTile = {game.activeTile.x, game.activeTile.y + 1};
  if (game.activeTile.y < (game.grid.y - 1) && !tileOccupied(newTile)) {
    copyTile(newTile, game.activeTile);
    resetTile(game.activeTile);
    game.activeTile = newTile;
    return true;
  }
  return false;
}


bool clearRow() {
  if (rowOccupied(game.grid.y - 1)) {
    for (unsigned int y = game.grid.y - 1; y > 0; y--) {
      copyRow(y, y - 1);
    }
    resetRow(0);
    return true;
  }
  return false;
}

void advanceLevel() {
  game.level++;
  switch(game.nextGameTick) {
  case 1:
    break;
  case 2 ... 10:
    game.nextGameTick--;
    break;
  case 11 ... 20:
    game.nextGameTick -= 2;
    break;
  default:
    game.nextGameTick -= 10;
  }
}

void newGame() {
  game.state = ACTIVE;
  game.tiles = 0;
  game.rows = 0;
  game.score = 0;
  game.tick = 0;
  game.level = 0;
  resetPlayfield();
}

void gameOver() {
  game.state = GAMEOVER;
  game.nextGameTick = game.initNextGameTick;
}


bool sTetris(int const key) {
  bool playfieldChanged = false;

  if (game.state & ACTIVE) {
    // Move the current tile
    if (key) {
      playfieldChanged = true;
      switch(key) {
      case KEY_LEFT:
        moveLeft();
        break;
      case KEY_RIGHT:
        moveRight();
        break;
      case KEY_DOWN:
        while (moveDown()) {};
        game.tick = 0;
        break;
      default:
        playfieldChanged = false;
      }
    }

    // If we have reached a tick to update the game
    if (game.tick == 0) {
      // We communicate the row clear and tile add over the game state
      // clear these bits if they were set before
      game.state &= ~(ROW_CLEAR | TILE_ADDED);

      playfieldChanged = true;
      // Clear row if possible
      if (clearRow()) {
        game.state |= ROW_CLEAR;
        game.rows++;
        game.score += game.level + 1;
        if ((game.rows % game.rowsPerLevel) == 0) {
          advanceLevel();
        }
      }

      // if there is no current tile or we cannot move it down,
      // add a new one. If not possible, game over.
      if (!tileOccupied(game.activeTile) || !moveDown()) {
        if (addNewTile()) {
          game.state |= TILE_ADDED;
          game.tiles++;
        } else {
          gameOver();
        }
      }
    }
  }

  // Press any key to start a new game
  if ((game.state == GAMEOVER) && key) {
    playfieldChanged = true;
    newGame();
    addNewTile();
    game.state |= TILE_ADDED;
    game.tiles++;
  }

  return playfieldChanged;
}

int readKeyboard() {
  struct pollfd pollStdin = {
       .fd = STDIN_FILENO,
       .events = POLLIN
  };
  int lkey = 0;

  if (poll(&pollStdin, 1, 0)) {
    lkey = fgetc(stdin);
    if (lkey != 27)
      goto exit;
    lkey = fgetc(stdin);
    if (lkey != 91)
      goto exit;
    lkey = fgetc(stdin);
  }
 exit:
    switch (lkey) {
      case 10: return KEY_ENTER;
      case 65: return KEY_UP;
      case 66: return KEY_DOWN;
      case 67: return KEY_RIGHT;
      case 68: return KEY_LEFT;
    }
  return 0;
}

void renderConsole(bool const playfieldChanged) {
  if (!playfieldChanged)
    return;

  // Goto beginning of console
  fprintf(stdout, "\033[%d;%dH", 0, 0);
  for (unsigned int x = 0; x < game.grid.x + 2; x ++) {
    fprintf(stdout, "-");
  }
  fprintf(stdout, "\n");
  for (unsigned int y = 0; y < game.grid.y; y++) {
    fprintf(stdout, "|");
    for (unsigned int x = 0; x < game.grid.x; x++) {
      coord const checkTile = {x, y};
      fprintf(stdout, "%c", (tileOccupied(checkTile)) ? '#' : ' ');
    }
    switch (y) {
      case 0:
        fprintf(stdout, "| Tiles: %10u\n", game.tiles);
        break;
      case 1:
        fprintf(stdout, "| Rows:  %10u\n", game.rows);
        break;
      case 2:
        fprintf(stdout, "| Score: %10u\n", game.score);
        break;
      case 4:
        fprintf(stdout, "| Level: %10u\n", game.level);
        break;
      case 7:
        fprintf(stdout, "| %17s\n", (game.state == GAMEOVER) ? "Game Over" : "");
        break;
    default:
        fprintf(stdout, "|\n");
    }
  }
  for (unsigned int x = 0; x < game.grid.x + 2; x++) {
    fprintf(stdout, "-");
  }
  fflush(stdout);
}


inline unsigned long uSecFromTimespec(struct timespec const ts) {
  return ((ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
}

int main(int argc, char **argv) {
  (void) argc;
  (void) argv;
  // This sets the stdin in a special state where each
  // keyboard press is directly flushed to the stdin and additionally
  // not outputted to the stdout
  {
    struct termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);
    ttystate.c_lflag &= ~(ICANON | ECHO);
    ttystate.c_cc[VMIN] = 1;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);
  }

  // Allocate the playing field structure
  game.rawPlayfield = (tile *) malloc(game.grid.x * game.grid.y * sizeof(tile));
  game.playfield = (tile**) malloc(game.grid.y * sizeof(tile *));
  if (!game.playfield || !game.rawPlayfield) {
    fprintf(stderr, "ERROR: could not allocate playfield\n");
    return 1;
  }
  for (unsigned int y = 0; y < game.grid.y; y++) {
    game.playfield[y] = &(game.rawPlayfield[y * game.grid.x]);
  }

  // Reset playfield to make it empty
  resetPlayfield();
  // Start with gameOver
  gameOver();

  if (!initializeSenseHat()) {
    fprintf(stderr, "ERROR: could not initilize sense hat\n");
    return 1;
  };

  // Clear console, render first time
  fprintf(stdout, "\033[H\033[J");
  renderConsole(true);
  renderSenseHatMatrix(true);

  while (true) {
    struct timeval sTv, eTv;
    gettimeofday(&sTv, NULL);

    int key = readSenseHatJoystick();
    if (!key)
      key = readKeyboard();
    if (key == KEY_ENTER)
      break;

    bool playfieldChanged = sTetris(key);
    renderConsole(playfieldChanged);
    renderSenseHatMatrix(playfieldChanged);

    // Wait for next tick
    gettimeofday(&eTv, NULL);
    unsigned long const uSecProcessTime = ((eTv.tv_sec * 1000000) + eTv.tv_usec) - ((sTv.tv_sec * 1000000 + sTv.tv_usec));
    if (uSecProcessTime < game.uSecTickTime) {
      usleep(game.uSecTickTime - uSecProcessTime);
    }
    game.tick = (game.tick + 1) % game.nextGameTick;
  }

  freeSenseHat();
  free(game.playfield);
  free(game.rawPlayfield);

  return 0;
}
