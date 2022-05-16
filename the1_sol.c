/*
 * File:   20212_the1.c
 * Author: Uluc Saranli
 *
 * This file is an example C solution for the CENG 336 THE1 within the 
 * Spring 2022 semester. It illustrates modular design principles using
 * various tasks and their data elements within a Round-Robin style
 * architecture. Note that this implementation will not necessarily pass
 * the grading scripts as-is, but should be able to do so with minor
 * changes and handling of edge-cases.
 * 
 * Created on April 8, 2022, 12:05 PM
 */

#include <xc.h>
#include <stdint.h>

// ************* Utility functions ****************
void init_ports() {
    TRISA = 0xff;  // RA4 is used to configure values
    TRISB = 0xf0;  // 0-3 are LEDs
    TRISC = 0xfC;  // 0-1 are LEDs
    TRISD = 0x00;  // 0-7 are LEDs
    TRISE = 0xff;  // RE4 is port select
}

// These are different counter limits to obtain 500ms delay without a busy
// wait. Different limits are needed since different parts of various tasks
// involve different numbers of instruction, changing the timing.
#define INIT_WAIT_500MS 45000
#define BLINK_WAIT_500MS 37000
#define CDOWN_WAIT_500MS 42000

// ************* Timer task and functions ****************
// The "timer" task is responsible from maintaining  counter which, 
// once started, counts up to a maximum count, and repeats this "ticks"
// number of times before declaring itself to be "done". The timer can be
// in one of three states: IDLE, RUN, DONE.
typedef enum {TMR_IDLE, TMR_RUN, TMR_DONE} tmr_state_t;
tmr_state_t tmr_state = TMR_IDLE;   // Current timer state
uint16_t tmr_cntmax_500ms = 40000;  // Timer ends when counter reaches this.
uint8_t tmr_startreq = 0;           // Flag to request the timer to start
uint8_t tmr_ticks_left;             // Number of "ticks" until "done"

// This function resets and starts the timer with the given max counter 
// and ticks. The total time waited is ticks*cntmax, after which the timer
// goes into the DONE state
void tmr_start(uint8_t ticks, uint16_t cntmax) {
    tmr_ticks_left = ticks;
    tmr_cntmax_500ms = cntmax;
    tmr_startreq = 1;
    tmr_state = TMR_IDLE;
}
// This function aborts the current timer run and goes back to IDLE
void tmr_abort() {
    tmr_startreq = 0;
    tmr_state = TMR_IDLE;
}

// This is the timer task
void timer_task() {
    static uint16_t tmr_count = 0;  // Current timer count, static local var.
    switch (tmr_state) {
        case TMR_IDLE:
            if (tmr_startreq) {
                // If a start request has been issued, go to the RUN state
                tmr_startreq = 0; tmr_count = 0;
                tmr_state = TMR_RUN;
            }
            break;
        case TMR_RUN:
            // Timer remains in the RUN state until the counter reaches its max
            // "ticks" number of times.
            if (tmr_count++ >= tmr_cntmax_500ms) {
                if (--tmr_ticks_left == 0) 
                    tmr_state = TMR_DONE;
                else 
                    tmr_count = 0;
            }
            break;
        case TMR_DONE:
            // State waits here until tmr_start() or tmr_abort() is called
            break;
    }
}

// ************* Input task and functions ****************
// The "input task" monitors RA4 and RE4 and increments associated counters 
// whenever a high pulse is observed (i.e. HIGH followed by a LOW).
uint8_t inp_config_cnt = 0; // Current count for CONFIGURE input(i.e. RA4)
uint8_t inp_port_cnt = 0;   // Current count for PORT SELECT input (i.e. RE4)
uint8_t inp_config_btn_st = 0, inp_port_btn_st = 0;

// This function resets the counter for PORT SELECT input 
void inp_port_reset() { inp_port_cnt = 0; }
// This function resets the counter for CONFIGURE input
void inp_config_reset() { inp_config_cnt = 0; }

