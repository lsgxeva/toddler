__asm__ (".code16gcc");
__asm__ ("jmp main");


#include "loader/loader.h"
#include "common/include/data.h"
#include "common/include/memlayout.h"
#include "common/include/bootparam.h"


static struct loader_variables *loader_var;
static struct boot_parameters *boot_param;


static void real_mode set_cursor_pos(u32 row, u32 col)
{
    u32 edx = 0;
    
    // Get current pos
    __asm__ __volatile__
    (
        "int    $0x10;"
        : "=d" (edx)
        : "a" (0x300), "b" (0)
        : "ecx"
    );
    
    // Calc new pos
    edx = (row << 8) + col;
    
    // Set new pos
    __asm__ __volatile__
    (
        "int    $0x10;"
        :
        : "a" (0x200), "b" (0), "d" (edx)
    );
    
//     u32 edx = (row << 8) + col;
//     
//     __asm__ __volatile__
//     (
//         "push   %%edx;"
//         "movw   $0x300, %%ax;"
//         "movw   $0, %%bx;"
//         "int    $0x10;"
//         "movw   $0x200, %%ax;"
//         "pop    %%edx;"
//         "int    $0x10"
//         :
//         : "d" (edx)
//         : "eax", "ebx"
//     );
}

static void real_mode print_new_line()
{
    boot_param->cursor_row++;
    boot_param->cursor_col = 0;
    
    if (boot_param->cursor_row >= 80) {
        boot_param->cursor_row = 0;
    }
    
    set_cursor_pos(boot_param->cursor_row, boot_param->cursor_col);
}

static void real_mode print_char(char ch)
{
    u32 eax = (0x9 << 8) | (u32)ch;
    
    __asm__ __volatile__
    (
        "int    $0x10"
        :
        : "a" (eax), "b" (0x7), "c" (0x1)
    );
    
    boot_param->cursor_col++;
    if (80 == boot_param->cursor_col) {
        print_new_line();
    } else {
        set_cursor_pos(boot_param->cursor_row, boot_param->cursor_col);
    }
}

static void real_mode print_string(char *str)
{
    while (*str) {
        switch (*str) {
        case '\n':
        case '\r':
            print_new_line();
            break;
        case '\\':
            print_char('\\');
            break;
        case '\t':
            boot_param->cursor_col /= 8;
            boot_param->cursor_col = (boot_param->cursor_col + 1) * 8;
            if (boot_param->cursor_col >= 80) {
                print_new_line();
            } else {
                set_cursor_pos(boot_param->cursor_row, boot_param->cursor_col);
            }
            break;
        default:
            print_char(*str);
            break;
        }
        
        str++;
    }
}

// static void real_mode print_bin(u32 n)
// {
//     u32 i, value;
//     
//     for (i = 0; i < sizeof(u32) * 8; i++) {
//         value = n;
//         value = value << i;
//         value = value >> sizeof(u32) * 8 - 1;
//         
//         print_char(value ? '1' : '0');
//     }
//     
//     print_char('b');
// }

static void real_mode print_dec(u32 n)
{
    int divider = 1000000000;
    int started = 0;
    u32 num = n;
    int i;
    
    if (!n) {
        print_char('0');
    }
    
    for (i = 0; i < 10; i++) {
        if (num / divider) {
            started = 1;
            print_char('0' + num / divider);
        } else if (started) {
            print_char('0');
        }
        
        num %= divider;
        divider /= 10;
    }
}

static void real_mode print_hex(u32 n)
{
    int i;
    u32 digit;
    
    for (i = 0; i < 32; i += 4) {
        digit = n;
        digit <<= i;
        digit >>= 28;
        
        print_char(digit > 9 ? (u8)(digit + 0x30 + 0x27) : (u8)(digit + 0x30));
    }
    
    print_char('h');
}

static void real_mode stop()
{
    print_string("\nUnable to start Toddler!\n");
    
    do {
        __asm__ __volatile__
        (
            "hlt"
            :
            :
        );
    } while (1);
}

static void real_mode print_done()
{
    print_string(" Done!\n");
}

static void real_mode print_failed()
{
    print_string(" Failed!\n");
}

static void real_mode fail()
{
    print_failed();
    stop();
}

static void real_mode init_cursor_pos()
{
    u32 edx = 0;
    
    __asm__ __volatile__
    (
        "xorl   %%edx, %%edx;"
        "int    $0x10"
        : "=d" (edx)
        : "a" (0x300), "b" (0)
        : "ecx"
    );
    
    boot_param->cursor_row = ((edx << 16) >> 24);
    boot_param->cursor_col = ((edx << 24) >> 24);
    
    print_string(" OS Loader is now running!\n");
}

