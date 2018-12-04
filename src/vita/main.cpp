/*
    Copyright 2018 Hydr8gon

    This file is part of melonDS.

    melonDS is free software: you can redistribute it and/or modify it under
    the terms of the GNU General Public License as published by the Free
    Software Foundation, either version 3 of the License, or (at your option)
    any later version.

    melonDS is distributed in the hope that it will be useful, but WITHOUT ANY
    WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
    FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with melonDS. If not, see http://www.gnu.org/licenses/.
*/

#include <algorithm>
#include <chrono>
#include <dirent.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <vitasdk.h>
#include <vita2d.h>
#include <vector>

// Deal with conflicting typedefs
#define u64 u64_
#define s64 s64_

#define SCREEN_W 960.0f
#define SCREEN_H 544.0f

#include "../Config.h"
#include "../Savestate.h"
#include "../GPU.h"
#include "../NDS.h"
#include "../SPU.h"
#include "../version.h"

#define lerp(value, from_max, to_max) ((((value*10) * (to_max*10))/(from_max*10))/10)

using namespace std;

vita2d_pgf *font;
vita2d_texture *vram_buffer;
SceCtrlData pad;
uint32_t oldpad = 0;

int _newlib_heap_size_user = 330 * 1024 * 1024;
int screen_x[2], screen_y[2];
float screen_scale;
void (*drawFunc)();

vector<const char*> OptionDisplay =
{
    "Boot game directly",
    "Threaded 3D renderer",
    "Separate savefiles",
    "Screen layout"
};

vector<vector<const char*>> OptionValuesDisplay =
{
    { "Off", "On " },
    { "Off", "On " },
    { "Off", "On " },
    { "Standard", "Side by Side", "Vertical Rotated" }
};

vector<int*> OptionValues =
{
    &Config::DirectBoot,
    &Config::Threaded3D,
    &Config::SavestateRelocSRAM,
    &Config::ScreenLayout
};

u8 *BufferData[2];
uint8_t AudioIdx = 0;
//AudioOutBuffer AudioBuffer, *ReleasedBuffer;

u32 *Framebuffer;
unsigned int TouchBoundLeft, TouchBoundRight, TouchBoundTop, TouchBoundBottom;

SceKernelLwMutexWork EmuMutex __attribute__ ((aligned (8)));

bool checkPressed(uint32_t button){
    return (pad.buttons & button) && (!(oldpad & button));
}

bool checkReleased(uint32_t button){
    return (oldpad & button) && (!(pad.buttons & button));
}

int y_printf = 60;
void vita2d_printf(unsigned int color,  const char *format, ...) {
    __gnuc_va_list arg;
    int done;
    va_start(arg, format);
    char msg[512];
    done = vsprintf(msg, format, arg);
    va_end(arg);
    vita2d_pgf_draw_text(font, 5, y_printf, color, 1.0, msg);
    y_printf += 20;
}

