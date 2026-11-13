/** @file test_candidate.c @brief Demo候选配置的真实驱动事务与故障路径。 */
#include "test_support.h"
#include "drv_ili9488_panels.h"

/** 按命令和参数序列检查总线，不依赖分批大小。 */
static int trace(uint8_t rotation)
{
    static const uint8_t commands[] = {0xC0,0xE0,0xE1,0xC1,0xC5,0xB0,0xB1,0xB4,0xB6,0xE9,0xF7,0x3A,0x36,0x20,0x29};
    static const uint8_t lengths[] = {2,15,15,1,3,1,1,1,3,1,4,1,1,0,0};
    static const uint8_t values[] = {
        0x18,0x17,
        0,0x13,0x18,4,0x0F,6,0x3A,0x56,0x4D,3,0x0A,6,0x30,0x3E,0x0F,
        0,0x13,0x18,1,0x11,6,0x38,0x34,0x4D,6,0x0D,0x0B,0x31,0x37,0x0F,
        0x41,0,0x1A,0x80,0,0xA0,2,0,0x22,0x3B,0,0xA9,0x51,0x2C,0x82,0x55,0x08
    };
    size_t e, c=0U, n=0U, total=0U, j; bool started=false;
    for(e=0U;e<mock.events_count;++e) {
        MockEvent *v=&mock.events[e];
        if(!started) { if(v->kind!=MOCK_CMD || v->value!=0xC0U) {continue;} started=true; }
        if(v->kind==MOCK_CMD) {
            if(c>0U) { CHECK(n==lengths[c-1U]); }
            CHECK(c<sizeof(commands)); CHECK(v->value==commands[c]); ++c; n=0U;
        } else {
            CHECK(v->kind==MOCK_DATA && c>0U);
            for(j=0U;j<v->count;++j) {
                CHECK(total<sizeof(values));
                CHECK(mock.units[v->value+j]==(total==sizeof(values)-1U ? rotation : values[total]));
                ++total; ++n;
            }
        }
    }
    CHECK(c==sizeof(commands) && n==0U && total==sizeof(values));
    CHECK(!mock.overflow && !mock.budget_mismatch); return 0;
}
TEST(test_candidate_open)
{
    DRV_ILI9488_Port p=Mock_Port();
    Mock_Reset(); p.set_reset=NULL;
    CHECK(DRV_ILI9488_DemoCandidateConfig.dbi_width==DRV_ILI9488_DBI_16);
    CHECK(DRV_ILI9488_InitState(&p,&DRV_ILI9488_DemoCandidateConfig)==DRV_ILI9488_OK);
    CHECK(mock.calls==0U);
    CHECK(DRV_ILI9488_Open(1000U)==DRV_ILI9488_OK);
    CHECK(mock.events[0].value==1U && mock.events[1].kind==MOCK_WAIT);
    CHECK(mock.events[2].value==0x11U && mock.events[3].kind==MOCK_WAIT);
    CHECK(mock.events[4].time==240U); CHECK(trace(0x08U)==0); return 0;
}
TEST(test_candidate_rotations_wake_recover)
{
    static const uint8_t rotations[]={0x08,0x68,0xC8,0xA8};
    DRV_ILI9488_Port p=Mock_Port(); unsigned i; DRV_ILI9488_Info info;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p,&DRV_ILI9488_DemoCandidateConfig)==DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000U)==DRV_ILI9488_OK);
    for(i=0U;i<4U;++i) {
        CHECK(DRV_ILI9488_SetRotation((DRV_ILI9488_Rotation)i,1000U)==DRV_ILI9488_OK);
        CHECK(DRV_ILI9488_Sleep(1000U)==DRV_ILI9488_OK); Mock_ClearTrace();
        CHECK(DRV_ILI9488_Wake(1000U)==DRV_ILI9488_OK); CHECK(trace(rotations[i])==0);
        CHECK(DRV_ILI9488_GetInfo(&info)==DRV_ILI9488_OK);
        CHECK(info.width==(i%2U ? 480U : 320U));
        Mock_ClearTrace(); mock.fail_at=1U;
        CHECK(DRV_ILI9488_SetRotation((DRV_ILI9488_Rotation)((i+1U)%4U),1000U)==DRV_ILI9488_IO_ERROR);
        Mock_ClearTrace(); mock.fail_at=0U;
        CHECK(DRV_ILI9488_Recover(1000U)==DRV_ILI9488_OK); CHECK(trace(rotations[i])==0);
    } return 0;
}
TEST(test_candidate_every_open_failure)
{
    DRV_ILI9488_Port p=Mock_Port(); DRV_ILI9488_Info info; size_t count,i;
    Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p,&DRV_ILI9488_DemoCandidateConfig)==DRV_ILI9488_OK);
    CHECK(DRV_ILI9488_Open(1000U)==DRV_ILI9488_OK); count=mock.calls;
    CHECK(DRV_ILI9488_Close(1000U)==DRV_ILI9488_OK);
    for(i=1U;i<=count;++i) {
        Mock_Reset(); CHECK(DRV_ILI9488_InitState(&p,&DRV_ILI9488_DemoCandidateConfig)==DRV_ILI9488_OK);
        mock.fail_at=i; CHECK(DRV_ILI9488_Open(1000U)==DRV_ILI9488_IO_ERROR);
        CHECK(mock.calls==i); CHECK(DRV_ILI9488_GetInfo(&info)==DRV_ILI9488_OK && info.state==DRV_ILI9488_FAULT);
        mock.fail_at=0U; CHECK(DRV_ILI9488_Close(1000U)==DRV_ILI9488_OK);
    } return 0;
}
TEST(test_candidate_invalid_extension)
{
    DRV_ILI9488_Panel panel=*DRV_ILI9488_DemoCandidateConfig.panel;
    ILI9488_DemoParameters extra=*panel.demo;
    DRV_ILI9488_Config c=DRV_ILI9488_DemoCandidateConfig; DRV_ILI9488_Port p=Mock_Port();
    Mock_Reset(); panel.demo=&extra; c.panel=&panel;
    extra.power2=0x42U; CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_INVALID_ARGUMENT);
    extra=*DRV_ILI9488_DemoCandidateConfig.panel->demo; extra.vcom[2]=0x81U;
    CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_INVALID_ARGUMENT);
    extra=*DRV_ILI9488_DemoCandidateConfig.panel->demo; extra.display_function[0]=0x20U;
    CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_INVALID_ARGUMENT);
    extra=*DRV_ILI9488_DemoCandidateConfig.panel->demo; extra.adjust3[0]=0U;
    CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_INVALID_ARGUMENT);
    extra=*DRV_ILI9488_DemoCandidateConfig.panel->demo; c.dbi_width=DRV_ILI9488_DBI_8;
    CHECK(DRV_ILI9488_InitState(&p,&c)==DRV_ILI9488_INVALID_ARGUMENT);
    CHECK(mock.calls==0U && mock.queries==0U); return 0;
}
