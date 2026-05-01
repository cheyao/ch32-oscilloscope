#include "ch32fun.h"

#include <stdio.h>

#define SSD1306_128X64
#include "ssd1306_i2c.h"

#include "ssd1306.h"

// Delay how many us after scroll
// Increase this value if you have artifacts
// Can use this time to perform computations
#define SCROLL_DELAY 12500

// 0-1023
volatile uint16_t adc_buffer[3];
uint8_t mode = 0;
uint8_t sample = 3;

void set_bit(uint8_t* data, const uint16_t adc) { data[7 - (adc / 128)] |= (1 << (7 - ((adc % 128) / 16))); }
void updategraph(void);
void setup(void);
void trigger(void);
void clear_screen(void);
void scroll(void);

int main() {
	SystemInit();
	setup();
	funDigitalWrite(PC0, FUN_HIGH);

	uint8_t trig_last = 1;
	while (1) {
		const uint8_t trig = funDigitalRead(PD2);
		if (trig_last != trig && trig == 0) {
			trigger();

			Delay_Ms(200);
			uint8_t trig_last = funDigitalRead(PD2);
			uint8_t button_last = funDigitalRead(PD0);
			while (1) {
				const uint8_t trig = funDigitalRead(PD2);
				const uint8_t button = funDigitalRead(PD0);

				if (trig_last != trig && trig == 0) {
					break;
				}
				if (button_last != button && button == 0) {
					sample = (sample >= 7) ? 0 : sample + 1;

					ADC1->SAMPTR2 &= ~(ADC_SMP0 | ADC_SMP1 | ADC_SMP2);
					ADC1->SAMPTR2 |=
						(sample << (3 * 0)) | (sample << (3 * 1)) | (sample << (3 * 2));

					ADC1->CTLR2 |= CTLR2_RSTCAL_Set;
					while (ADC1->CTLR2 & CTLR2_RSTCAL_Set)
						;

					ADC1->CTLR2 |= CTLR2_CAL_Set;
					while (ADC1->CTLR2 & CTLR2_CAL_Set)
						;

					trigger();
				}

				trig_last = trig;
				button_last = button;
			}
		} else {
			updategraph();
		}
		trig_last = trig;
	}
}