string Menu()
{
    string rompath = "ux0:/data/melonDS/";
    bool options = false;

    while (rompath.find(".nds", (rompath.length() - 4)) == string::npos)
    {
        
        unsigned int selection = 0;
        vector<string> files;

        SceUID dir = sceIoDopen(rompath.c_str());
        SceIoDirent entry;
        while (sceIoDread(dir, &entry) > 0)
        {
            string name = entry.d_name;
            if (SCE_S_ISDIR(entry.d_stat.st_mode) || name.find(".nds", (name.length() - 4)) != string::npos)
                files.push_back(name);
        }
        sceIoDclose(dir);
        sort(files.begin(), files.end());

        while (true)
        {
            y_printf = 60;
            vita2d_start_drawing();
            vita2d_clear_screen();
            vita2d_pgf_draw_text(font, 5, 20, 0xFFFFFFFF, 1.0, "melonDS " MELONDS_VERSION);
            sceCtrlPeekBufferPositive(0, &pad, 1);

            if (options)
            {
                if (checkPressed(SCE_CTRL_CROSS))
                {
                    (*OptionValues[selection])++;
                    if (*OptionValues[selection] >= (int)OptionValuesDisplay[selection].size())
                        *OptionValues[selection] = 0;
                }
                else if (checkPressed(SCE_CTRL_UP) && selection > 0)
                {
                    selection--;
                }
                else if (checkPressed(SCE_CTRL_DOWN) && selection < OptionDisplay.size() - 1)
                {
                    selection++;
                }
                else if (checkPressed(SCE_CTRL_SQUARE))
                {
                    Config::Save();
                    options = false;
                    break;
                }

                for (unsigned int i = 0; i < OptionDisplay.size(); i++)
                {
                    if (i == selection)
                    {
                        vita2d_printf(0xFF00FFFF, "%s: %s", OptionDisplay[i], OptionValuesDisplay[i][*OptionValues[i]]);
                    }
                    else
                    {
                        vita2d_printf(0xFFFFFFFF, "%s: %s", OptionDisplay[i], OptionValuesDisplay[i][*OptionValues[i]]);
                    }
                }
                vita2d_printf(0xFFFFFFFF, "");
                vita2d_printf(0xFFFFFFFF, "Press Square to return to the file browser.");
            }
            else
            {
                if (checkPressed(SCE_CTRL_CROSS) && files.size() > 0)
                {
                    rompath += "/" + files[selection];
                    break;
                }
                else if (checkPressed(SCE_CTRL_CIRCLE) && rompath != "ux0:/data/melonDS/")
                {
                    rompath = rompath.substr(0, rompath.rfind("/"));
                    break;
                }
                else if (checkPressed(SCE_CTRL_UP) && selection > 0)
                {
                    selection--;
                }
                else if (checkPressed(SCE_CTRL_DOWN) && selection < files.size() - 1)
                {
                    selection++;
                }
                else if (checkPressed(SCE_CTRL_SQUARE))
                {
                    Config::Load();
                    options = true;
                    break;
                }

                for (unsigned int i = 0; i < files.size(); i++)
                {
                    if (i == selection)
                        vita2d_printf(0xFF00FFFF, "%s", files[i].c_str());
                    else
                        vita2d_printf(0xFFFFFFFF, "%s", files[i].c_str());
                }
                
                vita2d_printf(0xFFFFFFFF, "");
                vita2d_printf(0xFFFFFFFF, "Press Square to open the options menu.");
            }

            oldpad = pad.buttons;
            vita2d_end_drawing();
            vita2d_wait_rendering_done();
            vita2d_swap_buffers();
            
        }
    }
    
    vita2d_end_drawing();
    vita2d_wait_rendering_done();
    vita2d_swap_buffers();

    return rompath;
}

void drawSingle(){
    vita2d_draw_texture_scale(vram_buffer, screen_x[0], screen_y[0], screen_scale, screen_scale);
}

void drawDouble(){
    vita2d_draw_texture_part_scale(vram_buffer, screen_x[0], screen_y[0], 0, 0, 256, 192, screen_scale, screen_scale);
    vita2d_draw_texture_part_scale(vram_buffer, screen_x[1], screen_y[1], 0, 192, 256, 192, screen_scale, screen_scale);
}

void drawSingleRotated(){
    vita2d_draw_texture_scale_rotate_hotspot(vram_buffer, screen_x[0], screen_y[0], screen_scale, screen_scale, 4.71239f, 0, 0);
}

void SetScreenLayout()
{
    switch (Config::ScreenLayout){
        case 0:
            drawFunc = drawSingle;
            screen_scale = SCREEN_H / (192 * 2);
            screen_x[0] = (SCREEN_W - 256 * screen_scale) / 2;
            screen_y[0] = 0;
            TouchBoundLeft = screen_x[0];
            TouchBoundRight = TouchBoundLeft + 256 * screen_scale;
            TouchBoundTop = SCREEN_H / 2;
            TouchBoundBottom = SCREEN_H;
            break;
        case 1:
            drawFunc = drawDouble;
            screen_scale = SCREEN_W / (256 * 2);
            screen_x[0] = 0;
            screen_x[1] = 256 * screen_scale;
            screen_y[0] = screen_y[1] = (SCREEN_H - 192 * screen_scale) / 2;
            TouchBoundLeft = screen_x[1];
            TouchBoundRight = TouchBoundLeft + 256 * screen_scale;
            TouchBoundTop = screen_y[0];
            TouchBoundBottom = TouchBoundTop + 192 * screen_scale;
            break;
        case 2:
            drawFunc = drawSingleRotated;
            screen_scale = SCREEN_H / 256;
            screen_x[0] = (SCREEN_W - (192 * 2 * screen_scale)) / 2;
            screen_y[0] = SCREEN_H - (SCREEN_H - 256 * screen_scale);
            TouchBoundLeft = screen_x[0] + 192 * screen_scale;
            TouchBoundRight = TouchBoundLeft + 192 * screen_scale;
            TouchBoundTop = 0;
            TouchBoundBottom = SCREEN_H;
            break;
        default:
            break;
    }
}

