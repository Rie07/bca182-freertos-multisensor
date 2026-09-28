#include "display.h"

#include <stdint.h>
#include <cstdio>


/* ============================================================
 * SSD1306 configuration
 * ============================================================ */

#define SSD1306_ADDR (0x3C << 1)


static I2C_HandleTypeDef *oled_i2c =
    nullptr;


static uint8_t buffer[1024];


/* ============================================================
 * 5x7 FONT
 *
 * Only characters needed by this laboratory are included.
 * ============================================================ */

static const uint8_t font_space[5] =
{
    0x00, 0x00, 0x00, 0x00, 0x00
};


/* Uppercase */

static const uint8_t font_A[5] =
{
    0x7E, 0x11, 0x11, 0x11, 0x7E
};


static const uint8_t font_C[5] =
{
    0x3E, 0x41, 0x41, 0x41, 0x22
};


static const uint8_t font_E[5] =
{
    0x7F, 0x49, 0x49, 0x49, 0x41
};


static const uint8_t font_H[5] =
{
    0x7F, 0x08, 0x08, 0x08, 0x7F
};


static const uint8_t font_I[5] =
{
    0x00, 0x41, 0x7F, 0x41, 0x00
};


static const uint8_t font_L[5] =
{
    0x7F, 0x40, 0x40, 0x40, 0x40
};


static const uint8_t font_M[5] =
{
    0x7F, 0x02, 0x0C, 0x02, 0x7F
};


static const uint8_t font_N[5] =
{
    0x7F, 0x04, 0x08, 0x10, 0x7F
};


static const uint8_t font_O[5] =
{
    0x3E, 0x41, 0x41, 0x41, 0x3E
};


static const uint8_t font_R[5] =
{
    0x7F, 0x09, 0x19, 0x29, 0x46
};


static const uint8_t font_T[5] =
{
    0x01, 0x01, 0x7F, 0x01, 0x01
};


static const uint8_t font_V[5] =
{
    0x1F, 0x20, 0x40, 0x20, 0x1F
};


/* Lowercase */

static const uint8_t font_a[5] =
{
    0x20, 0x54, 0x54, 0x54, 0x78
};


static const uint8_t font_d[5] =
{
    0x38, 0x44, 0x44, 0x48, 0x7F
};


static const uint8_t font_e[5] =
{
    0x38, 0x54, 0x54, 0x54, 0x18
};


static const uint8_t font_g[5] =
{
    0x0C, 0x52, 0x52, 0x52, 0x3E
};


static const uint8_t font_h[5] =
{
    0x7F, 0x08, 0x04, 0x04, 0x78
};


static const uint8_t font_i[5] =
{
    0x00, 0x44, 0x7D, 0x40, 0x00
};


static const uint8_t font_m[5] =
{
    0x7C, 0x04, 0x18, 0x04, 0x78
};


static const uint8_t font_n[5] =
{
    0x7C, 0x08, 0x04, 0x04, 0x78
};


static const uint8_t font_o[5] =
{
    0x38, 0x44, 0x44, 0x44, 0x38
};


static const uint8_t font_p[5] =
{
    0x7C, 0x14, 0x14, 0x14, 0x08
};


static const uint8_t font_r[5] =
{
    0x7C, 0x08, 0x04, 0x04, 0x08
};


static const uint8_t font_t[5] =
{
    0x04, 0x3F, 0x44, 0x40, 0x20
};


static const uint8_t font_u[5] =
{
    0x3C, 0x40, 0x40, 0x20, 0x7C
};


static const uint8_t font_y[5] =
{
    0x0C, 0x50, 0x50, 0x50, 0x3C
};


/* Symbols */

static const uint8_t font_dot[5] =
{
    0x00, 0x60, 0x60, 0x00, 0x00
};


static const uint8_t font_colon[5] =
{
    0x00, 0x36, 0x36, 0x00, 0x00
};


static const uint8_t font_percent[5] =
{
    0x62, 0x64, 0x08, 0x13, 0x23
};


static const uint8_t font_minus[5] =
{
    0x08, 0x08, 0x08, 0x08, 0x08
};