// Called once at start of main()
void setup(void) {
	funGpioInitAll();

	// Auto-boot
	if (FLASH->STATR & (1 << 14)) {
		NVIC_SystemReset();
	}
	FLASH->KEYR = FLASH_KEY1;
	FLASH->KEYR = FLASH_KEY2;
	FLASH->BOOT_MODEKEYR = FLASH_KEY1;
	FLASH->BOOT_MODEKEYR = FLASH_KEY2;
	FLASH->STATR |= (1 << 14);
	FLASH->CTLR = CR_LOCK_Set;
	funPinMode(PD4, GPIO_CFGLR_OUT_10Mhz_PP);

	// Buttons
	funPinMode(PD2, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	funDigitalWrite(PD2, FUN_HIGH);
	funPinMode(PD0, GPIO_Speed_In | GPIO_CNF_IN_PUPD);
	funDigitalWrite(PD0, FUN_HIGH);
	// GND for button PD0
	funPinMode(PC6, GPIO_Speed_2MHz | GPIO_CNF_OUT_PP);
	funDigitalWrite(PC6, FUN_LOW);

	// ADCPRE[4:0] = 0; (Negative mask)
	// ADC clock = HBCLK / 2
	// Default HBCLK = 24MHz * 2 = 48MHz
	// => ADC clock = 24MHz = max speed
	RCC->CFGR0 &= ~(0x1F << 11);
	RCC->APB2PCENR |= RCC_APB2Periph_ADC1;

	// A0
	funPinMode(PA2, GPIO_Speed_In | GPIO_CNF_IN_ANALOG);
	// A1
	funPinMode(PA1, GPIO_Speed_In | GPIO_CNF_IN_ANALOG);
	// A2
	funPinMode(PC4, GPIO_Speed_In | GPIO_CNF_IN_ANALOG);
	// LED
	funPinMode(PC0, GPIO_Speed_10MHz | GPIO_CNF_OUT_PP);

	// Reset ADC.
	RCC->APB2PRSTR |= RCC_APB2Periph_ADC1;
	RCC->APB2PRSTR &= ~RCC_APB2Periph_ADC1;

	ADC1->RSQR1 = 2 << 20;
	ADC1->RSQR2 = 0;
	ADC1->RSQR3 = (0 << (5 * 0)) | (1 << (5 * 1)) | (2 << (5 * 2));

	// TODO: Less cycles for less precise values?
	// 0:7 => 3/9/15/30/43/57/73/241 cycles
	ADC1->SAMPTR2 = (sample << (3 * 0)) | (sample << (3 * 1)) | (sample << (3 * 2));

	ADC1->CTLR2 |= ADC_ADON | ADC_EXTSEL;

	// Reset calibration
	ADC1->CTLR2 |= CTLR2_RSTCAL_Set;
	while (ADC1->CTLR2 & CTLR2_RSTCAL_Set)
		;

	// Calibrate
	ADC1->CTLR2 |= CTLR2_CAL_Set;
	while (ADC1->CTLR2 & CTLR2_CAL_Set)
		;

	// Enable DMA
	RCC->AHBPCENR |= RCC_AHBPeriph_DMA1;

	// DMA1_Channel1 is for ADC
	DMA1_Channel1->PADDR = (uint32_t)&ADC1->RDATAR;
	DMA1_Channel1->MADDR = (uint32_t)adc_buffer;
	DMA1_Channel1->CNTR = 3;
	DMA1_Channel1->CFGR = DMA_M2M_Disable | DMA_Priority_VeryHigh | DMA_MemoryDataSize_HalfWord |
			      DMA_PeripheralDataSize_HalfWord | DMA_MemoryInc_Enable | DMA_Mode_Circular |
			      DMA_DIR_PeripheralSRC;

	// Turn on DMA channel 1
	DMA1_Channel1->CFGR |= DMA_CFGR1_EN;

	// enable scanning
	ADC1->CTLR1 |= ADC_SCAN;

	// Enable continuous conversion and DMA
	ADC1->CTLR2 |= ADC_CONT | ADC_DMA | ADC_EXTSEL;

	// start conversion
	ADC1->CTLR2 |= ADC_SWSTART;

	ssd1306_i2c_init();

	uint8_t* cmd_list = (uint8_t*)ssd1306_init_array;
	while (*cmd_list != SSD1306_TERMINATE_CMDS) {
		if (ssd1306_cmd(*cmd_list++))
			break;
	}

	clear_screen();
}

void trigger(void) {
	clear_screen();

	// Free the ram asap
	uint16_t buffer0[128];
	uint16_t buffer1[128];
	uint16_t buffer2[128];

	// Clear EOC
	ADC1->STATR &= ~ADC_EOC;
	for (uint8_t i = 0; i < 128; ++i) {
		while ((ADC1->STATR & ADC_EOC) == 0)
			;
		ADC1->STATR &= ~ADC_EOC;

		buffer0[i] = adc_buffer[0];
		buffer1[i] = adc_buffer[1];
		buffer2[i] = adc_buffer[2];
	}

	ssd1306_cmd(SSD1306_COLUMNADDR);
	ssd1306_cmd(0);
	ssd1306_cmd(127);

	ssd1306_cmd(SSD1306_PAGEADDR);
	ssd1306_cmd(0);
	ssd1306_cmd(7);

	for (uint8_t i = 0; i < 128; ++i) {
		uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

		// 0-1023, normalize to 0-63
		if (mode == 0 || mode == 3) {
			set_bit(data, buffer0[i]);
		}
		if (mode == 1 || mode == 3) {
			set_bit(data, buffer1[i]);
		}
		if (mode == 2 || mode == 3) {
			set_bit(data, buffer2[i]);
		}

		if (i <= sample) {
			set_bit(data, 1023);
		}

		ssd1306_data(data, 8);
	}
}

void updategraph(void) {
	static uint8_t ptr = 0;

	scroll();

	ssd1306_cmd(SSD1306_COLUMNADDR);
	ssd1306_cmd(127);
	ssd1306_cmd(127);

	ssd1306_cmd(SSD1306_PAGEADDR);
	ssd1306_cmd(0);
	ssd1306_cmd(7);

	// Overwrite the last column
	uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	uint8_t button_last = 1;
	const uint8_t button = funDigitalRead(PD0);
	if (button_last != button && button == 0) {
		mode = (mode == 3) ? 0 : mode + 1;
	}
	button_last = button;

	// 0-1023, normalize to 0-63
	if (mode == 0 || mode == 3) {
		set_bit(data, adc_buffer[0]);
	}
	if (mode == 1 || mode == 3) {
		set_bit(data, adc_buffer[1]);
	}
	if (mode == 2 || mode == 3) {
		set_bit(data, adc_buffer[2]);
	}

	ssd1306_data(data, 8);

	// Inc pointer for next run
	ptr = (ptr == 63) ? 0 : ptr + 1;
}

void clear_screen(void) {
	const static uint8_t data[8] = {0, 0, 0, 0, 0, 0, 0, 0};

	ssd1306_cmd(SSD1306_COLUMNADDR);
	ssd1306_cmd(0);
	ssd1306_cmd(127);

	ssd1306_cmd(SSD1306_PAGEADDR);
	ssd1306_cmd(0);
	ssd1306_cmd(7);

	for (int i = 0; i < 128; ++i) {
		ssd1306_data(data, 8);
	}
}

void scroll(void) {
	ssd1306_cmd(0x2D);
	ssd1306_cmd(0x00); // Dummy
	ssd1306_cmd(0x00); // Start at page 0
	ssd1306_cmd(0x00); // Time interval
	ssd1306_cmd(0x07); // End at page 7
	ssd1306_cmd(0x00);
	ssd1306_cmd(0xFF);
	ssd1306_cmd(0x2F);
	ssd1306_cmd(0x2E);

	Delay_Us(SCROLL_DELAY);
}