int AdvFrame(unsigned int argc, void *args)
{
    while (true)
    {
        //chrono::steady_clock::time_point start = chrono::steady_clock::now();

        sceKernelLockLwMutex(&EmuMutex, 1, NULL);
        NDS::RunFrame();
        sceKernelUnlockLwMutex(&EmuMutex, 1);
        memcpy(Framebuffer, GPU::Framebuffer, 256 * 384 * 4);

        //while (chrono::duration_cast<chrono::duration<double>>(chrono::steady_clock::now() - start).count() < (float)1 / 60);
    }
    return 0;
}

void FillAudioBuffer()
{
    s16 buf_in[700 * 2];
    s16 *buf_out = (s16*)BufferData[AudioIdx];

    int num_in = SPU::ReadOutput(buf_in, 700);
    int num_out = 1024;

    int margin = 6;
    if (num_in < 700 - margin)
    {
        int last = num_in - 1;
        if (last < 0)
            last = 0;

        for (int i = num_in; i < 700 - margin; i++)
            ((u32*)buf_in)[i] = ((u32*)buf_in)[last];

        num_in = 700 - margin;
    }

    float res_incr = (float)num_in / num_out;
    float res_timer = 0;
    int res_pos = 0;

    for (int i = 0; i < 1024; i++)
    {
        buf_out[i * 2] = buf_in[res_pos * 2];
        buf_out[i * 2 + 1] = buf_in[res_pos * 2 + 1];

        res_timer += res_incr;
        while (res_timer >= 1)
        {
            res_timer--;
            res_pos++;
        }
    }
}

int PlayAudio(unsigned int argc, void *argv)
{
    int ch = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, 1024, 48000, SCE_AUDIO_OUT_MODE_STEREO);
    
    while (true)
    {
        FillAudioBuffer();
        sceAudioOutOutput(ch, BufferData[AudioIdx]);
        AudioIdx = (AudioIdx + 1) % 2;
    }
    return 0;
}

