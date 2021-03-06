#include <stddef.h>
#include <drivers/vga.h>

unsigned char vga_current_mode = 0; // No need for more than 255
unsigned char vga_current_mode_max_height = 0;       // Same
unsigned char vga_current_mode_max_width = 0;        // Same
char vga_default_colour = 0;

unsigned short *vga_memory_pointer;;
unsigned char vga_current_attribute = 0x0F;

unsigned char vga_row = 0;
unsigned char vga_column = 0;

// Code snippet taken from osdev.org Wiki
unsigned char vga_set_color(enum vga_color vga_foreground_colour, enum vga_color vga_background_colour) 
{
    return vga_foreground_colour | vga_background_colour << 4;
}

int vga_get_cursor_offset()
{
    outb(VGA_SCREEN_CONTROL_REGISTER, 14);
    int vga_offset = inb(VGA_SCREEN_DATA_REGISTER) << 8;
    outb(VGA_SCREEN_CONTROL_REGISTER, 15);
    vga_offset += inb(VGA_SCREEN_DATA_REGISTER);
    return vga_offset * 2;
}

int vga_get_offset_row(int vga_offset)
{
    return vga_offset / (2 * VGA_MAXIMUM_COLUMNS);
}

int vga_get_offset_col(int vga_offset)
{
    return (vga_offset - (vga_get_offset_row(vga_offset)*2*VGA_MAXIMUM_COLUMNS))/2;
}

void x86_screen_write_regs(unsigned char* regs)
{
    unsigned i;
    outb(VGA_MISC_WRITE, *regs);
    regs++;

    for (i = 0; i < VGA_NUM_SEQ_REGS; i++)
    {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, *regs);
        regs++;
    }
    
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
    regs[0x03] |= 0x80;
    regs[0x11] &= ~0x80;
    
    for (i = 0; i < VGA_NUM_CRTC_REGS; i++)
    {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, *regs);
        regs++;
    }
    
    for (i = 0; i < VGA_NUM_GC_REGS; i++)
    {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, *regs);
        regs++;
    }
    
    for (i = 0; i < VGA_NUM_AC_REGS; i++)
    {
        (void)inb(VGA_INSTAT_READ);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, *regs);
        regs++;
    }
    
    inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, 0x20);
}

void x86_update_screen_mode_info(unsigned char mode)
{
    switch(mode)
    {
        case(VGA_80x25_TEXT_MODE):
            vga_memory_pointer = (unsigned short *)VGA_TEXT_MODE_VIDEO_ADDRESS;
            vga_current_mode = VGA_80x25_TEXT_MODE; // 80x25 Text Mode
            vga_current_mode_max_height = 80;
            vga_current_mode_max_width = 25;
            break;
        case(VGA_320x200x256_MODE):
            vga_current_mode = VGA_320x200x256_MODE; // 320x200x256 SD Mode
            vga_current_mode_max_height = 320;
            vga_current_mode_max_width = 200;
        default:
            break;
    }
}

void scroll(void)
{
    unsigned char attributeByte = (0 << 4) | (15 & 0x0F);
    unsigned short blank = 0x20 | (attributeByte << 8);

    if(vga_column >= VGA_MAXIMUM_ROWS)
    {
        int i;

        for (i = 0*VGA_MAXIMUM_COLUMNS; i < (VGA_MAXIMUM_ROWS-1)*VGA_MAXIMUM_COLUMNS; i++)
        {
            vga_memory_pointer[i] = vga_memory_pointer[i+VGA_MAXIMUM_COLUMNS];
        }

        for (i = (VGA_MAXIMUM_ROWS-1)*VGA_MAXIMUM_COLUMNS; i < VGA_MAXIMUM_ROWS*VGA_MAXIMUM_COLUMNS; i++)
        {
            vga_memory_pointer[i] = blank;
        }

        vga_column = (VGA_MAXIMUM_ROWS-1);
    }
}

void move_csr()
{
    unsigned temp;
    temp = vga_column * VGA_MAXIMUM_COLUMNS + vga_row;
    outb(0x3D4, 14);
    outb(0x3D5, temp >> 8);
    outb(0x3D4, 15);
    outb(0x3D5, temp);
}

void update_cursor(int newrow, int newcol)
{
    unsigned short position = (newrow*80) + newcol;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (unsigned char)(position&0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (unsigned char)((position>>8)&0xFF));
    vga_row = newrow;
    vga_column = newcol;
    move_csr();
}

