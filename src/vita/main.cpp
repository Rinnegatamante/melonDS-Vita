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

#include "../Config.h"
#include "../Savestate.h"
#include "../GPU.h"
#include "../NDS.h"
#include "../SPU.h"
#include "../version.h"

using namespace std;

vita2d_pgf *font;
SceCtrlData pad;
uint32_t oldpad = 0;

int _newlib_heap_size_user = 192 * 1024 * 1024;

vector<const char*> OptionDisplay =
{
    "Boot game directly",
    "Threaded 3D renderer",
    "Separate savefiles",
    "Screen rotation",
    "Screen layout"
};

vector<vector<const char*>> OptionValuesDisplay =
{
    { "Off", "On " },
    { "Off", "On " },
    { "Off", "On " },
    { "0  ", "90 ", "180", "270" },
    { "Natural   ", "Vertical  ", "Horizontal" }
};

vector<int*> OptionValues =
{
    &Config::DirectBoot,
    &Config::Threaded3D,
    &Config::SavestateRelocSRAM,
    &Config::ScreenRotation,
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
                        vita2d_printf(0xFF00FFFF, "%s   %s", OptionDisplay[i], OptionValuesDisplay[i][*OptionValues[i]]);
                    }
                    else
                    {
                        vita2d_printf(0xFFFFFFFF, "%s   %s", OptionDisplay[i], OptionValuesDisplay[i][*OptionValues[i]]);
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

void SetScreenLayout()
{
    /*float width, height, offset_topX, offset_botX, offset_topY, offset_botY;

    if (Config::ScreenLayout == 0)
        Config::ScreenLayout = (Config::ScreenRotation % 2 == 0) ? 1 : 2;

    if (Config::ScreenLayout == 1)
    {
        height = 1.0f;
        if (Config::ScreenRotation % 2 == 0)
            width = height * 0.75;
        else
            width = height * 0.421875;

        offset_topX = offset_botX = -width / 2;
        offset_topY = height;
        offset_botY = 0.0f;
    }
    else
    {
        if (Config::ScreenRotation % 2 == 0)
        {
            width = 1.0f;
            height = width / 0.75;
        }
        else
        {
            height = 2.0f;
            width = height * 0.421875;
        }

        offset_topX = -width;
        offset_botX = 0.0f;
        offset_topY = offset_botY = height / 2;
    }

    Vertex screens[] =
    {
        { { offset_topX + width, offset_topY - height, 0.0f }, { 1.0f, 1.0f } },
        { { offset_topX,         offset_topY - height, 0.0f }, { 0.0f, 1.0f } },
        { { offset_topX,         offset_topY,          0.0f }, { 0.0f, 0.0f } },
        { { offset_topX,         offset_topY,          0.0f }, { 0.0f, 0.0f } },
        { { offset_topX + width, offset_topY,          0.0f }, { 1.0f, 0.0f } },
        { { offset_topX + width, offset_topY - height, 0.0f }, { 1.0f, 1.0f } },

        { { offset_botX + width, offset_botY - height, 0.0f }, { 1.0f, 1.0f } },
        { { offset_botX,         offset_botY - height, 0.0f }, { 0.0f, 1.0f } },
        { { offset_botX,         offset_botY,          0.0f }, { 0.0f, 0.0f } },
        { { offset_botX,         offset_botY,          0.0f }, { 0.0f, 0.0f } },
        { { offset_botX + width, offset_botY,          0.0f }, { 1.0f, 0.0f } },
        { { offset_botX + width, offset_botY - height, 0.0f }, { 1.0f, 1.0f } }
    };

    if (Config::ScreenRotation == 1 || Config::ScreenRotation == 2)
    {
        Vertex *copy = (Vertex*)malloc(sizeof(screens));
        memcpy(copy, screens, sizeof(screens));
        memcpy(screens, &copy[6], sizeof(screens) / 2);
        memcpy(&screens[6], copy, sizeof(screens) / 2);
    }

    TouchBoundLeft = (screens[8].position[0] + 1) * 640;
    TouchBoundRight = (screens[6].position[0] + 1) * 640;
    TouchBoundTop = (-screens[8].position[1] + 1) * 360;
    TouchBoundBottom = (-screens[6].position[1] + 1) * 360;

    for (int i = 0; i < Config::ScreenRotation; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (int k = 0; k < 12; k += 6)
            {
                screens[k].position[j] = screens[k + 1].position[j];
                screens[k + 1].position[j] = screens[k + 2].position[j];
                screens[k + 2].position[j] = screens[k + 4].position[j];
                screens[k + 3].position[j] = screens[k + 4].position[j];
                screens[k + 4].position[j] = screens[k + 5].position[j];
                screens[k + 5].position[j] = screens[k].position[j];
            }
        }
    }

    glBufferData(GL_ARRAY_BUFFER, sizeof(screens), screens, GL_STATIC_DRAW);*/
}

int AdvFrame(unsigned int argc, void *args)
{
    while (true)
    {
        chrono::steady_clock::time_point start = chrono::steady_clock::now();

        sceKernelLockLwMutex(&EmuMutex, 1, NULL);
        NDS::RunFrame();
        sceKernelUnlockLwMutex(&EmuMutex, 1);
        memcpy(Framebuffer, GPU::Framebuffer, 256 * 384 * 4);

        while (chrono::duration_cast<chrono::duration<double>>(chrono::steady_clock::now() - start).count() < (float)1 / 60);
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
    vita2d_texture *gpu_buffer = vita2d_create_empty_texture_format(256, 384, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
    vita2d_texture_set_alloc_memblock_type(SCE_KERNEL_MEMBLOCK_TYPE_USER_RW);
    vita2d_texture *tex_buffer = vita2d_create_empty_texture_format(256, 384, SCE_GXM_TEXTURE_FORMAT_U8U8U8U8_ARGB);
    Framebuffer = (u32*)vita2d_texture_get_datap(tex_buffer);

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
            
            
            if (touch.report[0].x > TouchBoundLeft && touch.report[0].x < TouchBoundRight && touch.report[0].y > TouchBoundTop && touch.report[0].y < TouchBoundBottom)
            {
                int x, y;
                if (Config::ScreenRotation == 0)
                {
                    x = (touch.report[0].x - TouchBoundLeft) * 256.0f / (TouchBoundRight - TouchBoundLeft);
                    y = (touch.report[0].y - TouchBoundTop) * 256.0f / (TouchBoundRight - TouchBoundLeft);
                }
                else if (Config::ScreenRotation == 1)
                {
                    x = (touch.report[0].y - TouchBoundTop) * 192.0f / (TouchBoundRight - TouchBoundLeft);
                    y = 192 - (touch.report[0].x - TouchBoundLeft) * 192.0f / (TouchBoundRight - TouchBoundLeft);
                }
                else if (Config::ScreenRotation == 2)
                {
                    x = (touch.report[0].x - TouchBoundLeft) * -256.0f / (TouchBoundRight - TouchBoundLeft);
                    y = 192 - (touch.report[0].y - TouchBoundTop) * 256.0f / (TouchBoundRight - TouchBoundLeft);
                }
                else
                {
                    x = (touch.report[0].y - TouchBoundTop) * -192.0f / (TouchBoundRight - TouchBoundLeft);
                    y = (touch.report[0].x - TouchBoundLeft) * 192.0f / (TouchBoundRight - TouchBoundLeft);
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
        
        memcpy(vita2d_texture_get_datap(gpu_buffer),vita2d_texture_get_datap(tex_buffer),vita2d_texture_get_stride(gpu_buffer)*vita2d_texture_get_height(gpu_buffer));
        vita2d_start_drawing();
        vita2d_clear_screen();
        vita2d_draw_texture_scale(gpu_buffer, 299, 0, 1.41, 1.41);
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
}
