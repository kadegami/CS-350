/*
 * Karina Washington
 * CS-499 Capstone – Milestone Three (Algorithms & Data Structures Enhancement)
 * Original Project: CS-350 – GPIO Morse Code Embedded System
 *
 * Enhancement:
 * - Introduced speedMultiplier for adjustable Morse playback speed.
 * - State machine uses structured MorseStep arrays for SOS and OK.
 * - Demonstrates algorithmic control, interrupt-driven timing, and data structure organization.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <ti/drivers/GPIO.h>
#include <ti/drivers/Timer.h>
#include "ti_drivers_config.h"

/* Base timer period = 500 ms (500,000 microseconds) */
#define TIMER_PERIOD_US    500000

/* Morse message selection */
typedef enum {
    MESSAGE_SOS,
    MESSAGE_OK
} MessageType;

/*
 * MorseStep:
 * led      - LED index (CONFIG_GPIO_LED_0 or CONFIG_GPIO_LED_1)
 * state    - true = LED ON, false = LED OFF
 * duration - number of timer ticks (each tick = 500 ms)
 */
typedef struct {
    uint_least8_t led;
    bool          state;
    int           duration;
} MorseStep;

/* SOS: ... --- ... */
MorseStep SOS_steps[] = {
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 1},
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 1},
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 3},

    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 1},
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 1},
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 3},

    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 1},
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 1},
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 3}
};

#define SOS_TOTAL_STEPS  (sizeof(SOS_steps) / sizeof(MorseStep))

/* OK: --- -.-- */
MorseStep OK_steps[] = {
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 1},
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 1},
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 3},

    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 1},
    {CONFIG_GPIO_LED_0, true, 1},
    {CONFIG_GPIO_LED_0, false, 1},
    {CONFIG_GPIO_LED_1, true, 3},
    {CONFIG_GPIO_LED_1, false, 3}
};

#define OK_TOTAL_STEPS  (sizeof(OK_steps) / sizeof(MorseStep))

/* Global state variables */
volatile MessageType currentMessage = MESSAGE_SOS;
volatile bool buttonPressed = false;

/*
 * Enhancement: Adjustable speed levels
 * 1 = normal
 * 2 = slower
 * 3 = slowest
 */
volatile uint8_t speedMultiplier = 1;

/* State machine tracking */
int currentStepIndex = 0;
int remainingTicks   = 0;

Timer_Handle timer0;

/* Forward declaration */
void runStateMachine(void);

/*
 * Timer interrupt callback (runs every 500 ms).
 */
void timerCallback(Timer_Handle handle, int_fast16_t status)
{
    runStateMachine();
}

/*
 * runStateMachine:
 * - Selects correct message (SOS or OK)
 * - Applies LED ON/OFF from the MorseStep array
 * - Scales duration based on speedMultiplier
 */
void runStateMachine(void)
{
    MorseStep *steps;
    int totalSteps;

    /* Select message */
    if (currentMessage == MESSAGE_SOS) {
        steps = SOS_steps;
        totalSteps = SOS_TOTAL_STEPS;
    } else {
        steps = OK_steps;
        totalSteps = OK_TOTAL_STEPS;
    }

    /* If finishing a step, load the next one */
    if (remainingTicks <= 0) {

        /* If there are more steps in the message */
        if (currentStepIndex < totalSteps) {

            /* Write LED state */
            GPIO_write(
                steps[currentStepIndex].led,
                steps[currentStepIndex].state ? CONFIG_GPIO_LED_ON : CONFIG_GPIO_LED_OFF
            );

            /* Apply enhanced speed scaling */
            if (speedMultiplier == 0) speedMultiplier = 1;
            remainingTicks = steps[currentStepIndex].duration * speedMultiplier;

            currentStepIndex++;

        } else {
            /* Completed entire message: reset */
            currentStepIndex = 0;

            if (buttonPressed) {
                buttonPressed = false;
                if (currentMessage == MESSAGE_SOS)
                    currentMessage = MESSAGE_OK;
                else
                    currentMessage = MESSAGE_SOS;
            }
        }
    }

    /* Consume timer tick */
    if (remainingTicks > 0) {
        remainingTicks--;
    }
}

/*
 * Button 0: toggles between SOS and OK messages.
 */
void gpioButtonFxn0(uint_least8_t index)
{
    buttonPressed = true;
}

/*
 * Button 1: cycles speed (1 → 2 → 3 → 1)
 * Enhancement feature.
 */
void gpioButtonFxn1(uint_least8_t index)
{
    if (speedMultiplier == 1)
        speedMultiplier = 2;
    else if (speedMultiplier == 2)
        speedMultiplier = 3;
    else
        speedMultiplier = 1;
}

/*
 * Initialize timer for periodic callback.
 */
void initTimer(void)
{
    Timer_Params params;
    Timer_Params_init(&params);

    params.period        = TIMER_PERIOD_US;
    params.periodUnits   = Timer_PERIOD_US;
    params.timerMode     = Timer_CONTINUOUS_CALLBACK;
    params.timerCallback = timerCallback;

    timer0 = Timer_open(CONFIG_TIMER_0, &params);
    if (timer0 == NULL) while (1);

    if (Timer_start(timer0) == Timer_STATUS_ERROR) while (1);
}

/*
 * mainThread:
 * - Initializes GPIO
 * - Configures LEDs and buttons
 * - Starts timer-driven state machine
 */
void *mainThread(void *arg0)
{
    GPIO_init();
    Timer_init();

    /* LEDs */
    GPIO_setConfig(CONFIG_GPIO_LED_0, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);
    GPIO_setConfig(CONFIG_GPIO_LED_1, GPIO_CFG_OUT_STD | GPIO_CFG_OUT_LOW);

    /* Buttons */
    GPIO_setConfig(CONFIG_GPIO_BUTTON_0, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);
    GPIO_setConfig(CONFIG_GPIO_BUTTON_1, GPIO_CFG_IN_PU | GPIO_CFG_IN_INT_FALLING);

    /* Turn on LED 0 to indicate system active */
    GPIO_write(CONFIG_GPIO_LED_0, CONFIG_GPIO_LED_ON);

    /* Install callbacks */
    GPIO_setCallback(CONFIG_GPIO_BUTTON_0, gpioButtonFxn0);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_0);

    GPIO_setCallback(CONFIG_GPIO_BUTTON_1, gpioButtonFxn1);
    GPIO_enableInt(CONFIG_GPIO_BUTTON_1);

    /* Start timer */
    initTimer();

    return (NULL);
}