int get_row()
{
    return vga_row;
}

int get_col()
{
    return vga_column;
}

void disable_cursor(void)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x3f);
}

void enable_cursor(unsigned char cursor_start, unsigned char cursor_end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3E0) & 0xE0) | cursor_end);
}

void cls(void)
{
    update_cursor(0, 0);
    int i = 0;
    for (i = 0; i < VGA_MAXIMUM_COLUMNS * VGA_MAXIMUM_ROWS; i++)
        vga_memory_pointer[i] = (vga_current_attribute << 8) | 0x20;
}

void printkcenter(char* c)
{
    int size = 0;

    while(c[size])
    {
        size++;
    }

    int half;

    if(size % 2 != 0)
    {
        half = size / 2 -1;

        for(int i = 0; i < 40 - half - 1; i++)
        {
            putch(' ');
        }

        printk(c);

        for(int i = 0; i < 40 - half - 2; i++)
        {
            putch(' ');
        }
    }
    else
    {
        half = size / 2;

        for(int i = 0; i < 40 - half; i++) 
        {
            putch(' ');
        }
        
        printk(c);
        
        for(int i = 0; i < 40 - half; i++)
        {
            putch(' ');
        }
    }
}

void printkc(char *text, int colour)
{
    settextcolor(colour, VGA_COLOR_BLACK);
    printk("%s", text);
    settextcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
}

