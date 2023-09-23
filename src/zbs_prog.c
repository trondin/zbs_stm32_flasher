#include <stm32f1xx.h>
#include "system_interrupts.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include "usb_cdc.h"
#include "gpio.h"
#include "cdc_config.h"
#include "device_config.h"
#include "version.h"
#include "zbs_prog.h"

uint32_t FLASHER_VERSION = 0x00000020;

typedef enum
{
  ZBS_CMD_W_RAM = 0x02,
  ZBS_CMD_R_RAM = 0x03,
  ZBS_CMD_W_FLASH = 0x08,
  ZBS_CMD_R_FLASH = 0x09,
  ZBS_CMD_W_SFR = 0x12,
  ZBS_CMD_R_SFR = 0x13,
  ZBS_CMD_ERASE_FLASH = 0x88,
  ZBS_CMD_ERASE_INFOBLOCK = 0x48,
} ZBS_CMD_LIST;


#define RST_PORT GPIOB
#define RST_PIN 0

#define INIT_RST_OUT                                 \
  RST_PORT->CRL &= ~(0xF << ((RST_PIN & 0x7) << 2)); \
  RST_PORT->CRL |= 0x3 << ((RST_PIN & 0x7) << 2);
#define INIT_RST_IN                                 \
  RST_PORT->CRL &= ~(0xF << ((RST_PIN & 0x7) << 2)); \
  RST_PORT->CRL |= 0x4 << ((RST_PIN & 0x7) << 2);
#define RST_L RST_PORT->BRR = 1 << RST_PIN;  // reset
#define RST_H RST_PORT->BSRR = 1 << RST_PIN; // set

#define PWR_PORT GPIOB
#define PWR_PIN 1

#define INIT_PWR                                     \
  PWR_PORT->CRL &= ~(0xF << ((PWR_PIN & 0x7) << 2)); \
  PWR_PORT->CRL |= 0x3 << ((PWR_PIN & 0x7) << 2);
#define PWR_L PWR_PORT->BRR = 1 << PWR_PIN;  // reset
#define PWR_H PWR_PORT->BSRR = 1 << PWR_PIN; // set

#define SS_PORT GPIOA
#define SS_PIN 4

#define INIT_SS_OUT                                \
  SS_PORT->CRL &= ~(0xF << ((SS_PIN & 0x7) << 2)); \
  SS_PORT->CRL |= 0x3 << ((SS_PIN & 0x7) << 2);
#define INIT_SS_IN                                 \
  SS_PORT->CRL &= ~(0xF << ((SS_PIN & 0x7) << 2)); \
  SS_PORT->CRL |= 0x4 << ((SS_PIN & 0x7) << 2);
#define SS_L SS_PORT->BRR = 1 << SS_PIN;  // reset
#define SS_H SS_PORT->BSRR = 1 << SS_PIN; // set

#define CLK_PORT GPIOA
#define CLK_PIN 5
#define INIT_CLK_OUT                                 \
  CLK_PORT->CRL &= ~(0xF << ((CLK_PIN & 0x7) << 2)); \
  CLK_PORT->CRL |=   0x3 << ((CLK_PIN & 0x7) << 2);
#define INIT_CLK_IN                                  \
  CLK_PORT->CRL &= ~(0xF << ((CLK_PIN & 0x7) << 2)); \
  CLK_PORT->CRL |=   0x4 << ((CLK_PIN & 0x7) << 2);
#define CLK_L CLK_PORT->BRR  = 1 << CLK_PIN; 
#define CLK_H CLK_PORT->BSRR = 1 << CLK_PIN;



#define MISO_PORT GPIOA
#define MISO_PIN 6
#define INIT_MISO_IN                                   \
  MISO_PORT->CRL &= ~(0xF << ((MISO_PIN & 0x7) << 2)); \
  MISO_PORT->CRL |=   0x4 << ((MISO_PIN & 0x7) << 2);

#define MOSI_PORT GPIOA
#define MOSI_PIN 7
#define INIT_MOSI_OUT                                  \
  MOSI_PORT->CRL &= ~(0xF << ((MOSI_PIN & 0x7) << 2)); \
  MOSI_PORT->CRL |=   0x3 << ((MOSI_PIN & 0x7) << 2);
