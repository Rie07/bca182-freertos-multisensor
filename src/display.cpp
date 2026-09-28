#include "display.h"
#include <stdint.h>

#define SSD1306_ADDR (0x3C << 1)

static I2C_HandleTypeDef *oled_i2c;
static uint8_t buffer[1024];

static const uint8_t font_space[5] = {0, 0, 0, 0, 0};

static const uint8_t font_R[5] = {0x7F, 0x09, 0x19, 0x29, 0x46};
static const uint8_t font_O[5] = {0x3E, 0x41, 0x41, 0x41, 0x3E};
static const uint8_t font_M[5] = {0x7F, 0x02, 0x0C, 0x02, 0x7F};
static const uint8_t font_N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};
static const uint8_t font_I[5] = {0x00, 0x41, 0x7F, 0x41, 0x00};

static const uint8_t font_T[5] = {0x01, 0x01, 0x7F, 0x01, 0x01};
static const uint8_t font_e[5] = {0x38, 0x54, 0x54, 0x54, 0x18};
static const uint8_t font_m[5] = {0x7C, 0x04, 0x18, 0x04, 0x78};
static const uint8_t font_p[5] = {0x7C, 0x14, 0x14, 0x14, 0x08};
static const uint8_t font_r[5] = {0x7C, 0x08, 0x04, 0x04, 0x08};
static const uint8_t font_a[5] = {0x20, 0x54, 0x54, 0x54, 0x78};
static const uint8_t font_t[5] = {0x04, 0x3F, 0x44, 0x40, 0x20};
static const uint8_t font_u[5] = {0x3C, 0x40, 0x40, 0x20, 0x7C};
static const uint8_t font_C[5] = {0x3E, 0x41, 0x41, 0x41, 0x22};

static const uint8_t font_dot[5] = {0x00, 0x60, 0x60, 0x00, 0x00};

static const uint8_t font_0[5] = {0x3E, 0x51, 0x49, 0x45, 0x3E};
static const uint8_t font_1[5] = {0x00, 0x42, 0x7F, 0x40, 0x00};
static const uint8_t font_2[5] = {0x42, 0x61, 0x51, 0x49, 0x46};
static const uint8_t font_3[5] = {0x21, 0x41, 0x45, 0x4B, 0x31};
static const uint8_t font_4[5] = {0x18, 0x14, 0x12, 0x7F, 0x10};
static const uint8_t font_5[5] = {0x27, 0x45, 0x45, 0x45, 0x39};
static const uint8_t font_6[5] = {0x3C, 0x4A, 0x49, 0x49, 0x30};
static const uint8_t font_7[5] = {0x01, 0x71, 0x09, 0x05, 0x03};
static const uint8_t font_8[5] = {0x36, 0x49, 0x49, 0x49, 0x36};
static const uint8_t font_9[5] = {0x06, 0x49, 0x49, 0x29, 0x1E};

static const uint8_t *GetGlyph(char c)
{
    switch (c)
    {
        case 'R': return font_R;
        case 'O': return font_O;
        case 'M': return font_M;
        case 'N': return font_N;
        case 'I': return font_I;

        case 'T': return font_T;
        case 'e': return font_e;
        case 'm': return font_m;
        case 'p': return font_p;
        case 'r': return font_r;
        case 'a': return font_a;
        case 't': return font_t;
        case 'u': return font_u;
        case 'C': return font_C;

        case '.': return font_dot;

        case '0': return font_0;
        case '1': return font_1;
        case '2': return font_2;
        case '3': return font_3;
        case '4': return font_4;
        case '5': return font_5;
        case '6': return font_6;
        case '7': return font_7;
        case '8': return font_8;
        case '9': return font_9;

        case ' ': return font_space;

        default: return font_space;
    }
}

static void SSD1306_Command(uint8_t command)
{
    uint8_t data[2] = {0x00, command};

    HAL_I2C_Master_Transmit(
        oled_i2c,
        SSD1306_ADDR,
        data,
        2,
        HAL_MAX_DELAY
    );
}

static void SSD1306_Data(uint8_t *data, uint16_t size)
{
    uint8_t packet[17];
    packet[0] = 0x40;

    while (size > 0)
    {
        uint16_t chunk = (size > 16) ? 16 : size;

        for (uint16_t i = 0; i < chunk; i++)
        {
            packet[i + 1] = data[i];
        }

        HAL_I2C_Master_Transmit(
            oled_i2c,
            SSD1306_ADDR,
            packet,
            chunk + 1,
            HAL_MAX_DELAY
        );

        data += chunk;
        size -= chunk;
    }
}

