/*
 * castle_mania.c
 * program which demonstraes tile mode 0
 */

/* include the image we are using */
#include "castle.h"
/* include the tile map we are using */
#include "wall.h"
#include "mania_sprites.h"

/* the width and height of the screen */
#define WIDTH 240
#define HEIGHT 160

/* the three tile modes */
#define MODE0 0x00
#define MODE1 0x01
#define MODE2 0x02

/* enable bits for the four tile layers */
#define BG0_ENABLE 0x100
#define BG1_ENABLE 0x200
#define BG2_ENABLE 0x400
#define BG3_ENABLE 0x800

/* flags to set sprite handling in display control register */
#define SPRITE_MAP_2D 0x0
#define SPRITE_MAP_1D 0x40
#define SPRITE_ENABLE 0x1000

/* the control registers for the four tile layers */
volatile unsigned short* bg0_control = (volatile unsigned short*) 0x4000008;
volatile unsigned short* bg1_control = (volatile unsigned short*) 0x400000a;
volatile unsigned short* bg2_control = (volatile unsigned short*) 0x400000c;
volatile unsigned short* bg3_control = (volatile unsigned short*) 0x400000e;

/* palette is always 256 colors */
#define PALETTE_SIZE 256

/* the memory location which controls sprite attributes */
volatile unsigned short* sprite_attribute_memory = (volatile unsigned short*) 0x7000000;

/* the display control pointer points to the gba graphics register */
volatile unsigned long* display_control = (volatile unsigned long*) 0x4000000;

/* the address of the color palette */
volatile unsigned short* bg_palette = (volatile unsigned short*) 0x5000000;

/* the button register holds the bits which indicate whether each button has
 * been pressed - this has got to be volatile as well
 */
volatile unsigned short* buttons = (volatile unsigned short*) 0x04000130;

/* scrolling registers for backgrounds */
volatile short* bg0_x_scroll = (unsigned short*) 0x4000010;
volatile short* bg0_y_scroll = (unsigned short*) 0x4000012;
volatile short* bg1_x_scroll = (unsigned short*) 0x4000014;
volatile short* bg1_y_scroll = (unsigned short*) 0x4000016;
volatile short* bg2_x_scroll = (unsigned short*) 0x4000018;
volatile short* bg2_y_scroll = (unsigned short*) 0x400001a;
volatile short* bg3_x_scroll = (unsigned short*) 0x400001c;
volatile short* bg3_y_scroll = (unsigned short*) 0x400001e;


/* the bit positions indicate each button - the first bit is for A, second for
 * B, and so on, each constant below can be ANDED into the register to get the
 * status of any one button */
#define BUTTON_A (1 << 0)
#define BUTTON_B (1 << 1)
#define BUTTON_SELECT (1 << 2)
#define BUTTON_START (1 << 3)
#define BUTTON_RIGHT (1 << 4)
#define BUTTON_LEFT (1 << 5)
#define BUTTON_UP (1 << 6)
#define BUTTON_DOWN (1 << 7)
#define BUTTON_R (1 << 8)
#define BUTTON_L (1 << 9)
#define NUM_SPRITES 128

/* a sprite is a moveable image on the screen */

/* flag for turning on DMA */
#define DMA_ENABLE 0x80000000

/* flags for the sizes to transfer, 16 or 32 bits */
#define DMA_16 0x00000000
#define DMA_32 0x04000000

/* pointer to the DMA source location */
volatile unsigned int* dma_source = (volatile unsigned int*) 0x40000D4;

/* pointer to the DMA destination location */
volatile unsigned int* dma_destination = (volatile unsigned int*) 0x40000D8;

/* pointer to the DMA count/control */
volatile unsigned int* dma_count = (volatile unsigned int*) 0x40000DC;

/* copy data using DMA */
void memcpy16_dma(unsigned short* dest, unsigned short* source, int amount) {
    *dma_source = (unsigned int) source;
    *dma_destination = (unsigned int) dest;
    *dma_count = amount | DMA_16 | DMA_ENABLE;
}

struct Sprite {
    unsigned short attribute0;
    unsigned short attribute1;
    unsigned short attribute2;
    unsigned short attribute3;
};

struct Sprite sprites[NUM_SPRITES];
int next_sprite_index = 0;