static void real_mode init_vesa()
{
    struct vesa_info vesa_info;
    struct vesa_mode_info mode_info;
    u32 eax;
    
    //return;
    
    // Zero the info buffer
    
    // Invoke BIOS
    print_string("Getting VESA info ...");
    __asm__ __volatile__
    (
        "pushw  %%es;"
        "xorl   %%eax, %%eax;"
        "movw   %%ax, %%es;"
        "movw   $0x4f00, %%ax;"
        "int    $0x10;"
        "popw   %%es;"
        : "=a" (eax)
        : "D" (&vesa_info)
    );
    
    // quit if there was an error
    if (eax >> 8) {
        boot_param->video_mode = 0;
        
        boot_param->res_x = 80;
        boot_param->res_y = 25;
        
        boot_param->framebuffer_addr = TEXT_VIDEO_MEM_ADDR;
        boot_param->bits_per_pixel = 16;
        boot_param->bytes_per_line = 80 * 2;
        
        print_failed();
        return;
    }
    
    // Echo the VESA signature
    print_char(vesa_info.VESASignature[0]);
    print_char(vesa_info.VESASignature[1]);
    print_char(vesa_info.VESASignature[2]);
    print_char(vesa_info.VESASignature[3]);
    print_done();
    
    // Get all mode numbers
    u32 mode_ds = vesa_info.VideoModePtr >> 16;
    u32 mode_si = vesa_info.VideoModePtr & 0xffff;
    u32 number_of_modes = 0;
    u32 cur_mode;
    u16 modes[64];
    
    print_string("Getting mode list ...");
    while (1) {
        __asm__ __volatile__
        (
            "movw   %%ds, %%bx;"
            "movw   %%ax, %%ds;"
            "movw   (%%si), %%ax;"
            "movw   %%bx, %%ds;"
            : "=a" (cur_mode)
            : "a" (mode_ds), "S" (mode_si)
            : "ebx"
        );
        
        if (cur_mode == 0xFFFF) {
            break;
        }
        
        print_char('.');
        
        modes[number_of_modes] = cur_mode;
        number_of_modes++;
        mode_si += 0x2;
    }
    print_done();
    
    // Go through all modes and find the best mode for this card
    int i;
    u32 best_num, best_x, best_y, best_bits, bytes_per_line, fb_paddr;
    u32 big = 0;
    for (i = 0; i < number_of_modes; i++) {
        // Get mode info
        __asm__ __volatile__
        (
            "xorl   %%eax, %%eax;"
            "movw   %%ax, %%es;"
            "movw   $0x4F01, %%ax;"
            "int    $0x10;"
            :
            : "c" (modes[i]), "D" (&mode_info)
            : "eax"
        );
        
//         print_dec(modes[i]);
//         print_string(": ");
//         print_dec(mode_info.XResolution);
//         print_string("x");
//         print_dec(mode_info.YResolution);
//         print_string("@");
//         print_dec(mode_info.BitsPerPixel);
//         print_string("                       ");
//         print_new_line();
        
        u32 mul = mode_info.XResolution * mode_info.YResolution;
        if (mul > big && mode_info.BitsPerPixel >= 24 && mode_info.XResolution <= 1024) {
            best_num = modes[i];
            best_x = mode_info.XResolution;
            best_y = mode_info.YResolution;
            best_bits = mode_info.BitsPerPixel;
            bytes_per_line = mode_info.BytesPerScanLine;
            fb_paddr = mode_info.PhysBasePtr;
            big = mul;
        }
    }
    
    // Tell the user about the VESA info
    print_string("Best VESA mode: #");
    print_dec(best_num);
    print_string(", ");
    print_dec(best_x);
    print_string("x");
    print_dec(best_y);
    print_string(", @");
    print_dec(best_bits);
    print_string("bits, bytes/line: ");
    print_dec(bytes_per_line);
    print_string(", FB: ");
    print_hex(fb_paddr);
    print_new_line();
    
    // Set loader variables
    boot_param->video_mode = 1;
    boot_param->res_x = best_x;
    boot_param->res_y = best_y;
    boot_param->bits_per_pixel = best_bits;
    boot_param->framebuffer_addr = fb_paddr;
    boot_param->bytes_per_line = bytes_per_line;
    
    // Set the best mode
    print_string("Setting VESA mode ...");
    unsigned int bx = 0x4000 | best_num;
    __asm__ __volatile__
    (
        "int    $0x10;"
        :
        : "a" (0x4f02), "b" (bx)
    );
    print_done();
}

static void real_mode detect_ebda()
{
    print_string("Detecting EBDA  ...");
    
    u16 ebda_base_addr_from_eda = *((u16 *)POINTER_TO_EBDA_ADDR);
    u32 ebda_phys_addr = ((u32)ebda_base_addr_from_eda) << 4;
    
    print_string(" found at ");
    print_hex(ebda_phys_addr);
    print_string(" ...");
    
    boot_param->ebda_addr = ebda_phys_addr;
    print_done();
}

static void real_mode detect_memory_e820()
{
    print_string("Detecting E820 memory map  ...");
    
    u32 cur_entry_addr = (u32)loader_var->e820_map;
    loader_var->mem_part_count = 0;
    u32 finish = 0;
    u32 signature;
    
    do
    {
        if (loader_var->mem_part_count >= 128) {
            goto failed;
        }
        
        __asm__ __volatile__
        (
            "int    $0x15"
            : "=a"(signature), "=b"(finish)
            : "a"(0xe820), "b"(finish), "c"(sizeof(struct e820_entry)),
              "d"(0x534d4150), "D"(cur_entry_addr)
        );
        
        if (signature != 0x534d4150) {
            goto failed;
        }
        
        // Continue
        loader_var->mem_part_count++;
        cur_entry_addr += sizeof(struct e820_entry);
        print_char('.');
    } while (finish);
    
    print_done();
    return;
    
failed:
    fail();
    return;
}

