
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <tgmath.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

//define I2C pins
#define I2C0_SDA_PIN 16
#define I2C0_SCL_PIN 17

//define LED pins
#define LED1 22
#define LED2 21
#define LED3 20

//define Buttons
#define SW_0 9
#define SW_1 8
#define SW_2 7

//EEPROME address
#define DEVADDR 0x50
#define BAUDRATE 100000
#define I2C_MEMORY_SIZE 32768

#define ENTRY_SIZE 64
#define MAX_ENTRIES 32
#define STRLEN 62
#define FIRST_ADDRESS 0

//uart pins
#if 1
#define UART_NR uart0
#define UART_TX_PIN 0
#define UART_RX_PIN 1
#else
#define UART_NR 1
#define UART_TX_PIN 4
#define UART_RX_PIN 5
#endif

#define UART_BAUD_RATE 9600

//stepper motor
#define OPTO_FORK 28
#define PIEZO_SENSOR 27
#define IN1 13
#define IN2 6
#define IN3 3
#define IN4 2

#define STR_SIZE 10
#define STEPPER_STEPS_PER_REVOLUTION 4096

const uint turning_sequence[8][4] = {{1, 0, 0, 0},
                                     {1, 1, 0, 0},
                                     {0, 1, 0, 0},
                                     {0, 1, 1, 0},
                                     {0, 0, 1, 0},
                                     {0, 0, 1, 1},
                                     {0, 0, 0, 1},
                                     {1, 0, 0, 1}};


void initializePins(void);
void calib(uint *row, int *revolution_steps, int *calib_completed);
void status(int calibrate_completed, int revolution_steps);
void runStepperMotor(uint *row, bool calibrating, int *revolution_steps, uint times);


int main() {

    initializePins();
    //for check user input
    char input_command[STR_SIZE];
    memset(input_command, 0, sizeof(input_command));
    char chr;
    int index = 0;

    printf("Boot\n");
    int calibrate_completed = 0;
    uint row = 0;
    int revolution_steps = 0;

    while (true) {

        chr = getchar_timeout_us(0);
        while (chr != 255) {
            input_command[index++] = chr;

            if (index == 7 || chr == '\n') {
                input_command[index - 1] = '\0';

                if (strncmp("run", input_command, strlen("run"))== 0) {
                    uint n = 8;
                    uint times = 0;
                    if (strlen(input_command) >= 5) {
                        sscanf(&input_command[strlen("run")], "%u", &n);
                    }
                    if(calibrate_completed){
                        times = n * revolution_steps / 8;
                    }else{
                        times = n * STEPPER_STEPS_PER_REVOLUTION / 8;
                    }
                    runStepperMotor(&row, false, &revolution_steps, times);
                } else if (strcmp("status", input_command)==0) {
//                    printf("status command\n");
                    status(calibrate_completed, revolution_steps);
                } else if (strcmp("calib", input_command)==0) {
//                    printf("Calib command\n");
                    calib(&row, &revolution_steps, &calibrate_completed);
                }

                memset(input_command, 0, sizeof(input_command));
                index = 0;
                break;
            }
            chr = getchar_timeout_us(0);
        }
    }

    return 0;
}
void status(int calibrate_completed, int revolution_steps){
    if(calibrate_completed){
        printf("%d steps/revolution\n", revolution_steps);
    }else{
        printf("Not available\n");
    }
}

void calib(uint *row, int *revolution_steps, int *calib_completed){
    int calibration_time = 0;
    int total_calibration = 0;
    bool calibrating = false;

    while(calibration_time<3){
        while(gpio_get(OPTO_FORK)){
            runStepperMotor(row, calibrating, revolution_steps,1);
        }
        printf("Found a falling edge\nStart calibrating...\n");
        sleep_ms(1500);
        calibrating=true;
        *revolution_steps = 0;
        while(!gpio_get(OPTO_FORK)){
            runStepperMotor(row, calibrating, revolution_steps, 1);
        }
//        printf("Found the rising edge, opto_state is %d\n", opto_state);
//        sleep_ms(5000);
        while(gpio_get(OPTO_FORK)){
            runStepperMotor(row, calibrating, revolution_steps, 1);
        }
        calibrating = false;

        printf("Calibration %d completed: %d steps/revolution\n", calibration_time+1, *revolution_steps);
        total_calibration += *revolution_steps;
        calibration_time++;
    }

    *revolution_steps = (total_calibration + 3-1)/3;
    printf("Average calibration after 3 revolutions: %d steps/revolution\n", *revolution_steps); //for round up the integer
    *calib_completed = 1;
}


void runStepperMotor(uint *row, bool calibrating, int *revolution_steps, uint times)
{
    for(int i = 0; i<times; i++){
        gpio_put(IN1, turning_sequence[*row][0]);
        gpio_put(IN2, turning_sequence[*row][1]);
        gpio_put(IN3, turning_sequence[*row][2]);
        gpio_put(IN4, turning_sequence[*row][3]);
        if(calibrating){
            *revolution_steps+=1;
        }
        *row+=1;
        if(*row == 8){
            *row = 0;
        }
        sleep_ms(4);
    }
}

void initializePins(void){

    // Initialize chosen serial port
    stdio_init_all();

    //Initialize I2C
    i2c_init(i2c1, BAUDRATE);
    i2c_init(i2c0, BAUDRATE);

    gpio_set_function(I2C0_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL_PIN, GPIO_FUNC_I2C);

    //Initialize led
    gpio_init(LED1);
    gpio_set_dir(LED1, GPIO_OUT);
    gpio_init(LED2);
    gpio_set_dir(LED2, GPIO_OUT);
    gpio_init(LED3);
    gpio_set_dir(LED3, GPIO_OUT);

    //Initialize buttons
    gpio_set_function(SW_0, GPIO_FUNC_SIO);
    gpio_set_dir(SW_0, GPIO_IN);
    gpio_pull_up(SW_0);

    gpio_set_function(SW_1, GPIO_FUNC_SIO);
    gpio_set_dir(SW_1, GPIO_IN);
    gpio_pull_up(SW_1);

    gpio_set_function(SW_2, GPIO_FUNC_SIO);
    gpio_set_dir(SW_2, GPIO_IN);
    gpio_pull_up(SW_2);

    //initialize stepper motor
    gpio_init(IN1);
    gpio_set_dir(IN1, GPIO_OUT);
    gpio_init(IN2);
    gpio_set_dir(IN2, GPIO_OUT);
    gpio_init(IN3);
    gpio_set_dir(IN3, GPIO_OUT);
    gpio_init(IN4);
    gpio_set_dir(IN4, GPIO_OUT);

    //Opto fork and Piezo sensor
    gpio_init(OPTO_FORK);
    gpio_set_dir(OPTO_FORK, GPIO_IN);
    gpio_pull_up(OPTO_FORK);

//    gpio_init(PIEZO_SENSOR);
//    gpio_set_dir(PIEZO_SENSOR, GPIO_IN);
//    gpio_pull_up(PIEZO_SENSOR);

}