int melon_main(unsigned int argc, void *argv)
{
    vita2d_init();
    vita2d_set_vblank_wait(0);
    font = vita2d_load_default_pgf();
    sceKernelCreateLwMutex(&EmuMutex, "Emu Mutex", 0, 0, NULL);
    string rompath = Menu();
    string srampath = rompath.substr(0, rompath.rfind(".")) + ".sav";
    string statepath = rompath.substr(0, rompath.rfind(".")) + ".mln";

    Config::Load();
    if (!Config::HasConfigFile("bios7.bin") || !Config::HasConfigFile("bios9.bin") || !Config::HasConfigFile("firmware.bin"))
    {
        while (true){
            vita2d_start_drawing();
            vita2d_clear_screen();
            vita2d_pgf_draw_text(font, 5, 20, 0xFFFFFFFF, 1.0, "One or more of the following required files don't exist or couldn't be accessed:");
            vita2d_pgf_draw_text(font, 5, 40, 0xFFFFFFFF, 1.0, "bios7.bin -- ARM7 BIOS");
            vita2d_pgf_draw_text(font, 5, 60, 0xFFFFFFFF, 1.0, "bios9.bin -- ARM9 BIOS");
            vita2d_pgf_draw_text(font, 5, 80, 0xFFFFFFFF, 1.0, "firmware.bin -- firmware image");
            vita2d_pgf_draw_text(font, 5, 150, 0xFFFFFFFF, 1.0, "Dump the files from your DS and place them in ux0:/data/melonDS");
            
            
            vita2d_end_drawing();
            vita2d_wait_rendering_done();
            vita2d_swap_buffers();
        }
    }

    NDS::Init();
    if (!NDS::LoadROM(rompath.c_str(), srampath.c_str(), Config::DirectBoot))
    {
        while (true){
            vita2d_start_drawing();
            vita2d_clear_screen();
            vita2d_pgf_draw_text(font, 5, 20, 0xFFFFFFFF, 1.0, "Failed to load ROM. Make sure the file can be accessed.");
            vita2d_end_drawing();
            vita2d_wait_rendering_done();
            vita2d_swap_buffers();
        }
    }

    sceTouchSetSamplingState(SCE_TOUCH_PORT_FRONT, SCE_TOUCH_SAMPLING_STATE_START);
    scePowerSetArmClockFrequency(444);
    scePowerSetBusClockFrequency(222);
    scePowerSetGpuClockFrequency(222);
    scePowerSetGpuXbarClockFrequency(166);

    SetScreenLayout();

    SceUID main = sceKernelCreateThread("main", &AdvFrame, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(main, 0, NULL);
    
    BufferData[1] = (u8*)malloc(4096);
    BufferData[2] = (u8*)malloc(4096);
    /*SceUID audio_thd = sceKernelCreateThread("audio", &PlayAudio, 0x10000100, 0x10000, 0, 0, NULL);
    sceKernelStartThread(audio_thd, 0, NULL);*/
    
    vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_CDRAM_RW);
    vram_buffer = vita2d_create_empty_texture_format(256, 384, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
    Framebuffer = (u32*)vita2d_texture_get_datap(vram_buffer);

    uint32_t keys[] = { SCE_CTRL_CROSS, SCE_CTRL_CIRCLE, SCE_CTRL_SELECT, SCE_CTRL_START, SCE_CTRL_RIGHT, SCE_CTRL_LEFT, SCE_CTRL_UP, SCE_CTRL_DOWN, SCE_CTRL_RTRIGGER, SCE_CTRL_LTRIGGER, SCE_CTRL_SQUARE, SCE_CTRL_TRIANGLE };
    
    while (true)
    {

        sceCtrlPeekBufferPositive(0, &pad, 1);

        if (checkPressed(SCE_CTRL_LTRIGGER) || checkPressed(SCE_CTRL_RTRIGGER))
        {
            Savestate* state = new Savestate(const_cast<char*>(statepath.c_str()), checkPressed(SCE_CTRL_LTRIGGER));
            if (!state->Error)
            {
                sceKernelLockLwMutex(&EmuMutex, 1, NULL);
                NDS::DoSavestate(state);
                sceKernelUnlockLwMutex(&EmuMutex, 1);
            }
            delete state;
        }

        for (int i = 0; i < 12; i++)
        {
            if (checkPressed(keys[i]))
                NDS::PressKey(i > 9 ? i + 6 : i);
            else if (checkReleased(keys[i]))
                NDS::ReleaseKey(i > 9 ? i + 6 : i);
        }
        
        SceTouchData touch;
        sceTouchPeek(SCE_TOUCH_PORT_FRONT, &touch, 1);

        if (touch.reportNum > 0)
        {
            
            int touch_x = lerp(touch.report[0].x, 1920, 960);
            int touch_y = lerp(touch.report[0].y, 1088, 544);
            
            if (touch_x > TouchBoundLeft && touch_x < TouchBoundRight && touch_y > TouchBoundTop && touch_y < TouchBoundBottom)
            {
                int x, y;
                switch (Config::ScreenLayout){
                case 0:
                case 1:
                    x = (touch_x - TouchBoundLeft) / screen_scale;
                    y = (touch_y - TouchBoundTop) / screen_scale;
                    break;
                case 2:
                    x = (touch_y - TouchBoundLeft) / screen_scale;
                    y = (touch_x - TouchBoundTop) / screen_scale;
                    break;
                default:
                    break;
                }
                NDS::PressKey(16 + 6);
                NDS::TouchScreen(x, y);
            }
        }
        else
        {
            NDS::ReleaseKey(16 + 6);
            NDS::ReleaseScreen();
        }
        
        vita2d_start_drawing();
        vita2d_clear_screen();
        drawFunc();
        vita2d_end_drawing();
        vita2d_wait_rendering_done();
        vita2d_swap_buffers();
        
        oldpad = pad.buttons;
        
    }

    NDS::DeInit();
    vita2d_fini();
    return 0;
}

int main(int argc, char **argv){
    SceUID main_thread = sceKernelCreateThread("melonDS", melon_main, 0x40, 0x800000, 0, 0, NULL);
    if (main_thread >= 0){
        sceKernelStartThread(main_thread, 0, NULL);
        sceKernelWaitThreadEnd(main_thread, NULL, NULL);
    }
    return 0;
}
