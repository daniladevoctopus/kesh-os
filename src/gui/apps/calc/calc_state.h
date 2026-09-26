// стейт калькулятора
#ifndef CALC_STATE_H
#define CALC_STATE_H

#include <stdint.h>

typedef struct {
    char display[24];    
    uint32_t disp_len;
    int64_t accumulator;  
    int64_t memory;        
    char pending_op;       
    int start_new_entry;   
    int div_by_zero;       

    char last_pressed_key;
    int press_flash_t;
} calc_state_t;

void calc_init(calc_state_t *c);
void calc_feed_char(calc_state_t *c, char key);
void calc_backspace(calc_state_t *c);

#endif