void Display_Init(I2C_HandleTypeDef *hi2c)
{
    oled_i2c = hi2c;

    HAL_Delay(100);

    SSD1306_Command(0xAE);
    SSD1306_Command(0xD5);
    SSD1306_Command(0x80);
    SSD1306_Command(0xA8);
    SSD1306_Command(0x3F);
    SSD1306_Command(0xD3);
    SSD1306_Command(0x00);
    SSD1306_Command(0x40);
    SSD1306_Command(0x8D);
    SSD1306_Command(0x14);
    SSD1306_Command(0x20);
    SSD1306_Command(0x00);
    SSD1306_Command(0xA1);
    SSD1306_Command(0xC8);
    SSD1306_Command(0xDA);
    SSD1306_Command(0x12);
    SSD1306_Command(0x81);
    SSD1306_Command(0x7F);
    SSD1306_Command(0xD9);
    SSD1306_Command(0xF1);
    SSD1306_Command(0xDB);
    SSD1306_Command(0x40);
    SSD1306_Command(0xA4);
    SSD1306_Command(0xA6);
    SSD1306_Command(0xAF);

    Display_Clear();
}

void Display_Clear(void)
{
    for (int i = 0; i < 1024; i++)
    {
        buffer[i] = 0;
    }

    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(0xB0 + page);
        SSD1306_Command(0x00);
        SSD1306_Command(0x10);

        SSD1306_Data(&buffer[page * 128], 128);
    }
}

static void DrawChar(uint8_t x, uint8_t page, char c)
{
    const uint8_t *glyph = GetGlyph(c);

    if (x > 122 || page > 7)
        return;

    for (uint8_t i = 0; i < 5; i++)
    {
        buffer[page * 128 + x + i] = glyph[i];
    }
}

static void DrawText(uint8_t x, uint8_t page, const char *text)
{
    while (*text)
    {
        DrawChar(x, page, *text);

        x += 6;
        text++;
    }
}

void Display_ShowTemperature(float temperature)
{
    int whole = (int)temperature;
    int decimal = (int)((temperature - whole) * 10);

    Display_Clear();

    DrawText(0, 0, "ROOM MONITOR");
    DrawText(0, 2, "Temperature");

    char number[16];

    number[0] = '0' + (whole / 10);
    number[1] = '0' + (whole % 10);
    number[2] = '.';
    number[3] = '0' + decimal;
    number[4] = ' ';
    number[5] = 'C';
    number[6] = '\0';

    DrawText(0, 4, number);

    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(0xB0 + page);
        SSD1306_Command(0x00);
        SSD1306_Command(0x10);

        SSD1306_Data(&buffer[page * 128], 128);
    }
}

void Display_ShowHumidity(float humidity)
{
    int whole = (int)humidity;
    int decimal = (int)((humidity - whole) * 10);

    Display_Clear();

    DrawText(0, 0, "ROOM MONITOR");
    DrawText(0, 2, "Humidity:");

    char number[16];

    number[0] = '0' + (whole / 10);
    number[1] = '0' + (whole % 10);
    number[2] = '.';
    number[3] = '0' + decimal;
    number[4] = ' ';
    number[5] = '%';
    number[6] = '\0';

    DrawText(0, 4, number);

    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(0xB0 + page);
        SSD1306_Command(0x00);
        SSD1306_Command(0x10);

        SSD1306_Data(&buffer[page * 128], 128);
    }
}


void Display_ShowLight(int lightLevel)
{
    Display_Clear();

    DrawText(0, 0, "ROOM MONITOR");
    DrawText(0, 2, "Light:");

    char number[16];

    number[0] = '0' + (lightLevel / 10);
    number[1] = '0' + (lightLevel % 10);
    number[2] = ' ';
    number[3] = '%';
    number[4] = '\0';

    DrawText(0, 4, number);

    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(0xB0 + page);
        SSD1306_Command(0x00);
        SSD1306_Command(0x10);

        SSD1306_Data(&buffer[page * 128], 128);
    }
}


void Display_ShowMotion(bool motionDetected)
{
    Display_Clear();

    DrawText(0, 0, "ROOM MONITOR");
    DrawText(0, 2, "Motion:");

    if (motionDetected)
    {
        DrawText(0, 4, "ACTIVE");
    }
    else
    {
        DrawText(0, 4, "INACTIVE");
    }

    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(0xB0 + page);
        SSD1306_Command(0x00);
        SSD1306_Command(0x10);

        SSD1306_Data(&buffer[page * 128], 128);
    }
}