void printkok(char *text)
{
    printk("%s", text);
    update_cursor(VGA_MAXIMUM_COLUMNS-6, get_col());
    printk("[ ");
    settextcolor(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    printk("OK");
    settextcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    printk(" ]");
}

void printkfail(char *text)
{
    printk("%s", text);
    update_cursor(VGA_MAXIMUM_COLUMNS-6, get_col());
    printk("[");
    settextcolor(VGA_COLOR_RED, VGA_COLOR_BLACK);
    printk("FAIL");
    settextcolor(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    printk("]");
}

void putch(char c)
{
    unsigned short *where;
    unsigned att = vga_current_attribute << 8;

    if(c == 0x08)
    {
        if(vga_row != 0) vga_row--;
    }
    else if(c == 0x09)
    {
        vga_row = (vga_row + 8) & ~(8 - 1);
    }
    else if(c == '\r')
    {
        vga_row = 0;
    }
    else if(c == '\n')
    {
        vga_row = 0;
        vga_column++;
    }
    else if(c >= ' ')
    {
        where = vga_memory_pointer + (vga_column * VGA_MAXIMUM_COLUMNS + vga_row);
        *where = c | att;
        vga_row++;
    }

    if(vga_row >= VGA_MAXIMUM_COLUMNS)
    {
        vga_row = 0;
        vga_column++;
    }

    scroll();
    move_csr();
}

int vga_get_offset(int vga_row, int vga_column)
{
    return 2 * (vga_row * VGA_MAXIMUM_COLUMNS + vga_column);
}

void vga_set_cursor_offset(int vga_offset)
{
    vga_offset /= 2;
    outb(VGA_SCREEN_CONTROL_REGISTER, 14);
    outb(VGA_SCREEN_DATA_REGISTER, (unsigned char)(vga_offset >> 8));
    outb(VGA_SCREEN_CONTROL_REGISTER, 15);
    outb(VGA_SCREEN_DATA_REGISTER, (unsigned char)(vga_offset & 0xff));
}

void print(char *text)
{
    unsigned int i;

    for (i = 0; i < strlen(text); i++)
    {
        putch(text[i]);
    }
}

void settextcolor(unsigned char forecolor, unsigned char backcolor)
{
    vga_current_attribute = (backcolor << 4) | (forecolor & 0x0F);
}

void x86_setup_vga_text_mode()
{
    x86_update_screen_mode_info(VGA_80x25_TEXT_MODE);
    vga_default_colour = vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    unsigned int initial_vga_offset = vga_get_cursor_offset();
    unsigned char initial_vga_row = vga_get_offset_row(initial_vga_offset);
    // Add 1 to the initial vga row so that it jumps to a new line right after we give control to the kernel
    initial_vga_offset = vga_get_offset(initial_vga_row + 1, 0);
    vga_set_cursor_offset(initial_vga_offset);
    disable_cursor();
    update_cursor(0, initial_vga_row + 1); // row , col
    enable_cursor(14, 15);
}

void init_text_mode()
{
    x86_screen_write_regs(vga_80x25_text_mode);
    unsigned char* vga_poke = (unsigned char*) 0x400;
    vga_poke[0x4A] = 0x00;
    vga_poke[0x4A+1] = 80;
    vga_poke[0x4C] = 0x0f;
    vga_poke[0x4C+1] = 0xa0;
    vga_poke[0x50] = 0x00;
    vga_poke[0x50+1] = 0x00;
    vga_poke[0x60] = 15;
    vga_poke[0x61] = 14;
    vga_poke[0x84] = 24;
    vga_poke[0x85] = 16;

    /* cakeh: We still need to clear screen and
       finish up last things */
    x86_setup_vga_text_mode();
}

void printkint(const int number)
{
    char buf[20];
    buf[0] = 0;
    int len = 0;
    int abs_value = number;

    if (number < 0)
    {
        abs_value = -number;
    }

    int last_digit;

    while (abs_value > 9)
    {
        last_digit = abs_value % 10;
        abs_value /= 10;
        buf[++len] = (char) last_digit + '0';
    }

    buf[++len] = (char) abs_value + '0';

    if (number < 0)
    {
        putch('-');
    }

    while (len > 0)
    {
        putch(buf[len--]);
    }
}

void printkdec(unsigned short n)
{
    if (n == 0)
    {
        putch('0');
        return;
    }

    int acc = n;
    char c[32];
    int i = 0;
    
    while (acc > 0)
    {
        c[i] = '0' + acc%10;
        acc /= 10;
        i++;
    }

    c[i] = 0;

    char c2[32];
    c2[i--] = 0;
    int j = 0;

    while(i >= 0)
    {
        c2[i--] = c[j++];
    }

    print(c2);
}

char hex_letters[] =
{
    '0', '1', '2', '3', '4', '5', '6', '7',
    '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'
};

void printkhex(unsigned int number)
{
    char buf[20];
    buf[0] = 0;
    unsigned int len = 0;

    unsigned int value = number;

    while (value >= 16)
    {
        buf[++len] = hex_letters[value & 0xF];
        value >>= 4;
    }

    buf[++len] = hex_letters[value & 0xF];

    while (len)
    {
        putch(buf[len--]);
    }
}

void printkuint(const unsigned short number)
{
    char buf[20];
    buf[0] = 0;
    int len = 0;

    unsigned short value = number;

    while (value >= 10)
    {
        int last_digit = value % 10;
        value /= 10;
        buf[++len] = '0' + last_digit;
    }

    buf[++len] = '0' + value;

    while (len)
    {
        putch(buf[len--]);
    }
}

void delete_last()
{
  update_cursor(get_row()-1, get_col());
  putch(' ');
  update_cursor(get_row()-1, get_col());
}

void enable_cursorq() {
    outb(0x3D4, 0x0A);
    char curstart = inb(0x3D5) & 0x1F; // get cursor scanline start

    outb(0x3D4, 0x0A);
    outb(0x3D5, curstart | 0x20); // set enable bit
}

void x86_switch_screen_mode_command(char *regs)
{
    if (!strcmp(regs, "modes")) {
        /* Make mode gathering automatic
           either by using multiboot header
           or using GPU and Monitor values
           or maybe EDID? */
        printkc("Available VESA modes:\n", VGA_COLOR_LIGHT_BLUE);
        printkc("Height     Width     BPP\n", VGA_COLOR_GREEN);
        printk("80      x  25\n");
    } else if (!strcmp(regs, "80x25")) {
        if (vga_current_mode == VGA_80x25_TEXT_MODE) {
            printkc("You can't set a mode you're already using!\n", VGA_COLOR_RED);
        } else {
            x86_screen_write_regs(vga_80x25_text_mode);
            init_text_mode();
        }
    } else if (!strcmp(regs, "") || !strcmp(regs, "help")) {
        printkc("Usage:\n", VGA_COLOR_LIGHT_BLUE);
        printk("vbe MODE\n");
        printkc("For available modes, use:\n", VGA_COLOR_LIGHT_BLUE);
        printk("vbe modes\n");
        printkc("Example:\n", VGA_COLOR_LIGHT_BLUE);
        printk("vbe 80x25\n");
    } else {
        printkc("VESA mode not found!\n", VGA_COLOR_RED);
    }
}