static int real_mode empty_8042()
{
    u8 status;
    u32 loops = 10;
    u32 ffs   = 32;
    
    while (loops--) {
        io_delay();
        
        status = io_in_u8(0x64);
        if (status == 0xff) {
            // FF is a plausible, but very unlikely status
            if (!--ffs) {
                return 1; // Assume no KBC present
            }
        }
        if (status & 1) {
            // Read and discard input data
            io_delay();
            io_in_u8(0x60);
        } else if (!(status & 2)) {
            // Buffers empty, finished!
            return 0;
        }
    }
    
    return 1;
}

static int real_mode check_a20_enabled()
{
    u16 addr_low_seg = 0;
    u16 addr_low_offset = 0;
    u16 addr_high_seg = 0xffff;
    u16 addr_high_offset = 0x10;
    
    u8 origin_low;
    u8 origin_high;
    u8 new_low;
    u8 new_high;
    
    for (; addr_high_offset <= 0xff; addr_high_offset++, addr_low_offset++) {
        // Read values from the target addresses
        origin_low = read_u8_seg_offset(addr_low_seg, addr_low_offset);
        origin_high = read_u8_seg_offset(addr_high_seg, addr_high_offset);
        
        // If they are not equal, check the next byte
        if (origin_low != origin_high) {
            continue;
        }
        
        // Try to write a new value to high address
        new_high = origin_high + 1;
        write_u8_seg_offset(addr_high_seg, addr_high_offset, new_high);
        
        // Read the value from the low address */
        new_low = read_u8_seg_offset(addr_low_seg, addr_low_offset);
        
        // If they are equal, A20 is not enabled
        if (new_low == new_high) {
            goto failed;
        }
        
        // Restore the value of high address
        write_u8_seg_offset(addr_high_seg, addr_high_offset, origin_high);
    }
    
    return 1;
    
failed:
    return 0;
}

static void real_mode enable_a20()
{
    print_string("Enabling A20 ...");
    
    u8 port;
    
    // The general way to enable A20
    port = io_in_u8(0x92);        /* Configuration port A */
    port |=  0x02;                /* Enable A20 */
    port &= ~0x01;                /* Do not reset machine */
    io_out_u8(port, 0x92);
    
    print_done();
    return;
    
    u32 loops = 256;
    u32 kb_error;
    
    while (loops--) {
        // First, check to see if A20 is already enabled
        if (check_a20_enabled()) {
            goto succeed;
        }
        
        // Next, try the BIOS (INT 0x15, AX=0x2401)
        __asm__ __volatile__
        (
            "movl   $0x2401, %%eax;"
            "xorl   %%ebx, %%ebx;"
            "xorl   %%ecx, %%ecx;"
            "xorl   %%edx, %%edx;"
            "int    $0x15;"
            :
            :
            : "eax", "ebx", "ecx", "edx"
        );
        
        if (check_a20_enabled()) {
            goto succeed;
        }
        
        /* Try enabling A20 through the keyboard controller */
        kb_error = empty_8042();
        
        /* BIOS worked, but with delayed reaction */
        if (check_a20_enabled()) {
            goto succeed;
        }
        
        /* Try enabling A20 through the keyboard controller */
        if (!kb_error) {
            empty_8042();
            
            io_out_u8(0xd1, 0x64);          /* Command write */
            empty_8042();
            
            io_out_u8(0xdf, 0x60);          /* A20 on */
            empty_8042();
            
            io_out_u8(0xff, 0x64);          /* Null command, but UHCI wants it */
            empty_8042();
            
            if (check_a20_enabled()) {
                goto succeed;
            }
        }
        
        /* Finally, try enabling the "fast A20 gate" */
        u8 port_a;
        
        port_a = io_in_u8(0x92);        /* Configuration port A */
        port_a |=  0x02;                /* Enable A20 */
        port_a &= ~0x01;                /* Do not reset machine */
        io_out_u8(port_a, 0x92);
        
        if (check_a20_enabled()) {
            goto succeed;
        }
        
        print_char('.');
    }
    
    // Failed
    fail();
    return;
    
succeed:
    print_done();
}

static void real_mode enter_protected_mode()
{
    print_string("Entering Protected Mode ... @ ");
    print_hex(loader_var->return_addr);
    
    __asm__ __volatile__
    (
        "jmp    *%%ebx;"
        :
        : "b"(loader_var->return_addr)
    );
}

int real_mode main()
{
    loader_var = (struct loader_variables *)LOADER_VARIABLES_ADDR_OFFSET;
    boot_param = (struct boot_parameters *)BOOT_PARAM_ADDR_OFFSET;
    
    init_cursor_pos();
    init_vesa();
    detect_ebda();
    detect_memory_e820();
    enable_a20();
    enter_protected_mode();
    
    return 0;
}