// This is the input task function
void input_task() {
    if (PORTEbits.RE4) inp_port_btn_st = 1;
    else if (inp_port_btn_st == 1) {
        // A high pulse has been observed on the PORT input
        inp_port_btn_st = 0;
        inp_port_cnt++;
    }
    if (PORTAbits.RA4) inp_config_btn_st = 1;
    else if (inp_config_btn_st == 1) {
        // A high pulse has been observed on the CONFIGURE input
        inp_config_btn_st = 0;
        inp_config_cnt++;
    }
}

// ************* Display task and functions ****************
// This is the "display task", which is responsible from maintaining and 
// updating outputs on PORTB, PORTC and PORTD. This task handles all
// blinking functionality when configured by using the timer task.

// Current expected states of output ports
uint8_t dsp_portb = 0x01, dsp_portc = 0x01, dsp_portd = 0x00;
// Current blink configuration 0: no blink, 1: blink PORTB, 2: blink PORTC
uint8_t dsp_blink = 0;
// Display state for all ports. This is used to implement blinking
// 0: display turned on, 1: display turned off (blink)
uint8_t dsp_off = 0;
// This flag indicates that the actual port outputs should be updated on next 
// iteration
uint8_t dsp_updatereq = 1;

// This function sets the current states for "level" (PORTB), action (PORTC)
// and count (PORTD). Level is 0,1,2 or 3, action is 0 (attack) or 1 (defend)
// and 0 <= count < 8 is the countdown start value. Port bit masks are
// computed accordingly here.
void dsp_set_state(uint8_t level, uint8_t action, uint8_t count ) {
    dsp_portb = (uint8_t) ((0x01 << level) -  0x01);
    dsp_portc = (action == 0)?0x01:0x02;
    dsp_portd = (uint8_t) ((count==0)?0x00:(((0x01<<(count-1))-0x01)<<1)|0x01);
    dsp_updatereq = 1;
}
// This function sets the current blink state as defined above
// 0: no blink, 1: blink PORTB, 2: blink PORTC
void dsp_set_blink( uint8_t blink ) {
    dsp_blink = blink; dsp_off = 0;
    dsp_updatereq = 1;
}
// This function updates the actual port outputs based on the blink on/off
// state and the computed port mask values.
void dsp_update_ports() {
    PORTB = (dsp_off & (dsp_blink == 1))?0x00:((PORTB & 0xf0) | dsp_portb);
    PORTC = (dsp_off & (dsp_blink == 2))?0x00:((PORTC & 0xFC) | dsp_portc);
    PORTD = dsp_portd;
}
// This function turns on all output LEDs on all ports. Used during the first
// 1s initialization stage.
void dsp_allportson() {
    PORTB = 0x0f;
    PORTC = 0x03;
    PORTD = 0xff;
    dsp_updatereq = 0;
}

// This is the display task function
void display_task() {
    if (dsp_updatereq) {
        // If an update request has been sent, update actual port values and 
        // abort existing timers if the blink has been disabled.
        dsp_off = 0;
        dsp_update_ports();
        dsp_updatereq = 0;
        if (dsp_blink != 0) tmr_abort();
        return;
    }
    
    // If blinking is enabled, this section handles the timer and 
    // monitors for its completion to toggle on/off states
    if (dsp_blink != 0) {
        if (tmr_state == TMR_DONE) {
            // Timer is DONE, so this is probably when the previous blink
            // cycle has ended
            // been started.
            dsp_off = 1 - dsp_off;
            tmr_start(1, BLINK_WAIT_500MS);
            dsp_update_ports();
        } else if (tmr_state == TMR_IDLE) {
            // Timer is IDLE, so this is probably when blinking has just
            // been started.
            dsp_off = 0;             
            tmr_start(1, BLINK_WAIT_500MS);
            dsp_update_ports();
        }
    }
}

