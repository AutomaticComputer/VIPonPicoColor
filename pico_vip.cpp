#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

#include "data.h"
#include "devices.h"


#define CYCLE_COUNT_DEF 
// #define CYCLE_COUNT_DEF int machine_cycles = 0;  // for debugging
#define CYCLE_COUNT_INIT
// #define CYCLE_COUNT_INIT machine_cycles = 0;
#define CYCLE_COUNT_ADD
// #define CYCLE_COUNT_ADD machine_cycles++;

#define WRITE_ADDR(x) (((x) & RAM_ADDRESS_MASK)) 
    // needs change for a system with > 4K RAM or the color board
#define HAS_RAM(x) (((x) < RAM_SIZE))  // always true on a 4K system

#define GET_MEMORY(x) (memory[((modify_msb || ((x) & 0x8000)) ? (((x) & 0x1ff) | 0x8000) : ((x) & RAM_ADDRESS_MASK))])



uint16_t registers[16];
uint8_t register_d;
uint8_t register_df, register_n, register_p, register_x, register_i, 
    register_tt;
uint8_t memory[65536 + 8]; // for safety in video dma
bool modify_msb;
bool register_q;
bool register_ie;

bool video_on;

enum cpu_state_enum {S0, S1, S2, S3, S1_2_0, S1_2_1, S1_2_NOP, S_IDL};
cpu_state_enum cpu_state;

enum cpu_instr_enum { 
    I_IDL, I_LDN, 
    I_INC, 
    I_DEC, 
    I_BR, I_NBR, I_BQ, I_BNQ, I_BZ, I_BNZ, I_BDF, I_BNF, I_B_EF, I_BN_EF, 

    I_LDA, 
    I_STR, 
    I_IRX, 
    I_OUTN, I_INPN, 

    I_RET, 
    I_DIS, I_LDXA, I_STXD, 
    I_ADC, I_SDB, I_SHRC, I_SMB, I_SAV, I_MARK, I_REQ, I_SEQ, 
    I_ADCI, I_SDBI, I_SHLC, I_SMBI, 

    I_LBR, I_LBQ, I_LBNQ, I_LBZ, I_LBNZ, I_LBDF, I_LBNF, 
    I_NOP, I_LSKP, I_LSIE, I_LSQ, I_LSNQ, I_LSZ, I_LSNZ, I_LSNF, I_LSDF, 

    I_GLO, 
    I_GHI, 
    I_PLO, 
    I_PHI, 
    I_SEP, 
    I_SEX, 

    I_LDX, I_OR, I_AND, I_XOR, I_ADD, I_SD, I_SHR, I_SM, 
    I_LDI, I_ORI, I_ANI, I_XRI, I_ADI, I_SDI, I_SHL, I_SMI 
};

cpu_instr_enum op_to_instr[] = {
    I_IDL, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, 
      I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, I_LDN, 
    I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, 
      I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, I_INC, 
    I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, 
      I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, I_DEC, 
    I_BR, I_BQ, I_BZ, I_BDF, I_B_EF, I_B_EF, I_B_EF, I_B_EF, 
      I_NBR, I_BNQ, I_BNZ, I_BNF, I_BN_EF, I_BN_EF, I_BN_EF, I_BN_EF, 
    I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, 
      I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, I_LDA, 
    I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, 
      I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, I_STR, 
    I_IRX, I_OUTN, I_OUTN, I_OUTN, I_OUTN, I_OUTN, I_OUTN, I_OUTN, 
      I_IDL, // undefined instruction 68 
      I_INPN, I_INPN, I_INPN, I_INPN, I_INPN, I_INPN, I_INPN, 
    I_RET, I_DIS, I_LDXA, I_STXD, I_ADC, I_SDB, I_SHRC, I_SMB, 
      I_SAV, I_MARK, I_REQ, I_SEQ, I_ADCI, I_SDBI, I_SHLC, I_SMBI, 
    I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, 
      I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, I_GLO, 
    I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, 
      I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, I_GHI, 
    I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, 
      I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, I_PLO, 
    I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, 
      I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, I_PHI, 
    I_LBR, I_LBQ, I_LBZ, I_LBDF, I_NOP, I_LSNQ, I_LSNZ, I_LSNF, 
      I_LSKP, I_LBNQ, I_LBNZ, I_LBNF, I_LSIE, I_LSQ, I_LSZ, I_LSDF, 
    I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, 
      I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, I_SEP, 
    I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, 
      I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, I_SEX, 
    I_LDX, I_OR, I_AND, I_XOR, I_ADD, I_SD, I_SHR, I_SM, 
      I_LDI, I_ORI, I_ANI, I_XRI, I_ADI, I_SDI, I_SHL, I_SMI 
};