/* the different sizes of sprites which are possible */
enum SpriteSize {
    SIZE_8_8,
    SIZE_16_16,
    SIZE_32_32,
    SIZE_64_64,
    SIZE_16_8,
    SIZE_32_8,
    SIZE_32_16,
    SIZE_64_32,
    SIZE_8_16,
    SIZE_8_32,
    SIZE_16_32,
    SIZE_32_64
};

volatile unsigned short* sprite_palette = (volatile unsigned short*) 0x5000200;

/* the memory location which stores sprite image data */
volatile unsigned short* sprite_image_memory = (volatile unsigned short*) 0x6010000;

/* setup the sprite image and palette */
void setup_sprite_image() {
    /* load the palette from the image into palette memory*/
    memcpy16_dma((unsigned short*) sprite_palette, (unsigned short*) mania_sprites_palette, PALETTE_SIZE);

    /* load the image into sprite image memory */
    memcpy16_dma((unsigned short*) sprite_image_memory, (unsigned short*) mania_sprites_data, (mania_sprites_width * mania_sprites_height) / 2);

}

/* function to initialize a sprite with its properties, and return a pointer */
struct Sprite* sprite_init(int x, int y, enum SpriteSize size,
        int horizontal_flip, int vertical_flip, int tile_index, int priority) {

    /* grab the next index */
    int index = next_sprite_index++;

    /* setup the bits used for each shape/size possible */
    int size_bits, shape_bits;
    switch (size) {
        case SIZE_8_8:   size_bits = 0; shape_bits = 0; break;
        case SIZE_16_16: size_bits = 1; shape_bits = 0; break;
        case SIZE_32_32: size_bits = 2; shape_bits = 0; break;
        case SIZE_64_64: size_bits = 3; shape_bits = 0; break;
        case SIZE_16_8:  size_bits = 0; shape_bits = 1; break;
        case SIZE_32_8:  size_bits = 1; shape_bits = 1; break;
        case SIZE_32_16: size_bits = 2; shape_bits = 1; break;
        case SIZE_64_32: size_bits = 3; shape_bits = 1; break;
        case SIZE_8_16:  size_bits = 0; shape_bits = 2; break;
        case SIZE_8_32:  size_bits = 1; shape_bits = 2; break;
        case SIZE_16_32: size_bits = 2; shape_bits = 2; break;
        case SIZE_32_64: size_bits = 3; shape_bits = 2; break;
    }

    int h = horizontal_flip ? 1 : 0;
    int v = vertical_flip ? 1 : 0;

    /* set up the first attribute */
    sprites[index].attribute0 = y     |         /* y coordinate */
                            (0 << 8)  |         /* rendering mode */
                            (0 << 10) |         /* gfx mode */
                            (0 << 12) |         /* mosaic */
                            (1 << 13) |         /* color mode, 0:16, 1:256 */
                            (shape_bits << 14); /* shape */

    /* set up the second attribute */
    sprites[index].attribute1 = x     |         /* x coordinate */
                            (0 << 9)  |         /* affine flag */
                            (h << 12) |         /* horizontal flip flag */
                            (v << 13) |         /* vertical flip flag */
                            (size_bits << 14);  /* size */

    /* setup the second attribute */
    sprites[index].attribute2 = tile_index   |  /* tile index */
                            (priority << 10) |  /* priority */
                            (0 << 12);          /* palette bank (only 16 color)*/

    /* return pointer to this sprite */
    return &sprites[index];
}

/* set a sprite position */
void sprite_position(struct Sprite* sprite, int x, int y) {
    /* clear out the y coordinate */
    sprite->attribute0 &= 0xff00;

    /* set the new y coordinate */
    sprite->attribute0 |= (y & 0xff);

    /* clear out the x coordinate */
    sprite->attribute1 &= 0xfe00;

    /* set the new x coordinate */
    sprite->attribute1 |= (x & 0x1ff);
}

/* move a sprite in a direction */
void sprite_move(struct Sprite* sprite, int dx, int dy) {
    /* get the current y coordinate */
    int y = sprite->attribute0 & 0xff;

    /* get the current x coordinate */
    int x = sprite->attribute1 & 0x1ff;

    /* move to the new location */
    sprite_position(sprite, x + dx, y + dy);
}

/* change the vertical flip flag */
void sprite_set_vertical_flip(struct Sprite* sprite, int vertical_flip) {
    if (vertical_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x2000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xdfff;
    }
}

