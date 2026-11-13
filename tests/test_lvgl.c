/** @file test_lvgl.c @brief 真实LVGL渲染到模拟ILI9488 GRAM，禁止用于固件。 */
#include "test_support.h"
#include "svc_lvgl_display.h"
#include "app_lvgl_demo.h"
static uint16_t gram[320U*480U], expected[320U*480U];
static uint16_t gram_width=320U,gram_height=480U;
static uint8_t command,params[4]; static size_t param_count;
static uint16_t x1,y1,x2,y2,x,y; static unsigned writes;
static DRV_ILI9488_Port base;
static uint32_t now(void) {return mock.now;}
static DRV_ILI9488_PortStatus cmd(void *c,uint8_t v,const DRV_ILI9488_Budget *b)
{
    command=v; param_count=0U;
    if(v==0x2CU) {x=x1;y=y1;}
    return base.write_command8(c,v,b);
}
static DRV_ILI9488_PortStatus data(void *c,const uint16_t *p,size_t n,const DRV_ILI9488_Budget *b)
{
    size_t i; DRV_ILI9488_PortStatus r=base.write_data_units(c,p,n,b); ++writes;
    if(r!=DRV_ILI9488_PORT_OK) {return r;}
    for(i=0;i<n;++i) {
        if(command==0x2AU || command==0x2BU) {
            if(param_count<4U) {params[param_count++]=(uint8_t)p[i];}
            if(param_count==4U) {
                uint16_t a=(uint16_t)((uint16_t)params[0]*256U+params[1]);
                uint16_t z=(uint16_t)((uint16_t)params[2]*256U+params[3]);
                if(command==0x2AU) {x1=a;x2=z;} else {y1=a;y2=z;}
            }
        } else if(command==0x2CU) {
            if(x>=gram_width || y>=gram_height || y>y2) {return DRV_ILI9488_PORT_IO_ERROR;}
            gram[(size_t)y*gram_width+x]=p[i];
            if(x==x2) {x=x1;++y;} else {++x;}
        }
    } return r;
}
static int setup(void)
{
    DRV_ILI9488_Port p=Mock_Port(); DRV_ILI9488_Config c=Mock_Config(DRV_ILI9488_DBI_16);
    base=p;p.write_command8=cmd;p.write_data_units=data;
    Mock_Reset();mock.capture=false;
    if(DRV_ILI9488_InitState(&p,&c)!=DRV_ILI9488_OK || DRV_ILI9488_Open(1000)!=DRV_ILI9488_OK) {return 1;}
    return 0;
}
static int render(void)
{
    mock.now+=40U;
    return SVC_LVGL_Process(1000U)==DRV_ILI9488_OK ? 0:1;
}
TEST(test_lvgl_lifecycle)
{
    CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_NOT_READY);
    CHECK(setup()==0); CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_INVALID_STATE);
    CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    CHECK(render()==0 && writes>0U);
    CHECK(APP_LVGL_DemoShowPage(1U)==DRV_ILI9488_OK);CHECK(render()==0);
    CHECK(APP_LVGL_DemoShowPage(2U)==DRV_ILI9488_INVALID_ARGUMENT);
    APP_LVGL_DemoDestroy();CHECK(SVC_LVGL_Stop()==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Stop()==DRV_ILI9488_OK); return 0;
}
TEST(test_lvgl_shorter_label)
{
    size_t i,changed=0U,visible=0U; CHECK(setup()==0);CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoSetValue(12345)==DRV_ILI9488_OK);CHECK(render()==0);
    for(i=0;i<320U*480U;++i) {expected[i]=gram[i];if(gram[i]!=0U) {++visible;}}
    CHECK(visible>20U);
    CHECK(APP_LVGL_DemoSetValue(12)==DRV_ILI9488_OK);CHECK(render()==0);
    for(i=0;i<320U*480U;++i) {if(expected[i]!=gram[i]) {++changed;}expected[i]=gram[i];}
    CHECK(changed>10U);
    APP_LVGL_DemoDestroy();CHECK(SVC_LVGL_Stop()==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoSetValue(12)==DRV_ILI9488_OK);CHECK(render()==0);
    for(i=0;i<320U*480U;++i) {CHECK(expected[i]==gram[i]);}
    return 0;
}
TEST(test_lvgl_fault_recovery)
{
    unsigned before;CHECK(setup()==0);CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    Mock_ClearTrace();mock.fail_at=1U;mock.now+=40U;
    CHECK(SVC_LVGL_Process(1000U)==DRV_ILI9488_IO_ERROR);before=writes;
    CHECK(SVC_LVGL_Process(1000U)==DRV_ILI9488_IO_ERROR && writes==before);
    CHECK(SVC_LVGL_Resume()==DRV_ILI9488_NOT_READY);
    mock.fail_at=0U;CHECK(DRV_ILI9488_Recover(1000U)==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Resume()==DRV_ILI9488_OK);CHECK(render()==0 && writes>before);
    return 0;
}

TEST(test_lvgl_timeout_and_rotation)
{
    unsigned before;CHECK(setup()==0);CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Process(0U)==DRV_ILI9488_INVALID_ARGUMENT);
    mock.io_cost=1U;mock.now+=40U;
    CHECK(SVC_LVGL_Process(3U)==DRV_ILI9488_TIMEOUT);before=writes;
    CHECK(SVC_LVGL_Process(1000U)==DRV_ILI9488_TIMEOUT && writes==before);
    mock.io_cost=0U;CHECK(DRV_ILI9488_Recover(1000U)==DRV_ILI9488_OK);
    CHECK(SVC_LVGL_Resume()==DRV_ILI9488_OK);CHECK(render()==0);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90,1000U)==DRV_ILI9488_OK);
    before=writes;CHECK(SVC_LVGL_Process(1000U)==DRV_ILI9488_NOT_READY && writes==before);
    CHECK(SVC_LVGL_Resume()==DRV_ILI9488_NOT_READY);
    APP_LVGL_DemoDestroy();CHECK(SVC_LVGL_Stop()==DRV_ILI9488_OK);return 0;
}

TEST(test_lvgl_landscape)
{
    size_t i,visible=0U; CHECK(setup()==0);
    CHECK(DRV_ILI9488_SetRotation(DRV_ILI9488_ROTATION_90,1000U)==DRV_ILI9488_OK);
    gram_width=480U;gram_height=320U;
    CHECK(SVC_LVGL_Start(now)==DRV_ILI9488_OK);CHECK(APP_LVGL_DemoCreate()==DRV_ILI9488_OK);
    CHECK(APP_LVGL_DemoSetValue(12345)==DRV_ILI9488_OK);CHECK(render()==0);
    for(i=0;i<480U*320U;++i) {if(gram[i]!=0U) {++visible;}}
    CHECK(visible>20U && writes>0U);return 0;
}
