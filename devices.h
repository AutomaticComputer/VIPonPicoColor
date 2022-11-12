void init_devices();
void start_display(); 
void stop_display();
void switch_bg_color();
void reset_bg_color();
void emu_start_clock();
void emu_stop_clock();
void reset_timer_dma(); 
int8_t get_hexkey();
void set_tone(uint16_t);

extern uint sys_timer_slice, csync_timer_slice, video_dma_timer_slice, beep_timer_slice, 
    beep_timer_channel;

#define RAM_SIZE 0x1000
#define RAM_ADDRESS_MASK 0x0FFF

#define CLOCKS_PER_CYCLE 8
#define CYCLES_PER_LINE 14

#define VIDEO_CHIP_DELAY 20

#define SYNC_PRESCALE (CLOCKS_1802_DIV)
#define SYNC_PERIODS 262
#define SYNC_HOR (14 * CLOCKS_PER_CYCLE) // one scan line
#define SYNC_TOP (SYNC_HOR - 1) 
#define SYNC_HS (CLOCKS_PER_CYCLE) // horizontal sync pulse
#define SYNC_VS (SYNC_HOR - SYNC_HS) // latter half of vertical sync line
#define SYNC_DISP (1u << 16)
#define SYNC_IRQ (1u << 17)

// SP: for Signal period
#define SP_PRESCALE SYNC_PRESCALE
#define SP_SIZE 128
#define SP_DMA_START (3 * CLOCKS_PER_CYCLE)
#define SP_DMA_MEMCPY (4 * CLOCKS_PER_CYCLE)
#define SP_SPI_START (5 * CLOCKS_PER_CYCLE)
#define SP_DMA_END (11 * CLOCKS_PER_CYCLE)
#define SP_HOR SYNC_HOR
#define SP_TOP (SP_HOR - 1)
#define SP_INITIAL (SP_HOR * 79 + SP_DMA_START + SYNC_TOP)
#define SP_FINAL (SP_HOR * 135 - 1)

#define CSYNC_GPIO 4 // pin 6
#define VIDEO_R_GPIO 5 // pin 7
#define VIDEO_B_GPIO 6 // pin 9
#define VIDEO_G_GPIO 7 // pin 10
#define VIDEO_DC0_GPIO 8 // pin 11
#define VIDEO_DC1_GPIO 9 // pin 12

#define BEEP_GPIO 10 // pin 14
#define OUT_Q_GPIO 11 // pin 15
#define IN_EF2_GPIO 12 // pin 16, casset tape in, inverted
#define BUTTON_GPIO 13 // pin 17, reset/run button

#define DEBUG0_GPIO 14
#define DEBUG1_GPIO 15

#define KEYOUT_1_GPIO 16 // pin 21
#define KEYOUT_2_GPIO 17 // pin 22
#define KEYOUT_3_GPIO 18 // pin 24
#define KEYOUT_4_GPIO 19 // pin 25
#define KEYIN_1_GPIO 20 // pin 26
#define KEYIN_2_GPIO 21 // pin 27
#define KEYIN_3_GPIO 22 // pin 29
#define KEYIN_4_GPIO 26 // pin 31


#define SIG_EF1 ((pwm_hw->slice[csync_timer_slice].cc & SYNC_DISP) != 0)
#define SIG_EF2 (gpio_get(IN_EF2_GPIO) == 0) // inverted
#define SIG_EF3 (false)
#define SIG_EF4 (false)
#define VIDEO_IRQ ((pwm_hw->slice[csync_timer_slice].cc & SYNC_IRQ) != 0) 
#define VIDEO_DMA (video_on && (pwm_get_counter(video_dma_timer_slice) < SP_DMA_END - SP_DMA_START))
#define WAIT_EMU_CLOCK {while(!(pwm_hw->intr & (1u << sys_timer_slice))); pwm_hw->intr = (1u << sys_timer_slice);}
#define BUTTON_PRESSED (gpio_get(BUTTON_GPIO))