/* change the vertical flip flag */
void sprite_set_horizontal_flip(struct Sprite* sprite, int horizontal_flip) {
    if (horizontal_flip) {
        /* set the bit */
        sprite->attribute1 |= 0x1000;
    } else {
        /* clear the bit */
        sprite->attribute1 &= 0xefff;
    }
}

/* change the tile offset of a sprite */
void sprite_set_offset(struct Sprite* sprite, int offset) {
    /* clear the old offset */
    sprite->attribute2 &= 0xfc00;

    /* apply the new one */
    sprite->attribute2 |= (offset & 0x03ff);
}

/* update all of the spries on the screen */
void sprite_update_all() {
    /* copy them all over */
    memcpy16_dma((unsigned short*) sprite_attribute_memory, (unsigned short*) sprites, NUM_SPRITES * 4);
}

/* a struct for the chocula's logic and behavior */
struct Chocula {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y position */
    int x, y;

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the chocula is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the chocula stays */
    int border;

    /* number of lives*/
    int lives;
};

struct Fireball {
    struct Sprite* sprite;
    int x, y;
    int vx, vy;
     /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the chocula is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the chocula stays */
    int border;

};

struct Cookie {
    struct Sprite* sprite;
    int x, y;
    int vx, vy;
     /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the chocula is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the chocula stays */
    int border;

};
/* a struct for the koopa's logic and behavior */
struct Koopa {
    /* the actual sprite attribute info */
    struct Sprite* sprite;

    /* the x and y postion */
    int x, y;

    /* which frame of the animation he is on */
    int frame;

    /* the number of frames to wait before flipping */
    int animation_delay;

    /* the animation counter counts how many frames until we flip */
    int counter;

    /* whether the koopa is moving right now or not */
    int move;

    /* the number of pixels away from the edge of the screen the koopa stays */
    int border;

    /* checks if koopa is attacking or not */
    int attacking;

    /* koopa's y velocity in 1/256 pixels/second */
    int yvel;

    /* koopa's y acceleration in 1/256 pixels/second */
    int gravity;

    /* if koopa is currently falling */
    int falling;

    /* koopa's remaining lives */
    int lives;
};

void koopa_init(struct Koopa* koopa) {
    koopa->x = 100;
    koopa->y = 120;
    koopa->border = 40;
    koopa->frame = 0;
    koopa->move = 0;
    koopa->counter = 0;
    koopa->animation_delay = 8;
    koopa->attacking = 0;
    koopa->yvel = 0;
    koopa->gravity = 50;
    koopa->falling = 0;
    koopa->lives = 30;
    koopa->sprite = sprite_init(koopa->x, koopa->y, SIZE_32_32, 0, 0, koopa->frame, 0);
}

void chocula_init(struct Chocula* chocula) {
    chocula->x = 210;
    chocula->y = 90;
    chocula->border = 40;
    chocula->frame = 0;
    chocula->move = 0;
    chocula->counter = 0;
    chocula->animation_delay = 8;
    chocula->sprite = sprite_init(chocula->x, chocula->y, SIZE_32_64, 0, 0, chocula->frame, 0);
    chocula->lives = 500;
    sprite_set_offset(chocula->sprite,96);
}


void fireball_init(struct Fireball* fireball){
    fireball->x = 180;
    fireball->y = 95;
    fireball->vx = -1;
    fireball->vy = 0;
    fireball->sprite = sprite_init(fireball->x, fireball->y, SIZE_32_32, 0, 0, 0, 0);
    sprite_set_offset(fireball->sprite, 160);
}

int random(int num){
    if(num > 9){
        num = num-9;
    }
    return (num+3);
}

void cookie_init(struct Cookie* cookie){
    cookie->x = 180;
    cookie->y = 90;
    cookie->vx = -1;
    cookie->vy = 1;
    cookie->sprite = sprite_init(cookie->x, cookie->y, SIZE_32_32, 0, 0, 0, 0);
    sprite_set_offset(cookie->sprite, 192);
}