#define INIT_MOSI_IN                                   \
  MOSI_PORT->CRL &= ~(0xF << ((MOSI_PIN & 0x7) << 2)); \
  MOSI_PORT->CRL |= 0x4 << ((MOSI_PIN & 0x7) << 2);
#define MOSI_L MOSI_PORT->BRR = 1 << MOSI_PIN; 
#define MOSI_H MOSI_PORT->BSRR = 1 << MOSI_PIN;  

volatile uint8_t TimeoutFlag;  

__attribute__((noinline)) static void cdc_shell_write_string(const char *buf)
{
  cdc_shell_write(buf, strlen(buf));
}

void delay_ms(uint16_t ms)
{
  TIM2->ARR = ms*10;
  TIM2->SR = 0;  // Clear the update event flag 
  TIM2->CR1 |= TIM_CR1_CEN ;   // Start the timer counter  
  while (!(TIM2->SR & TIM_SR_UIF));  // Loop until the update event flag is set
  TIM2->CR1 &= ~TIM_CR1_CEN ;  
}


void delay_us(uint16_t time)
{
  uint16_t i;
  uint16_t j;
  i=time;
  while(i--)
  {
    j=6;
    while(j--) 
    { 
      asm("nop");
      asm("nop");
    }
  }
}



void start_timeout(void)
{
  TimeoutFlag  = 0;
  TIM3->PSC = SystemCoreClock/10000-1;   // 0.1ms    
  ////TIM3->ARR =  5000;  // 500ms
  //TIM3->CR1 = 0; 
  TIM3->ARR =  1000;  // 100ms
  TIM3->CNT =0;
  TIM3->SR = 0;  // Clear the update event flag 
  TIM3->DIER |= TIM_DIER_UIE;  
  TIM3->CR1 |= TIM_CR1_CEN ;   // Start the timer counter  
}


/*
void test(void)
{
  for(uint8_t i=0; i<3; i++)
  {
    CLK_H  
    delay_ms(1);
    CLK_L    
    delay_ms(1);
  }
}
*/


void disconnect_io(void)
{
  INIT_SS_IN
  INIT_CLK_IN
  INIT_MOSI_IN    
  INIT_RST_IN    
}

void connect_io(void)
{
  INIT_SS_OUT
  INIT_CLK_OUT
  INIT_MOSI_OUT  
  INIT_RST_OUT

  RST_L
  CLK_L
  MOSI_H
  SS_H
}


void zbs_prog_init()
{
  // SysTick_Config(SystemCoreClock/1000); // set tick to every 1ms
  RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;  
  TIM2->CR1 = 0; //TIM_CR1_DIR;
  TIM2->PSC = SystemCoreClock/10000-1;   // 0.1ms

  RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;    
  TIM3->CR1 = 0; 
  TIM3->PSC = SystemCoreClock/10000-1;   // 0.1ms  
  TIM3->ARR =  1000;  // 100ms
  NVIC_SetPriority(TIM3_IRQn, SYSTEM_INTERRUTPS_PRIORITY_BASE);
  NVIC_EnableIRQ(TIM3_IRQn);

  delay_ms(1);  
  RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN;
  disconnect_io();
  INIT_MISO_IN 
  INIT_PWR
  PWR_H  
  
  // walkaround
  TimeoutFlag  = 0;
  start_timeout();  // dummy timeout
  while (TimeoutFlag  == 0) asm("nop");
  TimeoutFlag  = 0;
}  



uint8_t at_cmd_rx_state = 0; // can be reset by timer
uint8_t at_rx_buffer[266];

uint8_t at_tx_buffer[266];

void send_at_answer(uint8_t answer_cmd, uint8_t len)
{
  uint16_t CRC_value = 0xAB34;
  uint8_t *buff_point;
  
  buff_point = at_tx_buffer;
  *buff_point++  = 'A';
  *buff_point++  = 'T';
  *buff_point++  = answer_cmd;
  CRC_value += answer_cmd;
  *buff_point++ = len;
  CRC_value += len;
  uint8_t i=len; 
  while(i--) CRC_value += *buff_point++;  
  *buff_point++ = CRC_value >> 8;
  *buff_point = CRC_value;
  cdc_shell_write(at_tx_buffer, 2 + 2 + len + 2);
}