/* Digits */

static const uint8_t font_0[5] =
{
    0x3E, 0x51, 0x49, 0x45, 0x3E
};


static const uint8_t font_1[5] =
{
    0x00, 0x42, 0x7F, 0x40, 0x00
};


static const uint8_t font_2[5] =
{
    0x42, 0x61, 0x51, 0x49, 0x46
};


static const uint8_t font_3[5] =
{
    0x21, 0x41, 0x45, 0x4B, 0x31
};


static const uint8_t font_4[5] =
{
    0x18, 0x14, 0x12, 0x7F, 0x10
};


static const uint8_t font_5[5] =
{
    0x27, 0x45, 0x45, 0x45, 0x39
};


static const uint8_t font_6[5] =
{
    0x3C, 0x4A, 0x49, 0x49, 0x30
};


static const uint8_t font_7[5] =
{
    0x01, 0x71, 0x09, 0x05, 0x03
};


static const uint8_t font_8[5] =
{
    0x36, 0x49, 0x49, 0x49, 0x36
};


static const uint8_t font_9[5] =
{
    0x06, 0x49, 0x49, 0x29, 0x1E
};


/* ============================================================
 * Select glyph
 * ============================================================ */

static const uint8_t *GetGlyph(char c)
{
    switch (c)
    {
        /* Uppercase */

        case 'A':
            return font_A;

        case 'C':
            return font_C;

        case 'E':
            return font_E;

        case 'H':
            return font_H;

        case 'I':
            return font_I;

        case 'L':
            return font_L;

        case 'M':
            return font_M;

        case 'N':
            return font_N;

        case 'O':
            return font_O;

        case 'R':
            return font_R;

        case 'T':
            return font_T;

        case 'V':
            return font_V;


        /* Lowercase */

        case 'a':
            return font_a;

        case 'd':
            return font_d;

        case 'e':
            return font_e;

        case 'g':
            return font_g;

        case 'h':
            return font_h;

        case 'i':
            return font_i;

        case 'm':
            return font_m;

        case 'n':
            return font_n;

        case 'o':
            return font_o;

        case 'p':
            return font_p;

        case 'r':
            return font_r;

        case 't':
            return font_t;

        case 'u':
            return font_u;

        case 'y':
            return font_y;


        /* Symbols */

        case '.':
            return font_dot;

        case ':':
            return font_colon;

        case '%':
            return font_percent;

        case '-':
            return font_minus;


        /* Numbers */

        case '0':
            return font_0;

        case '1':
            return font_1;

        case '2':
            return font_2;

        case '3':
            return font_3;

        case '4':
            return font_4;

        case '5':
            return font_5;

        case '6':
            return font_6;

        case '7':
            return font_7;

        case '8':
            return font_8;

        case '9':
            return font_9;


        case ' ':
            return font_space;


        default:
            return font_space;
    }
}


/* ============================================================
 * SSD1306 low-level communication
 * ============================================================ */

static void SSD1306_Command(uint8_t command)
{
    uint8_t data[2] =
    {
        0x00,
        command
    };


    HAL_I2C_Master_Transmit(
        oled_i2c,
        SSD1306_ADDR,
        data,
        2,
        HAL_MAX_DELAY
    );
}


static void SSD1306_Data(
    uint8_t *data,
    uint16_t size)
{
    uint8_t packet[17];


    packet[0] =
        0x40;


    while (size > 0)
    {
        uint16_t chunk =
            (size > 16)
                ? 16
                : size;


        for (uint16_t i = 0; i < chunk; i++)
        {
            packet[i + 1] =
                data[i];
        }


        HAL_I2C_Master_Transmit(
            oled_i2c,
            SSD1306_ADDR,
            packet,
            chunk + 1,
            HAL_MAX_DELAY
        );


        data +=
            chunk;


        size -=
            chunk;
    }
}


/* ============================================================
 * Buffer functions
 * ============================================================ */

static void ClearBuffer(void)
{
    for (int i = 0; i < 1024; i++)
    {
        buffer[i] =
            0;
    }
}