/* move the chocula left or right returns if it is at edge of the screen */
int chocula_left(struct Chocula* chocula) {
    /* face left */
    sprite_set_horizontal_flip(chocula->sprite, 1);
    chocula->move = 1;

    /* if we are at the left end, just scroll the screen */
    if (chocula->x < chocula->border) {
        return 1;
    } else {
        /* else move left */
        chocula->x--;
        return 0;
    }
}
/*make the koopa jump */
void koopa_jump(struct Koopa* koopa) {
    if (!koopa->falling) {
        koopa->yvel = -1350;
        koopa->falling = 1;
    }
}

/* move the koopa left or right returns if it is at edge of the screen */
int koopa_left(struct Koopa* koopa) {
    /* face left */
    sprite_set_horizontal_flip(koopa->sprite, 1);
    koopa->move = 1;

    /* if we are at the left end, just scroll the screen */
    if (koopa->x < koopa->border) {
        return 1;
    } else {
        /* else move left */
        koopa->x--;
        return 0;
    }
}
int koopa_right(struct Koopa* koopa) {
    /* face right */
    sprite_set_horizontal_flip(koopa->sprite, 0);
    koopa->move = 1;

    /* if we are at the right end, just scroll the screen */
    if (koopa->x > (WIDTH - 16 - koopa->border)) {
        return 1;
    } else {
        /* else move right */
        koopa->x++;
        return 0;
    }
}

void koopa_stop(struct Koopa* koopa) {
    koopa->move = 0;
    koopa->frame = 0;
    koopa->counter = 7;
    koopa->attacking = 0;
    sprite_set_offset(koopa->sprite, koopa->frame);
}

void koopa_attack(struct Koopa* koopa, struct Chocula* chocula, int init) {
    sprite_set_offset(koopa->sprite, 64);
    if(init == 1){
        if(koopa->x >= 180){
            chocula->lives --;
        }
    }
}
/* finds which tile a screen coordinate maps to, taking scroll into acco  unt */
unsigned short tile_lookup(int x, int y, int xscroll, int yscroll,
        const unsigned short* tilemap, int tilemap_w, int tilemap_h) {

    /* adjust for the scroll */
    x += xscroll;
    y += yscroll;

    /* convert from screen coordinates to tile coordinates */
    x >>= 3;
    y >>= 3;

    /* account for wraparound */
    while (x >= tilemap_w) {
        x -= tilemap_w;
    }
    while (y >= tilemap_h) {
        y -= tilemap_h;
    }
    while (x < 0) {
        x += tilemap_w;
    }
    while (y < 0) {
        y += tilemap_h;
    }

    /* the larger screen maps (bigger than 32x32) are made of multiple stitched
       together - the offset is used for finding which screen block we are in
       for these cases */
    int offset = 0;

    /* if the width is 64, add 0x400 offset to get to tile maps on right   */
    if (tilemap_w == 64 && x >= 32) {
        x -= 32;
        offset += 0x400;
    }

    /* if height is 64 and were down there */
    if (tilemap_h == 64 && y >= 32) {
        y -= 32;

        /* if width is also 64 add 0x800, else just 0x400 */
        if (tilemap_w == 64) {
            offset += 0x800;
        } else {
            offset += 0x400;
        }
    }

    /* find the index in this tile map */
    int index = y * 32 + x;
    /* return the tile */
    return tilemap[index + offset];

}

/* update the koopa */
void koopa_update(struct Koopa* koopa,struct Cookie* cookie, struct Fireball* fireball, int xscroll) {
    /* update y position and speed if falling */
    if (koopa->falling) {
        koopa->y += (koopa->yvel >> 8);
        koopa->yvel += koopa->gravity;
    }

    if ((fireball->x < koopa ->x) && (fireball->x + 25 > koopa->x) && (fireball->y > koopa->y) && (fireball->y < koopa->y + 32)){
        koopa->lives--;
    }
    if ((fireball->x < koopa ->x) && (fireball->x + 40 > koopa->x) && (fireball->y > koopa->y) && (fireball->y < koopa->y + 32)){
        koopa->lives--;
    }

    /* check which tile the simon's feet are over */
    unsigned short tile = tile_lookup(koopa->x + 8, koopa->y + 32, xscroll, 0, wall, wall_width, wall_height);

    if ((tile == 210)) {
        /*stop falling */
        koopa->falling = 0;
        koopa->yvel = 0;
        koopa->y &= ~0x3;
        koopa->y++;
    }

    if (koopa->move) {
        koopa->counter++;
        if (koopa->counter >= koopa->animation_delay) {
            koopa->frame = koopa->frame + 32;
            if (koopa->frame > 32) {
                koopa->frame = 0;
            }
            sprite_set_offset(koopa->sprite, koopa->frame);
            koopa->counter = 0;
        }
    }

    sprite_position(koopa->sprite, koopa->x, koopa->y);
}