// ************* Game task and functions ****************
// This task handles the overall game logic and control remaining tasks 
// through their utility functions and flags

// Game state definitions and the global state
typedef enum {G_INIT,G_INIT_WAIT,G_START, G_LEVEL,G_ACTION,G_CNTDWN,G_END} game_state_t;
game_state_t game_state = G_INIT;
// Current game choices and the countdown
uint8_t game_level = 1, game_action = 0, game_count = 0;

void game_task() {
    switch (game_state) {
        case G_INIT:
            // INIT state starts a 1s timer (i.e. 2 ticks of 500ms) and
            // goes to INIT_WAIT
            dsp_set_state(game_level, game_action, game_count);
            dsp_allportson();
            tmr_start(2, INIT_WAIT_500MS);
            game_state = G_INIT_WAIT;
            break;
        case G_INIT_WAIT:
            // INIT_WAIT is done when the timer task signals the end of the 
            // requested period. At that point, the game state goes to the
            // START state
            if (tmr_state == TMR_DONE) {
                dsp_set_blink(0);
                inp_port_reset();
                inp_config_reset();
                game_state = G_START;
            }
            break;
        case G_START:
            // START is done when the user presses the PORT button. 
            // PORTB blink is enabled and game state goes to LEVEL selection
            if (inp_port_cnt != 0) {
                inp_port_reset();
                dsp_set_blink(1);
                game_state = G_LEVEL;
            }
            break;
        case G_LEVEL:
            // LEVEL state ends when the user pressed the PORT input, which 
            // is when blinking is changed to ACTION and game state goes into
            // ACTION selection.
            if (inp_port_cnt != 0) {
                inp_port_reset();
                dsp_set_blink(2);
                game_state = G_ACTION;
            }
            // If the CONFIGURE input is observed, level is increased up to
            // a maximum of 4.
            if (inp_config_cnt != 0) {
                inp_config_reset();
                if (game_level < 4) game_level++;
                else game_level = 1;
                dsp_set_state(game_level, game_action, game_count);
            }
            break;
        case G_ACTION:
            // ACTION state ends when the user pressed the PORT input, which 
            // is when blinking is disabled and game state goes into
            // the COUNTDOWN state .
            if (inp_port_cnt != 0) {
                inp_port_reset();
                dsp_set_blink(0);
                // This is where we compute the countdown amount for
                // the next state to use
                game_count = (game_action==0)?game_level:2*game_level;
                dsp_set_state(game_level, game_action, game_count);
                game_state = G_CNTDWN;
                tmr_start(1, BLINK_WAIT_500MS);
            }
            // Within the ACTION state, a CONFIGURE input toggles between
            // attack and defend actions.
            if (inp_config_cnt != 0) {
                inp_config_reset();
                game_action = 1 - game_action;
                dsp_set_state(game_level, game_action, game_count);
            }
            break;
        case G_CNTDWN:
            // The COUNTDOWN state uses the timer task to decrement the 
            // countdown once every 500ms until the countdown is finished
            // The game transitions into the END state after that.
            if (tmr_state == TMR_DONE) {
                if (--game_count != 0) {
                    dsp_set_state(game_level, game_action, game_count);
                    tmr_start(1, CDOWN_WAIT_500MS);
                } else {
                    dsp_set_state(game_level, game_action, 0);
                    tmr_start(1, CDOWN_WAIT_500MS);
                    game_state = G_END;
                }
            }
            break;
        case G_END:
            // The END state waits for another 500ms after which the
            // game restarts from the INIT state all over again.
            if (tmr_state == TMR_DONE) {
                game_level = 1; game_action = 0; game_count = 0;
                dsp_set_state(game_level, game_action, game_count);
                dsp_set_blink(0);
                inp_port_reset();
                inp_config_reset();
                game_state = G_START;
            }
            break;
    }    
}

void main(void) {
    init_ports();
    while (1) {
        timer_task();
        input_task();
        display_task();
        game_task();
    }
}
