/** @file gfx_text.c @brief 有界文字扫描、裁剪和逐行同步输出。 */
#include "gfx_text.h"
/** 检查总预算，使用模2^32差值；调用不得跨完整计数周期。 */
static uint32_t remaining(const GFX_Surface *s,uint32_t start,uint32_t budget)
{
    uint32_t used=(uint32_t)(s->now_ms(s->context)-start);
    return used<budget ? budget-used : 0U;
}
GFX_Status GFX_DrawString(const GFX_Surface *s,int16_t origin_x,int16_t origin_y,
    const char *text,size_t count,const GFX_Font *f,const GFX_TextStyle *style,uint32_t budget)
{
    uint32_t start,left; size_t len=0U,i,row,col,stride,glyph;
    int32_t x=origin_x,y=origin_y; uint16_t pixels[16];
    if(s==NULL || text==NULL || f==NULL || style==NULL || s->now_ms==NULL ||
       s->write_row==NULL || s->width==0U || s->height==0U || budget==0U || budget>0x7FFFFFFFU ||
       f->data==NULL || f->width==0U || f->width>16U || f->height==0U ||
       f->first>f->last || f->last>126U || f->first<32U) {return GFX_INVALID_ARGUMENT;}
    start=s->now_ms(s->context);
    stride=((size_t)f->width+7U)/8U;
    if(f->data_bytes<((size_t)f->last-f->first+1U)*f->height*stride) {return GFX_INVALID_ARGUMENT;}
    /* count有界，不使用strlen。扫描本身也受同一个时限约束。 */
    while(len<count && text[len]!='\0') {
        uint8_t ch=(uint8_t)text[len];
        if(ch!=10U && ch!=13U && (ch<f->first || ch>f->last)) {return GFX_INVALID_ARGUMENT;}
        if(remaining(s,start,budget)==0U) {return GFX_TIMEOUT;} ++len;
    }
    for(i=0U;i<len;++i) {
        uint8_t ch=(uint8_t)text[i];
        if(remaining(s,start,budget)==0U) {return GFX_TIMEOUT;}
        if(ch==13U) {x=origin_x;continue;}
        if(ch==10U) {x=origin_x;y+=f->height;}
        else {
            if(style->wrap && x!=origin_x && x+f->width>s->width) {x=origin_x;y+=f->height;}
            if(y>=s->height) {break;}
            glyph=((size_t)ch-f->first)*f->height*stride;
            for(row=0U;row<f->height;++row) {
                int32_t py=y+(int32_t)row;
                if(py<0 || py>=s->height) {continue;}
                col=0U;
                while(col<f->width) {
                    size_t n=0U; int32_t px=x+(int32_t)col; uint16_t begin;
                    bool on=(f->data[glyph+row*stride+col/8U] & (uint8_t)(0x80U>>(col%8U)))!=0U;
                    if(px<0 || px>=s->width || (!style->opaque && !on)) {++col;continue;}
                    begin=(uint16_t)px;
                    do {
                        on=(f->data[glyph+row*stride+col/8U] & (uint8_t)(0x80U>>(col%8U)))!=0U;
                        if(!style->opaque && !on) {break;}
                        pixels[n++]=on ? style->foreground : style->background; ++col;
                    } while(col<f->width && x+(int32_t)col<s->width);
                    left=remaining(s,start,budget);
                    if(left==0U) {return GFX_TIMEOUT;}
                    {
                        GFX_Status result=s->write_row(s->context,begin,(uint16_t)py,pixels,n,left);
                        if(result!=GFX_OK) {return result;}
                    }
                    if(remaining(s,start,budget)==0U) {return GFX_TIMEOUT;}
                }
            }
            /* Saturate invisible horizontal cursor to avoid overflow on long strings. */
            if(x<65536) {x+=f->width;}
        }
        if(y>=s->height) {break;}
    }
    return remaining(s,start,budget)==0U ? GFX_TIMEOUT : GFX_OK;
}