int chocula_right(struct Chocula* chocula) {
    /* face right */
    sprite_set_horizontal_flip(chocula->sprite, 0);
    chocula->move = 1;

    /* if we are at the right end, just scroll the screen */
    if (chocula->x > (WIDTH - 16 - chocula->border)) {
        return 1;
    } else {
        /* else move right */
        chocula->x++;
        return 0;
    }
}

void chocula_stop(struct Chocula* chocula) {
    chocula->move = 0;
    chocula->frame = 0;
    chocula->counter = 7;
    sprite_set_offset(chocula->sprite, chocula->frame);
}
void fireball_update(struct Fireball* fireball){
    fireball->x += fireball->vx;
    fireball->y += fireball->vy;
    sprite_position(fireball->sprite, fireball->x, fireball->y);
}
void cookie_update(struct Cookie* cookie){
    cookie->x += cookie->vx;
    cookie->y += cookie->vy;
    if (cookie->y <= 10){
        cookie->vy = -1*(cookie->vy);
    }
    else if(cookie->y >= 120){
        cookie ->vy = -1*(cookie->vy);
    }
    sprite_position(cookie->sprite, cookie->x, cookie->y);
}
/* update the chocula */
void chocula_update(struct Chocula* chocula) {
    if (chocula->move) {
        chocula->counter++;
        if (chocula->counter >= chocula->animation_delay) {
            chocula->frame = chocula->frame + 16;
            if (chocula->frame > 16) {
                chocula->frame = 0;
            }
            sprite_set_offset(chocula->sprite, chocula->frame);
            chocula->counter = 0;
        }
    }

    sprite_position(chocula->sprite, chocula->x, chocula->y);
}

int gameOver(int playerLives, int bossLives);
int checkScroll(int x);


/* the scanline counter is a memory cell which is updated to indicate how
 * much of the screen has been drawn */
volatile unsigned short* scanline_counter = (volatile unsigned short*) 0x4000006;

/* wait for the screen to be fully drawn so we can do something during vblank */
void wait_vblank() {
    /* wait until all 160 lines have been updated */
    while (*scanline_counter < 160) { }
}


/* this function checks whether a particular button has been pressed */
unsigned char button_pressed(unsigned short button) {
    /* and the button register with the button constant we want */
    unsigned short pressed = *buttons & button;

    /* if this value is zero, then it's not pressed */
    if (pressed == 0) {
        return 1;
    } else {
        return 0;
    }
}


