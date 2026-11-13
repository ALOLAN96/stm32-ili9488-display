/** @file gfx_ili9488.c @brief 将Graphics行输出映射到Device BlitRect。 */
#include "gfx_ili9488.h"
#include "drv_ili9488.h"
/** 同步消费行缓冲，Device不会保留其指针。 */
static GFX_Status write_row(void *context,uint16_t x,uint16_t y,const uint16_t *pixels,
                            size_t count,uint32_t timeout)
{
    DRV_ILI9488_Rect rect={x,y,(uint16_t)count,1U}; DRV_ILI9488_Status r;
    (void)context; r=DRV_ILI9488_BlitRect(rect,pixels,count,count,timeout);
    return r==DRV_ILI9488_OK ? GFX_OK : r==DRV_ILI9488_TIMEOUT ? GFX_TIMEOUT : GFX_IO_ERROR;
}
GFX_Status GFX_ILI9488_DrawString(int16_t x,int16_t y,const char *text,size_t count,
    const GFX_TextStyle *style,uint32_t (*now_ms)(void *),uint32_t timeout)
{
    DRV_ILI9488_Info info; GFX_Surface surface;
    if(now_ms==NULL) {return GFX_INVALID_ARGUMENT;}
    if(DRV_ILI9488_GetInfo(&info)!=DRV_ILI9488_OK || info.state!=DRV_ILI9488_READY) {return GFX_IO_ERROR;}
    surface.context=NULL; surface.width=info.width; surface.height=info.height;
    surface.now_ms=now_ms; surface.write_row=write_row;
    return GFX_DrawString(&surface,x,y,text,count,&GFX_Font8x16,style,timeout);
}

/** 固定容量补齐字段，共用一个总预算；不保存任何调用者数据。 */
GFX_Status GFX_ILI9488_DrawTextField(int16_t x,int16_t y,const char *text,size_t count,
    size_t columns,const GFX_TextStyle *style,uint32_t (*now_ms)(void *),uint32_t timeout)
{
    char padded[64]; size_t i=0U,j; uint32_t start,used; GFX_TextStyle solid;
    DRV_ILI9488_Info info;
    if(text==NULL || style==NULL || now_ms==NULL || columns==0U || columns>sizeof(padded) ||
       timeout==0U || timeout>0x7FFFFFFFU || x<0 || y<0) {return GFX_INVALID_ARGUMENT;}
    start=now_ms(NULL);
    while(i<count && text[i]!='\0') {
        uint8_t ch=(uint8_t)text[i];
        if(i>=columns || ch<32U || ch>126U) {return GFX_INVALID_ARGUMENT;}
        padded[i]=text[i]; ++i;
    }
    for(j=i;j<columns;++j) {padded[j]=' ';}
    if(DRV_ILI9488_GetInfo(&info)!=DRV_ILI9488_OK || info.state!=DRV_ILI9488_READY) {return GFX_IO_ERROR;}
    if((size_t)(uint16_t)x+columns*8U>info.width || (uint32_t)(uint16_t)y+16U>info.height) {
        return GFX_INVALID_ARGUMENT;
    }
    used=(uint32_t)(now_ms(NULL)-start);
    if(used>=timeout) {return GFX_TIMEOUT;}
    solid=*style; solid.opaque=true; solid.wrap=false;
    return GFX_ILI9488_DrawString(x,y,padded,columns,&solid,now_ms,timeout-used);
}
