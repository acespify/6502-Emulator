//-
//- ImGui-Addons: Memory Editor
//- v0.35
//- https://github.com/ocornut/imgui_club
//-
//- Copyright (c) 2019-2021 Omar Cornut
//-
//- MIT License
//-
//- Permission is hereby granted, free of charge, to any person obtaining a copy
//- of this software and associated documentation files (the "Software"), to deal
//- in the Software without restriction, including without limitation the rights
//- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//- copies of the Software, and to permit persons to whom the Software is
//- furnished to do so, subject to the following conditions:
//-
//- The above copyright notice and this permission notice shall be included in all
//- copies or substantial portions of the Software.
//-
//- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//- SOFTWARE.

#pragma once
#include <stdio.h>      // sprintf, scanf
#include <stdint.h>     // uint8_t, etc.
#include <sstream>      // std::stringstream
#include <iomanip>      // std::setw, std::setfill

#ifdef _MSC_VER
#define IM_PRAGMA_WARNING_PUSH __pragma(warning( push ))
#define IM_PRAGMA_WARNING_POP  __pragma(warning( pop ))
#else
#define IM_PRAGMA_WARNING_PUSH
#define IM_PRAGMA_WARNING_POP
#endif

struct MemoryEditor
{
    enum DataFormat
    {
        DataFormat_Bin = 0,
        DataFormat_Dec = 1,
        DataFormat_Hex = 2,
        DataFormat_COUNT
    };

    // Settings
    bool            Open;
    bool            ReadOnly;
    int             Cols;
    bool            OptShowOptions;
    bool            OptShowDataPreview;
    bool            OptShowHexII;
    bool            OptShowAscii;
    bool            OptGreyOutZeroes;
    bool            OptUpperCaseHex;
    int             OptMidColsCount;
    int             OptAddrDigitsCount;
    float           OptFooterExtraHeight;
    ImU32           HighlightColor;
    ImU32           ReadColor;
    ImU32           WriteColor;
    bool            (*ReadFn)(const ImU8* data, size_t off);
    void            (*WriteFn)(ImU8* data, size_t off, ImU8 d);
    bool            (*HighlightFn)(const ImU8* data, size_t off);

    // State/private
    bool            ContentsWidthChanged;
    size_t          DataPreviewAddr;
    size_t          DataEditingAddr;
    bool            DataEditingTakeFocus;
    char            DataInputBuf[32];
    char            AddrInputBuf[32];
    size_t          GotoAddr;
    size_t          HighlightMin, HighlightMax;
    int             PreviewEndianness;
    ImGuiDataType   PreviewDataType;

    MemoryEditor()
    {
        Open = true;
        ReadOnly = false;
        Cols = 16;
        OptShowOptions = true;
        OptShowDataPreview = false;
        OptShowHexII = false;
        OptShowAscii = true;
        OptGreyOutZeroes = true;
        OptUpperCaseHex = true;
        OptMidColsCount = 8;
        OptAddrDigitsCount = 0;
        OptFooterExtraHeight = 0.0f;
        HighlightColor = IM_COL32(255, 255, 255, 50);
        ReadColor = IM_COL32(255, 255, 0, 150);
        WriteColor = IM_COL32(0, 255, 0, 150);
        ReadFn = NULL;
        WriteFn = NULL;
        HighlightFn = NULL;
        ContentsWidthChanged = false;
        DataPreviewAddr = (size_t)-1;
        DataEditingAddr = (size_t)-1;
        DataEditingTakeFocus = false;
        memset(DataInputBuf, 0, sizeof(DataInputBuf));
        memset(AddrInputBuf, 0, sizeof(AddrInputBuf));
        GotoAddr = (size_t)-1;
        HighlightMin = HighlightMax = (size_t)-1;
        PreviewEndianness = 0;
        PreviewDataType = ImGuiDataType_S32;
    }

    void DrawContents(void* mem_data_void, size_t mem_size, size_t base_display_addr = 0x0000)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
        
        ImGui::BeginChild("##scrolling", ImVec2(0, 0), false, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
        
        const float line_height = ImGui::GetTextLineHeight();
        const int line_total_count = (int)((mem_size + Cols - 1) / Cols);
        ImGuiListClipper clipper;
        clipper.Begin(line_total_count, line_height);
        while (clipper.Step())
        {
            for (int line_i = clipper.DisplayStart; line_i < clipper.DisplayEnd; line_i++)
            {
                size_t line_addr = (size_t)(line_i * Cols);

                // Print address
                ImGui::Text("%0*X: ", OptAddrDigitsCount ? OptAddrDigitsCount : 4, (int)(base_display_addr + line_addr));
                ImGui::SameLine();

                // Build and print hex values for the line
                std::stringstream hex_ss;
                for (int n = 0; n < Cols; n++)
                {
                    if (n > 0 && OptMidColsCount > 0 && n % OptMidColsCount == 0)
                        hex_ss << " "; // Add mid-column gap

                    if (line_addr + n < mem_size)
                    {
                        uint8_t byte = ((ImU8*)mem_data_void)[line_addr + n];
                        hex_ss << " " << (OptUpperCaseHex ? std::uppercase : std::nouppercase) 
                               << std::hex << std::setw(2) << std::setfill('0') << (int)byte;
                    }
                }
                ImGui::TextUnformatted(hex_ss.str().c_str());

                // Build and print ASCII values for the line
                if (OptShowAscii)
                {
                    ImGui::SameLine();
                    std::stringstream ascii_ss;
                    ascii_ss << "  |"; // Separator
                    for (int n = 0; n < Cols; n++)
                    {
                        if (line_addr + n < mem_size)
                        {
                            unsigned char c = ((ImU8*)mem_data_void)[line_addr + n];
                            ascii_ss << (char)((c >= 32 && c < 127) ? c : '.');
                        }
                    }
                    ascii_ss << "|";
                    ImGui::TextUnformatted(ascii_ss.str().c_str());
                }
            }
        }
        clipper.End();
        
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    
    void DrawWindow(const char* title, void* mem_data, size_t mem_size, size_t base_display_addr = 0x0000)
    {
        if (!ImGui::Begin(title, &Open))
        {
            ImGui::End();
            return;
        }
        DrawContents(mem_data, mem_size, base_display_addr);
        ImGui::End();
    }
};