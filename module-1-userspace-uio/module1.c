#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdbool.h>

#define SWITCH_UIO_DEVICE "/dev/uio1"
#define LED_UIO_DEVICE  "/dev/uio0"
#define MAP_SIZE        0x10000
#define SW0_MASK        0x1
#define LD0_MASK        0x1

int main()
{
    int switch_fd, led_fd;
    void * led_mapped_address;    // mmap() returns a void pointer; cast it when accessing the registers
    void * switch_mapped_address; // mmap() returns a void pointer; cast it when accessing the registers
    volatile uint32_t * switch_registers;
    volatile uint32_t * led_registers;
    uint32_t led_value;
    
    bool previous_state, current_state;
    // Open and map the switch UIO device
    switch_fd = open(SWITCH_UIO_DEVICE, O_RDWR | O_SYNC);
    if (switch_fd < 0) {
        perror("Failed to open /dev/uio1");
        return 1;
    }
    switch_mapped_address = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, switch_fd, 0);
    if (switch_mapped_address == MAP_FAILED) {
            perror("Failed to open");
            close(switch_fd);
            return 1;
    }

    // Open and map the LED UIO device
    led_fd = open(LED_UIO_DEVICE, O_RDWR | O_SYNC);
    if (led_fd < 0) {
        perror("Failed to open /dev/uio0");
        close(switch_fd);
        return 1;
    }
    led_mapped_address = mmap(NULL, MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, led_fd, 0);
    if (led_mapped_address == MAP_FAILED) {
        perror("Failed to open");
        close(led_fd);
        return 1;
    }

    switch_registers = (volatile uint32_t *)switch_mapped_address;
    led_registers = (volatile uint32_t *)led_mapped_address;

    // Configure the LD0 TRI bit as output 
    // 000x ---> 0000
    led_registers[1] &= ~LD0_MASK;

    // Configure the SW0 TRI bit as input
    // 000x ---> 0001
    switch_registers[1] |= SW0_MASK;


    // If SW0 is on, previous_state is true; otherwise it is false
    previous_state = (switch_registers[0] & SW0_MASK) != 0;

    printf("LD0 is being controlled rn\n");

    while(true){
        // If SW0 is on, current_state is true; otherwise it is false
        current_state = (switch_registers[0] & SW0_MASK) != 0;
        led_value = led_registers[0];

        if(current_state) {
            led_value = led_value | LD0_MASK;
        }
        else {
            led_value = led_value & ~LD0_MASK;
        }
        led_registers[0] = led_value;

        if (!previous_state && current_state) {
            printf("Hello World from SW0\n");
        }

        previous_state = current_state;
        usleep(100000); // Equivalent to 0.1 seconds
    }

    munmap(switch_mapped_address, MAP_SIZE);
    munmap(led_mapped_address, MAP_SIZE);

    close(led_fd);
    close(switch_fd);
    
    return 0;
}