void zbs_reset(void)
{
  disconnect_io();   
  // disconnect serial port
  uint16_t status = GPIOA->CRH;                         
  GPIOA->CRH &= ~((0xF << ((9 & 0x7) << 2))| (0xF << ((10 & 0x7) << 2))); 
  GPIOA->CRL |= (0x4 << ((9 & 0x7) << 2)) | (0x4 << ((10 & 0x7) << 2));

  INIT_RST_OUT    
  RST_L
  PWR_H
  delay_ms(500);
  PWR_L
  delay_ms(10);
  PWR_L
  delay_ms(100);  
  INIT_RST_IN    
  GPIOA->CRH = status;  
  delay_ms(10);  
}


void zbs_enable_debug(void)
{
  RST_H
  delay_ms(21);
  RST_L
  delay_ms(33);
  for(uint8_t i=0; i<4; i++)
  {
    CLK_H  
    delay_ms(1);
    CLK_L    
    delay_ms(1);
  }
  delay_ms(10);
  RST_H
  delay_ms(100);
}

uint8_t zbs_read_spi(void)
{
  uint8_t data = 0x00;
  SS_L  
  delay_us(5);  
  uint8_t i =8;
  while(i--)  
  {
    data <<= 1;
    CLK_H
    asm("nop");
    asm("nop");                  
    if ((GPIOA->IDR & 0x40) != 0) data |= 1;   
    CLK_L
  }
  delay_us(2);
  SS_H
  delay_us(2);  
  return data;
}
void zbs_send_spi(uint8_t data)
{  
  SS_L
  delay_us(5);
  uint8_t i =8;
  while(i--)
  {
    if (data & 0x80) MOSI_H
    else             MOSI_L
    CLK_H
    asm("nop");
    asm("nop");    
    asm("nop");
    asm("nop");                                
    CLK_L   
    data <<= 1;
  }
  delay_us(2);
  SS_H    
  MOSI_H  
  delay_us(2);  
}

void zbs_write_byte(uint8_t cmd, uint8_t addr, uint8_t data)
{
    zbs_send_spi(cmd);
    zbs_send_spi(addr);
    zbs_send_spi(data);
}

uint8_t zbs_read_byte(uint8_t cmd, uint8_t addr)
{
  uint8_t data = 0x00;
  zbs_send_spi(cmd);
  zbs_send_spi(addr);
  data = zbs_read_spi();
  return data;
}


uint8_t zbs_read_ram(uint8_t addr)
{
  return zbs_read_byte(ZBS_CMD_R_RAM, addr);
}

void zbs_write_ram(uint8_t addr, uint8_t data)
{
    zbs_write_byte(ZBS_CMD_W_RAM, addr, data);
}

uint8_t zbs_check_connection(void)
{
  zbs_write_ram(0xba, 0xA5);
  return zbs_read_ram(0xba) == 0xA5; 
}

void zbs_write_sfr(uint8_t addr, uint8_t data)
{
  zbs_write_byte(ZBS_CMD_W_SFR, addr, data);
}

uint8_t zbs_read_sfr(uint8_t addr)
{
  return zbs_read_byte(ZBS_CMD_R_SFR, addr);
}

uint8_t zbs_select_flash(uint8_t page)
{
  uint8_t result = 0;
  uint8_t sfr_low_bank = page ? 0x80 : 0x00;
  zbs_write_sfr(0xd8, sfr_low_bank);
  if(zbs_read_sfr(0xd8) == sfr_low_bank) result=1;
  return result;
}

void zbs_write_flash(uint16_t addr, uint8_t data)
{
    zbs_send_spi(ZBS_CMD_W_FLASH);
    zbs_send_spi(addr >> 8);
    zbs_send_spi(addr);
    zbs_send_spi(data);
}

uint8_t zbs_read_flash(uint16_t addr)
{
    uint8_t data = 0x00;
    zbs_send_spi(ZBS_CMD_R_FLASH);
    zbs_send_spi(addr >> 8);
    zbs_send_spi(addr);
    data = zbs_read_spi();
    return data;
}