cpu_instr_enum emu_instr;

#define BYTES_PER_LINE 8
#define DISPLAY_LINES 128

CYCLE_COUNT_DEF

void emu_reset();
void emu_loop();
void download_data();

extern uint8_t color_ram[];
extern bool is_hires_color;

void write_to_memory(uint16_t addr, uint8_t data) {
    uint16_t tmp_addr;
    if ((addr & 0xF000) == 0xD000) {
        color_ram[addr & 0xFF] = (data & 7);
        is_hires_color = true;
    } else if ((addr & 0xF000) == 0xC000) {
        color_ram[addr & 0xE7] = (data & 7);
        is_hires_color = false;
    } else {
        tmp_addr = WRITE_ADDR(addr);
        if (HAS_RAM(tmp_addr)) 
            memory[tmp_addr] = data;
    }
}

int main() {
    video_on = false;

    // hard reset
    for(int i = 0; i < 16; i++)
        registers[i] = 0;
    for(int i = 0; i < RAM_SIZE; i++) 
        memory[i] = 0;
    for(int i = RAM_SIZE; i < 0x10000; i++) 
        memory[i] = 0xff; // seems to be pulled-up
    registers[0] = 0; // for safety in DMA

    init_devices();
    emu_reset();
    emu_start_clock();
    emu_loop();

    return 0;
}


bool video_irq;
bool irq_ready;
uint8_t key_code;

void emu_reset() {
    int8_t mode;

    if (video_on) {
        stop_display();
        video_on = false;
    }

    reset_timer_dma(); 
    set_tone(0);

    register_p = 0; 
    register_x = 0; 
    register_d = 0;
    register_df = 0;
    registers[0] = 0;
    modify_msb = true; // for running from ROM
    register_q = false;
    register_ie = true;
    irq_ready = true;
    cpu_state = S0;
    key_code = 0;

    reset_bg_color();
    for(int i = 0; i < 256; i++)
        color_ram[i] = 7;

    for(int i = 0; i < 0x200; i++) 
        memory[0x8000 + i] = rom_data[i];

    mode = get_hexkey();

    if (mode < 0 || mode == 0xc) // no preloading
        return; 
    do {
        sleep_ms(10);
    } while (get_hexkey() == mode);

    switch(mode) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 0xa:
        case 0xb:
            for(int i = 0; i < 0x1000; i++) 
                memory[i] = ram_data[0x1000 * mode + i];
            sleep_ms(10);
            break;
        case 0xf: // download data
            download_data(); // never returns
            break;
        default: 
            break;
    }
}

bool in_dma = false;
int dma_count = 0;