static void FlushBuffer(void)
{
    for (uint8_t page = 0; page < 8; page++)
    {
        SSD1306_Command(
            0xB0 + page
        );


        SSD1306_Command(
            0x00
        );


        SSD1306_Command(
            0x10
        );


        SSD1306_Data(
            &buffer[page * 128],
            128
        );
    }
}


/* ============================================================
 * OLED initialization
 * ============================================================ */

void Display_Init(
    I2C_HandleTypeDef *hi2c)
{
    oled_i2c =
        hi2c;


    HAL_Delay(
        100
    );


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


/* ============================================================
 * Clear OLED
 * ============================================================ */

void Display_Clear(void)
{
    ClearBuffer();

    FlushBuffer();
}


/* ============================================================
 * Character drawing
 * ============================================================ */

static void DrawChar(
    uint8_t x,
    uint8_t page,
    char c)
{
    if (
        x > 122 ||
        page > 7
    )
    {
        return;
    }


    const uint8_t *glyph =
        GetGlyph(c);


    for (uint8_t i = 0; i < 5; i++)
    {
        buffer[
            page * 128 +
            x +
            i
        ] =
            glyph[i];
    }
}


static void DrawText(
    uint8_t x,
    uint8_t page,
    const char *text)
{
    while (*text)
    {
        DrawChar(
            x,
            page,
            *text
        );


        x +=
            6;


        text++;
    }
}


/* ============================================================
 * Temperature
 * ============================================================ */

void Display_ShowTemperature(
    float temperature)
{
    ClearBuffer();


    DrawText(
        0,
        0,
        "ROOM MONITOR"
    );


    DrawText(
        0,
        2,
        "Temperature"
    );


    /*
     * Convert temperature to tenths without requiring
     * floating-point printf support.
     */
    int tenths =
        static_cast<int>(
            temperature *
            10.0f
        );


    bool negative =
        tenths < 0;


    if (negative)
    {
        tenths =
            -tenths;
    }


    int whole =
        tenths / 10;


    int decimal =
        tenths % 10;


    char number[20];


    if (negative)
    {
        snprintf(
            number,
            sizeof(number),
            "-%d.%d C",
            whole,
            decimal
        );
    }
    else
    {
        snprintf(
            number,
            sizeof(number),
            "%d.%d C",
            whole,
            decimal
        );
    }


    DrawText(
        0,
        4,
        number
    );


    FlushBuffer();
}


/* ============================================================
 * Humidity
 * ============================================================ */

void Display_ShowHumidity(
    float humidity)
{
    ClearBuffer();


    DrawText(
        0,
        0,
        "ROOM MONITOR"
    );


    DrawText(
        0,
        2,
        "Humidity:"
    );


    int tenths =
        static_cast<int>(
            humidity *
            10.0f
        );


    int whole =
        tenths / 10;


    int decimal =
        tenths % 10;


    if (decimal < 0)
    {
        decimal =
            -decimal;
    }


    char number[20];


    snprintf(
        number,
        sizeof(number),
        "%d.%d %%",
        whole,
        decimal
    );


    DrawText(
        0,
        4,
        number
    );


    FlushBuffer();
}


/* ============================================================
 * Light
 * ============================================================ */

void Display_ShowLight(
    int lightLevel)
{
    ClearBuffer();


    DrawText(
        0,
        0,
        "ROOM MONITOR"
    );


    DrawText(
        0,
        2,
        "Light:"
    );


    char number[20];


    snprintf(
        number,
        sizeof(number),
        "%d %%",
        lightLevel
    );


    DrawText(
        0,
        4,
        number
    );


    FlushBuffer();
}


/* ============================================================
 * Motion
 * ============================================================ */

void Display_ShowMotion(
    bool motionDetected)
{
    ClearBuffer();


    DrawText(
        0,
        0,
        "ROOM MONITOR"
    );


    DrawText(
        0,
        2,
        "Motion:"
    );


    if (motionDetected)
    {
        DrawText(
            0,
            4,
            "ACTIVE"
        );
    }
    else
    {
        DrawText(
            0,
            4,
            "INACTIVE"
        );
    }


    FlushBuffer();
}