void zbs_erase_flash()
{
    zbs_send_spi(ZBS_CMD_ERASE_FLASH);
    zbs_send_spi(0x00);
    zbs_send_spi(0x00);
    zbs_send_spi(0x00);
    delay_ms(100);
}

void zbs_erase_infoblock()
{
    zbs_send_spi(ZBS_CMD_ERASE_INFOBLOCK);
    zbs_send_spi(0x00);
    zbs_send_spi(0x00);
    zbs_send_spi(0x00);
    delay_ms(100);
}

uint8_t zbs_begin(uint8_t speed_code)
{
  connect_io();
  PWR_L  //  setpower
  zbs_enable_debug();
  return zbs_check_connection();
}


typedef enum
{
  CMD_GET_VERSION = 1,
  CMD_RESET_ESP = 2,
  CMD_ZBS_BEGIN = 10,
  CMD_RESET_ZBS = 11,
  CMD_SELECT_PAGE = 12,
  CMD_SET_POWER = 13,
  CMD_READ_RAM = 20,
  CMD_WRITE_RAM = 21,
  CMD_READ_FLASH = 22,
  CMD_WRITE_FLASH = 23,
  CMD_READ_SFR = 24,
  CMD_WRITE_SFR = 25,
  CMD_ERASE_FLASH = 26,
  CMD_ERASE_INFOBLOCK = 27,
  CMD_SAVE_MAC_FROM_FW = 40,
  CMD_PASS_THROUGH = 50,
  CMD_PRG_TIMEOUT = 60,  
} ZBS_UART_PROTO;



void handle_uart_cmd(uint8_t cmd, uint8_t len)
{
  uint8_t length;
  uint16_t address;   
  uint8_t data;   
  uint8_t * tx_buff_point;
  uint8_t * rx_buff_point;
  uint16_t i;
  uint8_t j;
  uint8_t error;  

  switch (cmd)
  {
  case CMD_GET_VERSION:
    tx_buff_point = &at_tx_buffer[4];
    *tx_buff_point++ = (uint8_t)(FLASHER_VERSION >> 24);
    *tx_buff_point++ = (uint8_t)(FLASHER_VERSION >> 16);
    *tx_buff_point++ = (uint8_t)(FLASHER_VERSION >> 8);
    *tx_buff_point++ = (uint8_t)(FLASHER_VERSION);
    send_at_answer(cmd, 4);
    break;
  case CMD_RESET_ESP:
    send_at_answer(cmd, 0);
    delay_ms(100) ;
    NVIC_SystemReset();
    break;
  case CMD_ZBS_BEGIN:
    at_tx_buffer[4] = zbs_begin(at_rx_buffer[0]);
    send_at_answer(cmd, 1);
    break;
  case CMD_RESET_ZBS:
    zbs_reset();
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_SELECT_PAGE:
    at_tx_buffer[4] = zbs_select_flash(at_rx_buffer[0] ? 1 : 0);
    send_at_answer(cmd, 1);
    break;
  case CMD_SET_POWER:
    if (at_rx_buffer[0]) PWR_L
    else             PWR_H
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_READ_RAM:
    at_tx_buffer[4] = zbs_read_ram(at_rx_buffer[0]);
    send_at_answer(cmd, 1);
    break;
  case CMD_WRITE_RAM:
    zbs_write_ram(at_rx_buffer[0], at_rx_buffer[1]);
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_READ_FLASH:
    //test();
    length = at_rx_buffer[0];
    address = at_rx_buffer[1];
    address <<= 8;
    address |= at_rx_buffer[2];    
    tx_buff_point = &at_tx_buffer[4];
    i = length;
    while(i--) *tx_buff_point++ = zbs_read_flash(address++);    
    send_at_answer(cmd, length);
    break;
  case CMD_WRITE_FLASH:
    length = at_rx_buffer[0];
    //address = (at_rx_buffer[1] << 8) | at_rx_buffer[2]; 
    address = at_rx_buffer[1];
    address <<= 8;
    address |= at_rx_buffer[2];        
    rx_buff_point =  &at_rx_buffer[3];
    /*
    if (length >= (0xff - 3))
    { // Len too high, only 0xFF - header len possible
      at_tx_buffer[4] = 0xEE;
      send_at_answer(cmd, 1);
      break;
    }
    */

    error = 0;
    i = length;
    while(i--)
    {
      data = *rx_buff_point++;
      if (data != 0xff)
      {
        /*
        zbs_write_flash(address, data);
        if (zbs_read_flash(address)!=data)  
        {
          error = 1;
          break;
        }       
        */

        j = 3;
        error = 1;
        while(j--)
        {
          zbs_write_flash(address, data);
          if (zbs_read_flash(address)==data)
          {
            error = 0;
            break;
          }
        }
        if (error) break;
        
      }
      address++;
    }
    if (error) at_tx_buffer[4] = 0;
    else       at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_READ_SFR:
    at_tx_buffer[4] = zbs_read_sfr(at_rx_buffer[0]);
    send_at_answer(cmd, 1);
    break;
  case CMD_WRITE_SFR:
    zbs_write_sfr(at_rx_buffer[0], at_rx_buffer[1]);
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_ERASE_FLASH:
    zbs_erase_flash();
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  case CMD_ERASE_INFOBLOCK:
    zbs_erase_infoblock();
    at_tx_buffer[4] = 1;
    send_at_answer(cmd, 1);
    break;
  }
}