void __not_in_flash_func(emu_loop)() {
    uint8_t buf;
    uint16_t tmp_addr, tmp_data;
    bool tmp_cond;

    while(1) {
        if (BUTTON_PRESSED) {
            stop_display();
            emu_stop_clock();
            do {
                sleep_ms(10);
            } while(BUTTON_PRESSED);
            emu_reset();
            emu_start_clock();
        }

        WAIT_EMU_CLOCK

        switch(cpu_state) {
            case S0: // Fetch
                if (VIDEO_DMA) {
/*
if (!in_dma) {
dma_count++;
if (dma_count % 1024 == 0 && dma_count < 10240)
printf("dma no %d, us %d\n", dma_count, time_us_32());
in_dma = true;
}
*/
//                    cpu_state = S2;
                    break;
                }
// in_dma = false;
CYCLE_COUNT_ADD
                video_irq = VIDEO_IRQ;
                if (irq_ready) {
                    if (register_ie && video_irq) {
                        // S3 cycle
                        irq_ready = false;
                        register_tt = ((register_x << 4) | register_p);
                        register_ie = false;
                        register_x = 2;
                        register_p = 1;
                        cpu_state = S0;
CYCLE_COUNT_INIT
                        break; // next cycle
                    } 
                } else {
                    irq_ready = !video_irq;
                }

                buf = GET_MEMORY(registers[register_p]);
                emu_instr = op_to_instr[buf];
                register_n = (buf & 0x0f);
                registers[register_p]++;

                cpu_state = S1;
                break;
            case S_IDL: // repeat fetch
                if (VIDEO_DMA) {
                    cpu_state = S0;
                    break;
                }
CYCLE_COUNT_ADD
                video_irq = VIDEO_IRQ;
                if (irq_ready) {
                    if (register_ie && video_irq) {
/* should go to S0? 
                        // S3 cycle
                        irq_ready = false;
                        register_tt = ((register_x << 4) | register_p);
                        register_ie = false;
                        register_x = 2;
                        register_p = 1;
*/
                        cpu_state = S0;
CYCLE_COUNT_INIT
                        break; // next cycle
                    } 
                } else {
                    irq_ready = !video_irq;
                }
                break;
            case S1: // Execute
CYCLE_COUNT_ADD
                switch(emu_instr) {
                    case I_IDL: 
                        // bus should be M(R(0))
                        cpu_state = S_IDL;
                        break;
                    case I_LDN: 
                        register_d = GET_MEMORY(registers[register_n]);
                        cpu_state = S0;
                        break;
                    case I_INC:
                        registers[register_n]++;
                        cpu_state = S0;
                        break;
                    case I_DEC:
                        registers[register_n]--;
                        cpu_state = S0;
                        break;
                    case I_BR: 
                        registers[register_p] = 
                            ((registers[register_p] & 0xff00) | 
                                GET_MEMORY(registers[register_p]));
                        cpu_state = S0;
                        break;
                    case I_NBR: 
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_BQ:
                        if (register_q) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BNQ:
                        if (!register_q) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BZ:
                        if (register_d == 0) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BNZ:
                        if (register_d != 0) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BDF:
                        if (register_df == 1) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BNF:
                        if (register_df == 0) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_B_EF:
                        switch(register_n - 4) {
                            case 0: 
                                tmp_cond = SIG_EF1;
                                break;
                            case 1: 
                                tmp_cond = SIG_EF2;
                                break;
                            case 2: 
                                switch(key_code) {
                                    case 1: 
                                    case 4: 
                                    case 7: 
                                    case 0xa: 
                                        tmp_cond = (gpio_get(KEYIN_1_GPIO) != 0);
                                        break;
                                    case 2: 
                                    case 5: 
                                    case 8: 
                                    case 0: 
                                        tmp_cond = (gpio_get(KEYIN_2_GPIO) != 0);
                                        break;
                                    case 3: 
                                    case 6: 
                                    case 9: 
                                    case 0xb: 
                                        tmp_cond = (gpio_get(KEYIN_3_GPIO) != 0);
                                        break;
                                    case 0xc: 
                                    case 0xd: 
                                    case 0xe: 
                                    case 0xf: 
                                        tmp_cond = (gpio_get(KEYIN_4_GPIO) != 0);
                                        break;
                                }
                                break;
                            case 3: 
                                tmp_cond = SIG_EF4;
                                break;
                        }
                        if (tmp_cond) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_BN_EF:
                        switch(register_n - 0x0c) {
                            case 0: 
                                tmp_cond = !SIG_EF1;
                                break;
                            case 1: 
                                tmp_cond = !SIG_EF2;
                                break;
                            case 2: 
                                switch(key_code) {
                                    case 1: 
                                    case 4: 
                                    case 7: 
                                    case 0xa: 
                                        tmp_cond = (gpio_get(KEYIN_1_GPIO) == 0);
                                        break;
                                    case 2: 
                                    case 5: 
                                    case 8: 
                                    case 0: 
                                        tmp_cond = (gpio_get(KEYIN_2_GPIO) == 0);
                                        break;
                                    case 3: 
                                    case 6: 
                                    case 9: 
                                    case 0xb: 
                                        tmp_cond = (gpio_get(KEYIN_3_GPIO) == 0);
                                        break;
                                    case 0xc: 
                                    case 0xd: 
                                    case 0xe: 
                                    case 0xf: 
                                        tmp_cond = (gpio_get(KEYIN_4_GPIO) == 0);
                                        break;
                                }
                                break;
                            case 3: 
                                tmp_cond = !SIG_EF4;
                                break;
                        }
                        if (tmp_cond) { 
                            registers[register_p] = 
                                ((registers[register_p] & 0xff00) | 
                                    GET_MEMORY(registers[register_p]));
                        } else {
                            registers[register_p]++;
                        }
                        cpu_state = S0;
                        break;
                    case I_LBR: 
                        buf = GET_MEMORY(registers[register_p]);
                        registers[register_p]++;
                        cpu_state = S1_2_1;
                        break;
                    case I_NOP: 
                        cpu_state = S1_2_NOP;
                        break;
                    case I_LBQ:
                        if (register_q) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;
                    case I_LBNQ:
                        if (!register_q) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;
                    case I_LBZ:
                        if (register_d == 0) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;
                    case I_LBNZ:
                        if (register_d != 0) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;
                    case I_LBDF:
                        if (register_df == 1) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;
                    case I_LBNF:
                        if (register_df == 0) { 
                            buf = GET_MEMORY(registers[register_p]);
                            cpu_state = S1_2_1;
                        } else {
                            cpu_state = S1_2_0;
                        }
                        registers[register_p]++;
                        break;

                    case I_LSKP: 
                        registers[register_p]++;
                        cpu_state = S1_2_0;
                        break;
                    case I_LSIE:
                        if (register_ie) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSQ:
                        if (register_q) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSNQ:
                        if (!register_q) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSZ:
                        if (register_d == 0) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSNZ:
                        if (register_d != 0) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSDF:
                        if (register_df == 1) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;
                    case I_LSNF:
                        if (register_df == 0) { 
                            registers[register_p]++;
                            cpu_state = S1_2_0;
                        } else {
                            cpu_state = S1_2_NOP;
                        }
                        break;

                    case I_IRX: 
                        registers[register_x]++;
                        cpu_state = S0;
                        break;
                    case I_OUTN: 
                        switch(register_n) {
                            case 1:
                                stop_display();
                                // bus should be GET_MEMORY(registers[register_x])
                                break;
                            case 2:
                                // hex key
                                key_code = (GET_MEMORY(registers[register_x]) & 0x0f);
                                switch(key_code) {
                                    case 1:
                                    case 2:
                                    case 3:
                                    case 0xc:
                                        gpio_put(KEYOUT_1_GPIO, 1);
                                        gpio_put(KEYOUT_2_GPIO, 0);
                                        gpio_put(KEYOUT_3_GPIO, 0);
                                        gpio_put(KEYOUT_4_GPIO, 0);
                                        break;                                  
                                    case 4:
                                    case 5:
                                    case 6:
                                    case 0xd:
                                        gpio_put(KEYOUT_1_GPIO, 0);
                                        gpio_put(KEYOUT_2_GPIO, 1);
                                        gpio_put(KEYOUT_3_GPIO, 0);
                                        gpio_put(KEYOUT_4_GPIO, 0);
                                        break;                                  
                                    case 7:
                                    case 8:
                                    case 9:
                                    case 0xe:
                                        gpio_put(KEYOUT_1_GPIO, 0);
                                        gpio_put(KEYOUT_2_GPIO, 0);
                                        gpio_put(KEYOUT_3_GPIO, 1);
                                        gpio_put(KEYOUT_4_GPIO, 0);
                                        break;                                  
                                    case 0xa:
                                    case 0:
                                    case 0xb:
                                    case 0xf:
                                        gpio_put(KEYOUT_1_GPIO, 0);
                                        gpio_put(KEYOUT_2_GPIO, 0);
                                        gpio_put(KEYOUT_3_GPIO, 0);
                                        gpio_put(KEYOUT_4_GPIO, 1);
                                        break;                                  
                                }
                                break;
                            case 3: // output port: Simple sound board
                                set_tone(GET_MEMORY(registers[register_x]));
                                break;
                            case 4:
                                modify_msb = false; // switch to normal address
                                break;
                            case 5:
                                switch_bg_color();
                                break;
                        // TODO
                            default:
                                break;
                        }
                        registers[register_x]++;
                        cpu_state = S0;
                        break;
                    case I_INPN: 
                        switch(register_n - 8) {
                            case 1:
                                start_display();
                                buf = 0xff;
                                break;
                        // TODO
                            default:
                                break;
                        }
                        register_d = buf;
                        write_to_memory(registers[register_x], buf);
                        cpu_state = S0;
                        break;

                    case I_RET: 
                        buf = GET_MEMORY(registers[register_x]);
                        registers[register_x]++;
                        register_x = (buf >> 4);
                        register_p = (buf & 0x0f);
                        register_ie = true;
                        cpu_state = S0;
                        break;
                    case I_DIS: 
                        buf = GET_MEMORY(registers[register_x]);
                        registers[register_x]++;
                        register_x = (buf >> 4);
                        register_p = (buf & 0x0f);
                        register_ie = false;
                        cpu_state = S0;
                        break;
                    
                    case I_LDA:
                        register_d = GET_MEMORY(registers[register_n]);
                        registers[register_n]++;
                        cpu_state = S0;
                        break;
                    case I_LDXA:
                        register_d = GET_MEMORY(registers[register_x]);
                        registers[register_x]++;
                        cpu_state = S0;
                        break;
                    case I_LDX:
                        register_d = GET_MEMORY(registers[register_x]);
                        cpu_state = S0;
                        break;
                    case I_LDI:
                        register_d = GET_MEMORY(registers[register_p]);
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_STR:
                        write_to_memory(registers[register_n], register_d);
                        cpu_state = S0;
                        break;
                    case I_STXD:
                        write_to_memory(registers[register_x], register_d);
                        registers[register_x]--;
                        cpu_state = S0;
                        break;
                    case I_ADD:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) buf) + ((uint16_t) register_d);
                        register_d = (tmp_data & 0xff); 
                        register_df = (tmp_data >> 8);
                        cpu_state = S0;
                        break;
                    case I_ADI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) buf) + ((uint16_t) register_d);
                        register_d = (tmp_data & 0xff); 
                        register_df = (tmp_data >> 8);
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_ADC:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) buf) + ((uint16_t) register_d) + ((uint16_t) register_df);
                        register_d = (tmp_data & 0xff); 
                        register_df = (tmp_data >> 8);
                        cpu_state = S0;
                        break;
                    case I_ADCI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) buf) + ((uint16_t) register_d) + ((uint16_t) register_df);
                        register_d = (tmp_data & 0xff); 
                        register_df = (tmp_data >> 8);
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_SD:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) buf) - ((uint16_t) register_d);
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        cpu_state = S0;
                        break;
                    case I_SDI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) buf) - ((uint16_t) register_d);
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_SDB:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) buf) - ((uint16_t) register_d) - (1 - ((uint16_t) register_df));
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        cpu_state = S0;
                        break;
                    case I_SDBI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) buf) - ((uint16_t) register_d) - (1 - ((uint16_t) register_df));
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_SM:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) register_d) - ((uint16_t) buf);
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        cpu_state = S0;
                        break;
                    case I_SMI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) register_d) - ((uint16_t) buf);
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_SMB:
                        buf = GET_MEMORY(registers[register_x]);
                        tmp_data = 
                          ((uint16_t) register_d) - ((uint16_t) buf) - (1 - ((uint16_t) register_df));
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        cpu_state = S0;
                        break;
                    case I_SMBI:
                        buf = GET_MEMORY(registers[register_p]);
                        tmp_data = 
                          ((uint16_t) register_d) - ((uint16_t) buf) - (1 - ((uint16_t) register_df));
                        register_d = (tmp_data & 0xff); 
                        register_df = ((tmp_data & 0x100) == 0) ? 1 : 0;
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_OR:
                        buf = GET_MEMORY(registers[register_x]);
                        register_d = (register_d | buf); 
                        cpu_state = S0;
                        break;
                    case I_ORI:
                        buf = GET_MEMORY(registers[register_p]);
                        register_d = (register_d | buf); 
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_AND:
                        buf = GET_MEMORY(registers[register_x]);
                        register_d = (register_d & buf); 
                        cpu_state = S0;
                        break;
                    case I_ANI:
                        buf = GET_MEMORY(registers[register_p]);
                        register_d = (register_d & buf); 
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_XOR:
                        buf = GET_MEMORY(registers[register_x]);
                        register_d = (register_d ^ buf); 
                        cpu_state = S0;
                        break;
                    case I_XRI:
                        buf = GET_MEMORY(registers[register_p]);
                        register_d = (register_d ^ buf); 
                        registers[register_p]++;
                        cpu_state = S0;
                        break;
                    case I_SHRC: 
                        tmp_data = ((((uint16_t) register_df) << 8) | (uint16_t) register_d);
                        register_df = (tmp_data & 1);
                        register_d = (tmp_data >> 1);
                        cpu_state = S0;
                        break;
                    case I_SHR: 
                        register_df = (register_d & 1);
                        register_d = (register_d >> 1);
                        cpu_state = S0;
                        break;
                    case I_SHLC: 
                        tmp_data = ((((uint16_t) register_d) << 1) | (uint16_t) register_df);
                        register_df = (tmp_data & 0x100) ? 1 : 0;
                        register_d = (tmp_data & 0xff);
                        cpu_state = S0;
                        break;
                    case I_SHL: 
                        register_df = (register_d & 0x80) ? 1 : 0;
                        register_d = (register_d << 1);
                        cpu_state = S0;
                        break;
                    case I_SAV:
                        write_to_memory(registers[register_x], register_tt);
                        cpu_state = S0;
                        break;
                    case I_MARK:
                        register_tt = ((register_x << 4) | register_p);
                        write_to_memory(registers[2], register_tt);
                        register_x = register_p;
                        registers[2]--; 
                        cpu_state = S0;
                        break;
                    case I_REQ: 
                        register_q = false;
                        gpio_put(OUT_Q_GPIO, false);
                        pwm_set_enabled(beep_timer_slice, false);
                        cpu_state = S0;
                        break;
                    case I_SEQ: 
                        register_q = true;
                        gpio_put(OUT_Q_GPIO, true);
                        pwm_set_enabled(beep_timer_slice, true);
                        cpu_state = S0;
                        break;
                    case I_GLO:
                        register_d = (uint8_t) registers[register_n];
                        cpu_state = S0;
                        break;
                    case I_GHI:
                        register_d = (registers[register_n] >> 8);
                        cpu_state = S0;
                        break;
                    case I_PLO:
                        registers[register_n] = 
                          ((registers[register_n] & 0xff00) | register_d);
                        cpu_state = S0;
                        break;
                    case I_PHI:
                        registers[register_n] = 
                          ((registers[register_n] & 0x00ff) | (((uint16_t) register_d) << 8));
                        cpu_state = S0;
                        break;
                    case I_SEP: 
                        register_p = register_n;
                        cpu_state = S0;
                        break;
                    case I_SEX: 
                        register_x = register_n;
                        cpu_state = S0;
                        break;
                }
                break;
            case S1_2_0: // last cycle of long jump, jump not taken, skip
