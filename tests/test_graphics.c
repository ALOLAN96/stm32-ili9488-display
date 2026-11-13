/** @file test_graphics.c @brief 检查实际像素、裁剪、换行、预算和器件适配。 */
#include "test_support.h"
#include "gfx_text.h"
#include "gfx_ili9488.h"
static uint16_t image[32][32];
static uint32_t tick; static unsigned calls; static bool fail;
static uint32_t clock_ms(void *c) { (void)c; return tick; }
static GFX_Status row(void *c,uint16_t x,uint16_t y,const uint16_t *p,size_t n,uint32_t timeout)
{
    size_t i; (void)c; (void)timeout; ++calls; ++tick;
    if(fail) {return GFX_IO_ERROR;}
    for(i=0;i<n;++i) {image[y][x+i]=p[i];} return GFX_OK;
}
TEST(test_text_pixels_clip_newline)
{
    GFX_Surface s={NULL,16,32,clock_ms,row}; GFX_TextStyle style={0xFFFF,0,true,false};
    tick=0;calls=0;
    CHECK(GFX_DrawString(&s,0,0,"A\nB",3,&GFX_Font8x16,&style,1000)==GFX_OK);
    CHECK(calls==32U);
    /* Source A column0=001Fh/ F000h: compare independent source bits below. */
    CHECK(image[0][0]==0U);
    CHECK(GFX_DrawString(&s,-7,-15,"A",1,&GFX_Font8x16,&style,100)==GFX_OK);
    CHECK(calls==33U);
    return 0;
}
TEST(test_text_reject_and_timeout)
{
    GFX_Surface s={NULL,16,32,clock_ms,row}; GFX_TextStyle style={1,0,true,false};
    tick=0;calls=0;
    CHECK(GFX_DrawString(&s,0,0,"A",1,&GFX_Font8x16,&style,0)==GFX_INVALID_ARGUMENT);
    CHECK(GFX_DrawString(&s,0,0,"\x80",1,&GFX_Font8x16,&style,100)==GFX_INVALID_ARGUMENT);
    CHECK(calls==0U);
    CHECK(GFX_DrawString(&s,0,0,"A",1,&GFX_Font8x16,&style,3)==GFX_TIMEOUT);
    CHECK(calls==3U);
    calls=0;fail=true;
    CHECK(GFX_DrawString(&s,0,0,"A",1,&GFX_Font8x16,&style,100)==GFX_IO_ERROR);
    CHECK(calls==1U); return 0;
}
TEST(test_text_device_adapter)
{
    DRV_ILI9488_Port p=Mock_Port(); DRV_ILI9488_Config c=Mock_Config(DRV_ILI9488_DBI_16);
    GFX_TextStyle style={0xF800,0,true,false};
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000)==DRV_ILI9488_OK); Mock_ClearTrace();
    CHECK(GFX_ILI9488_DrawString(0,0,"Hi!",3,&style,NULL,1000)==GFX_INVALID_ARGUMENT);
    CHECK(GFX_ILI9488_DrawString(0,0,"Hi!",3,&style,clock_ms,1000)==GFX_OK);
    CHECK(mock.calls>0U && !mock.overflow); return 0;
}

TEST(test_text_custom_font_transparency_wrap)
{
    static const uint8_t bits[]={0x80U,0x40U};
    GFX_Font font={2,2,65,65,bits,sizeof(bits)};
    GFX_Surface s={NULL,3,4,clock_ms,row}; GFX_TextStyle style={9,4,true,true};
    tick=0; calls=0;
    CHECK(GFX_DrawString(&s,0,0,"AA",2,&font,&style,100)==GFX_OK);
    CHECK(image[0][0]==9U && image[0][1]==4U && image[1][0]==4U && image[1][1]==9U);
    CHECK(image[2][0]==9U && image[3][1]==9U && calls==4U);
    style.opaque=false; style.foreground=7U;
    CHECK(GFX_DrawString(&s,0,0,"A",1,&font,&style,100)==GFX_OK);
    CHECK(image[0][0]==7U && image[0][1]==4U && image[1][0]==4U && image[1][1]==7U);
    tick=0xFFFFFFFEU;
    CHECK(GFX_DrawString(&s,0,0,"A",1,&font,&style,2)==GFX_TIMEOUT);
    return 0;
}

/** 短文本的末尾三个字符必须实际写背景，且不受透明/换行选项影响。 */
TEST(test_text_field_erases_tail)
{
    DRV_ILI9488_Port p=Mock_Port(); DRV_ILI9488_Config c=Mock_Config(DRV_ILI9488_DBI_16);
    GFX_TextStyle style={0xF800,0x1234,false,true}; size_t e,j,rows=0U; bool pixels=false;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000)==DRV_ILI9488_OK);
    CHECK(GFX_ILI9488_DrawTextField(0,0,"12345",5,5,&style,clock_ms,1000)==GFX_OK);
    Mock_ClearTrace();
    CHECK(GFX_ILI9488_DrawTextField(0,0,"12",2,5,&style,clock_ms,1000)==GFX_OK);
    for(e=0U;e<mock.events_count;++e) {
        MockEvent *v=&mock.events[e];
        if(v->kind==MOCK_CMD) {pixels=v->value==0x2CU;}
        else if(v->kind==MOCK_DATA && pixels) {
            if(rows>=32U) {for(j=0U;j<v->count;++j) {CHECK(mock.units[v->value+j]==0x1234U);}}
            ++rows;
        }
    }
    CHECK(rows==80U); Mock_ClearTrace();
    CHECK(GFX_ILI9488_DrawTextField(0,0,"123456",6,5,&style,clock_ms,1000)==GFX_INVALID_ARGUMENT);
    CHECK(GFX_ILI9488_DrawTextField(0,0,"A\n",2,5,&style,clock_ms,1000)==GFX_INVALID_ARGUMENT);
    CHECK(GFX_ILI9488_DrawTextField(0,0,"",0,0,&style,clock_ms,1000)==GFX_INVALID_ARGUMENT);
    CHECK(mock.calls==0U);
    CHECK(GFX_ILI9488_DrawTextField(0,0,"",0,5,&style,clock_ms,1000)==GFX_OK);
    return 0;
}