void zbs_prog_process_input(const void *buf, size_t count)
{

  static uint16_t CRC_calc;
  static uint16_t CRC_in;
  static uint8_t at_cmd; 
  static uint8_t expected_len;
  static uint8_t curr_data_pos;
  const uint8_t *buf_p = buf;


  while (count--)
  {
    uint8_t curr_char = *buf_p++;


    switch (at_cmd_rx_state)
    {
    case 0:
      if(TimeoutFlag)
      {
        TimeoutFlag=0;
        at_tx_buffer[4] = 0;
        send_at_answer(CMD_PRG_TIMEOUT, 1);
      }
      if (curr_char == 'A') at_cmd_rx_state++;
      break;
    case 1:
      if (curr_char == 'T') 
      {
        at_cmd_rx_state++;
        curr_data_pos = 0;
        CRC_calc = 0xAB34;              
        start_timeout();  // start timer here!!
      }
      else 
      {
        if (curr_char != 'A') at_cmd_rx_state = 0;
      }
      break;
    case 2:
      at_cmd = curr_char; // Receive current CMD
      CRC_calc += curr_char;
      at_cmd_rx_state++;
      break;
    case 3:
      expected_len = curr_char; // Receive Expected length of data
      CRC_calc += curr_char;
      if (expected_len == 0) at_cmd_rx_state = 5;
      else                   at_cmd_rx_state++;
      break;
    case 4:
      CRC_calc += curr_char; // Read the actual data
      at_rx_buffer[curr_data_pos++] = curr_char;
      if (curr_data_pos == expected_len) at_cmd_rx_state++;
      break;
    case 5:
      CRC_in = curr_char << 8; // Receive high byte of crude CRC
      at_cmd_rx_state++;
      break;
    case 6:
      CRC_in |= curr_char;
      TIM3->SR = 0;  // reset flags      
      TIM3->DIER = 0;  // ~TIM_DIER_UIE;  
      TIM3->CR1  = 0;   // ~TIM_CR1_CEN ;        
      if (CRC_calc == CRC_in) handle_uart_cmd(at_cmd, expected_len);
      at_cmd_rx_state = 0;
      break;
    default:
      break;
    }
    //  cdc_shell_write_string(cdc_shell_prompt);
  }
}



//timeout

void TIM3_IRQHandler() 
{
  TIM3->SR = 0;  // reset flags
  TIM3->DIER = 0;  // ~TIM_DIER_UIE;  
  TIM3->CR1  = 0; // ~TIM_CR1_CEN ;   
  at_cmd_rx_state = 0;
  TimeoutFlag  = 1;
}


/*
void zbs_prog_process_input(const void *buf, size_t count)
{
  cdc_shell_write(buf, count);
}
*/