CYCLE_COUNT_ADD
                registers[register_p]++;
                cpu_state = S0;
                break;
            case S1_2_1: // last cycle of long jump, jump taken
CYCLE_COUNT_ADD
                registers[register_p] = 
                    ((buf << 8) | GET_MEMORY(registers[register_p]));
                cpu_state = S0;
                break;
            case S1_2_NOP: // last cycle of NOP, skip(not taken)
CYCLE_COUNT_ADD
                cpu_state = S0;
                break;
            case S2: // DMA, but not processed here
CYCLE_COUNT_ADD
                break;
            case S3: // IRQ, but not processed here
CYCLE_COUNT_ADD
                break;

        }
    }
}

void download_data() {
    int c, bytes, sector, base;
    uint8_t buf[4096];

    multicore_reset_core1();
    stdio_init_all();

    while(1) {
        c = getchar();
        bytes = 0;

        if (c == 'h') {
            printf("  r: write 512 bytes of data for ROM\n  0 to 9, a, b: write 4096 bytes of data for RAM\n  d: send RAM contents to PC\n");
        }
        if (c >= '0' && c <= '9') {
            bytes = 4096;
            sector = c - '0';
        }
        if (c >= 'a' && c <= 'b') {
            bytes = 4096;
            sector = c - 'a' + 10;
        }
        if (c == 'r') {
            bytes = 512;
        }

        if (c == 'd') { // transfer RAM to PC
            printf("Press a key to transfer RAM to PC.\n");
            getchar();
            for(int i = 0; i < 0x1000; i++) {
                putchar_raw(memory[i]);
            }
            continue;
        }

        if (bytes > 0) {
            if (c == 'r') {
                printf("Receiving data for ROM...\n");
            } else {
                printf("Receiving data for RAM, no %d...\n", sector);
            }
            for(int i = 0; i < bytes; i++) {
                buf[i] = getchar();
            }
            if (c == 'r') {
                base = (int) rom_data - XIP_BASE;
            } else {
                base = (int) ram_data - XIP_BASE + sector * 0x1000;
            }
            printf("Writing %d bytes, offset %d...\n", bytes, base);
            sleep_ms(100);

// From https://dominicmaas.co.nz/pages/pico-filesystem-littlefs
            uint32_t ints = save_and_disable_interrupts(); 
            flash_range_erase(base, 4096);
            flash_range_program(base, buf, bytes);
            restore_interrupts(ints);
            printf("Ok\n\n");
        }
        sleep_ms(10);
    }    
}