/* return a pointer to one of the 4 character blocks (0-3) */
volatile unsigned short* char_block(unsigned long block) {
    /* they are each 16K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x4000));
}

/* return a pointer to one of the 32 screen blocks (0-31) */
volatile unsigned short* screen_block(unsigned long block) {
    /* they are each 2K big */
    return (volatile unsigned short*) (0x6000000 + (block * 0x800));
}


/* function to setup background 0 for this program */
void setup_background() {

    /* load the palette from the image into palette memory*/
    for (int i = 0; i < PALETTE_SIZE; i++) {
        bg_palette[i] = castle_palette[i];
    }

    /* load the image into char block 0 (16 bits at a time) */
    volatile unsigned short* dest = char_block(0);
    /*volatile unsigned short* dest2 = char_block(0);*/
    unsigned short* image = (unsigned short*) castle_data;
    for (int i = 0; i < ((castle_width * castle_height) / 2); i++) {
        dest[i] = image[i];
    }
     /*for (int i = 0; i < ((tilemap2_width * tilemap2_height) / 2); i++){
        dest2[i] = image[i];
    }*/

    /* set all control the bits in this register */
    *bg0_control = 0 |    /* priority, 0 is highest, 3 is lowest */
        (0 << 2)  |       /* the char block the image data is stored in */
        (0 << 6)  |       /* the mosaic flag */
        (1 << 7)  |       /* color mode, 0 is 16 colors, 1 is 256 colors */
        (16 << 8) |       /* the screen block the tile data is stored in */
        (1 << 13) |       /* wrapping flag */
        (0 << 14);        /* bg size, 0 is 256x256 */

   /* *bg1_control = 0 |
        (0 << 2)  |
        (0 << 6)  |
        (1 << 7)  |
        (17 << 8) |
        (1 << 13) |
        (0 << 14);*/

    /* load the tile data into screen block 16 */
     dest = screen_block(16);
    for (int i = 0; i < (wall_width * wall_height); i++) {
        dest[i] = wall[i];
    }
    /*dest2 = screen_block(17);
    for (int i = 0; i< (map_width * map_height); i++){
        dest2[i] = map2[i];
    }*/
}
/* setup all sprites */
void sprite_clear() {
    /* clear the index counter */
    next_sprite_index = 0;

    /* move all sprites offscreen to hide them */
    for(int i = 0; i < NUM_SPRITES; i++) {
        sprites[i].attribute0 = HEIGHT;
        sprites[i].attribute1 = WIDTH;
    }
}
/* just kill time */
void delay(unsigned int amount) {
    for (int i = 0; i < amount * 10; i++);
}

int main() {
    *display_control = MODE0 | BG0_ENABLE | SPRITE_ENABLE | SPRITE_MAP_1D;
    setup_background();
    setup_sprite_image();
    sprite_clear();
    int xscroll = 0;
    int yscroll = 0;

    struct Koopa koopa;
    koopa_init(&koopa);
    int flip = 1;

    /* create the chocula */
    struct Chocula chocula;
    int chocula_initialize = 0;

    struct Cookie cookie;
    int cookie_initialize = 0;
    int cookie_cooldown = 0;

    struct Fireball fireball;
    int fireball_initialize = 0;
    int fireball_cooldown = 0;
    int choc_on_screen = 0;
    int end = 0;
    while(1) {
        koopa_update(&koopa,&cookie,&fireball, xscroll);
        if(chocula_initialize){
            end = gameOver(koopa.lives,chocula.lives);
        }
        if (end== 0) {
            if (checkScroll(xscroll) && !chocula_initialize){
                chocula_init(&chocula);
                choc_on_screen = 1;
                chocula_initialize = 1;
                sprite_set_horizontal_flip(chocula.sprite,1);
            }
            if (chocula_initialize && !fireball_initialize){
                fireball_init(&fireball);
                fireball_initialize = 1;
            }
            if (fireball_initialize && fireball_cooldown <= 0){
                if(fireball.x > -30){
                    fireball_update(&fireball);
                }
                else if (fireball.x <= -30){
                    fireball_cooldown = 300;
                }
            }
            if(cookie_cooldown > 0){
                cookie.x = 180;
                cookie_cooldown -= 1;
            }
            if (fireball_cooldown > 0){
                fireball.x = 180;
                fireball_cooldown -= 1;
                if(!cookie_initialize){
                    cookie_init(&cookie);
                    cookie_initialize = 1;
                }
                if(cookie.x > -30){
                    cookie_update(&cookie);
                }
                else if(cookie.x <= -30){
                    cookie_cooldown = 50;
                }
            }
            if (button_pressed(BUTTON_RIGHT)) {
                if (koopa_right(&koopa)) {
                    xscroll++;
                }
            } else if (button_pressed(BUTTON_LEFT)) {
                if (koopa_left(&koopa)) {
                    xscroll--;
                }
            } else if (button_pressed(BUTTON_B)) {
                koopa_attack(&koopa,&chocula,chocula_initialize);
            } else {
                koopa_stop(&koopa);
            }

            if (button_pressed(BUTTON_A)) {
                koopa_jump(&koopa);
            }
        }
             /* we died */
        else if (end == 1) {
            flip = flip^1;
             sprite_set_horizontal_flip(chocula.sprite, flip);
             sprite_set_offset(koopa.sprite,224);
             koopa.y = 130;
        }

        /* we won */
        else {
            flip = flip^1;
            sprite_set_horizontal_flip(koopa.sprite, flip);
            sprite_set_offset(chocula.sprite,256);
            chocula.y = 130;
        }
        wait_vblank();
        if (choc_on_screen == 0){
            *bg0_x_scroll = xscroll;
            *bg0_y_scroll = yscroll;
            *bg1_x_scroll = 2 * xscroll;
            *bg1_y_scroll = 2 * yscroll;
        }
        sprite_update_all();
        delay(50);
    }
}
