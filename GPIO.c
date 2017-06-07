//
//  How to access GPIO registers from C-code on the Raspberry-Pi
//  Example program
//  15-January-2012
//  Dom and Gert
//  Revised: 15-Feb-2013

/* Rewritten by Thorbjørn and Jesper 2017-06-07 */
 
 
// Access from ARM Running Linux
 
#define BCM2708_PERI_BASE        0x3F000000
#define GPIO_BASE                (BCM2708_PERI_BASE + 0x200000) /* GPIO controller */
 
#include "GPIO.h"
#include <stdio.h>
#include <errno.h>
#include <stdlib.h> 
#include <math.h>
#include <signal.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
 
#define PAGE_SIZE (4*1024)
#define BLOCK_SIZE (4*1024)
 
int  mem_fd;
void *gpio_map;
 
// I/O access
volatile unsigned *gpio;
 
 
// GPIO setup macros. Always use INP_GPIO(x) before using OUT_GPIO(x) or SET_GPIO_ALT(x,y)
#define INP_GPIO(g) *(gpio+((g)/10)) &= ~(7<<(((g)%10)*3)) // bit fiddeling in an "array"
#define OUT_GPIO(g) *(gpio+((g)/10)) |=  (1<<(((g)%10)*3))
#define SET_GPIO_ALT(g,a) *(gpio+(((g)/10))) |= (((a)<=3?(a)+4:(a)==4?3:2)<<(((g)%10)*3))
 
#define GPIO_SET *(gpio+7)  // sets   bits which are 1 ignores bits which are 0
#define GPIO_CLR *(gpio+10) // clears bits which are 1 ignores bits which are 0
 
#define GET_GPIO(g) (*(gpio+13)&(1<<g)) // 0 if LOW, (1<<g) if HIGH
#define GET_GPIO_BYTE *(gpio+13)
 
#define GPIO_PULL *(gpio+37) // Pull up/pull down
#define GPIO_PULLCLK0 *(gpio+38) // Pull up/pull down clock
 
/* 
 * Function called to get the values 
 * from 0 to 255 (from a 8bit DipSwitch)
 * @returns dipValue 
 */
int getDipValue(int gpioNumbers[]) //
{
	int i = 0, dipValue = 0;
	for(i; i < 8; i++){
		
		int value = 0, gpioValue = 0, index = gpioNumbers[i];
		
		if( GET_GPIO(index) != 0){
			value=1;
		}
				
		dipValue += value << i;
		//printf("%i, index :%i \n", value, i);	
	}
	return dipValue;
	
	
}
 
/*int main(int argc, char **argv)
{

  // Set up gpi pointer for direct register access
  setup_io();
	
	int gpioPinNumbers[8] = {17, 27, 22, 24, 5, 12, 13, 26};
	
	while(1){
		printButton(gpioPinNumbers); //gpioNumbers
	}

	
  return 0;
 
} */// main
 
 
//
// Set up a memory regions to access GPIO
//
void setup_io()
{
   /* open /dev/mem */
   if ((mem_fd = open("/dev/mem", O_RDWR|O_SYNC) ) < 0) {
      printf("can't open /dev/mem \n");
      exit(-1);
   }
 
   /* mmap GPIO */
   gpio_map = mmap(
      NULL,             //Any address in our space will do
      BLOCK_SIZE,       //Map length
      PROT_READ|PROT_WRITE,// Enable reading & writing to mapped memory
      MAP_SHARED,       //Shared with other processes
      mem_fd,           //File to map
      GPIO_BASE         //Offset to GPIO peripheral
   );
 
   close(mem_fd); //No need to keep mem_fd open after mmap
 
   if (gpio_map == MAP_FAILED) {
      printf("mmap error %d\n", (int)gpio_map);//errno also set!
      exit(-1);
   }
 
   // Always use volatile pointer!
   gpio = (volatile unsigned *)gpio_map;
 
 
} // setup_io