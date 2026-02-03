#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>
#include <filesystem>

#include "output2.h"
#include "logger2.h"
#include "module2.h"
#include "../include/resource.h"

#pragma comment(lib,"Comdlg32.lib")

namespace {

    struct AudioConfig {
        int mp3_bitrate = 192;
        int samplerate = 44100;
        int flac_level = 5;
        int opus_bitrate = 128;
        int wav_bitdepth = 16;
    } g_config;

    HINSTANCE g_module = nullptr;

    const wchar_t* const INFO_STR = L"AudioEnc v1.2";
    const wchar_t* PLUGIN_NAME = L"‰¹ºo—Í";
    const wchar_t* FILE_FILTER =
        L"WAV (*.wav)\0*.wav\0MP3 (*.mp3)\0*.mp3\0FLAC (*.flac)\0*.flac\0Opus (*.opus)\0*.opus\0\0";

    std::wstring GetPresetDir() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(g_module, path, MAX_PATH);
        auto dir = std::filesystem::path(path).parent_path() / L"AudioEnc_Presets";
        std::filesystem::create_directories(dir);
        return dir.wstring();
    }

    void SavePresetFile(const wchar_t* path) {
        WritePrivateProfileStringW(L"audio", L"mp3", std::to_wstring(g_config.mp3_bitrate).c_str(), path);
        WritePrivateProfileStringW(L"audio", L"sr", std::to_wstring(g_config.samplerate).c_str(), path);
        WritePrivateProfileStringW(L"audio", L"flac", std::to_wstring(g_config.flac_level).c_str(), path);
        WritePrivateProfileStringW(L"audio", L"opus", std::to_wstring(g_config.opus_bitrate).c_str(), path);
        WritePrivateProfileStringW(L"audio", L"wav", std::to_wstring(g_config.wav_bitdepth).c_str(), path);
    }

    void LoadPreset(const std::wstring& name) {
        auto p = GetPresetDir() + L"\\" + name + L".preset";
        g_config.mp3_bitrate = GetPrivateProfileIntW(L"audio", L"mp3", 192, p.c_str());
        g_config.samplerate = GetPrivateProfileIntW(L"audio", L"sr", 44100, p.c_str());
        g_config.flac_level = GetPrivateProfileIntW(L"audio", L"flac", 5, p.c_str());
        g_config.opus_bitrate = GetPrivateProfileIntW(L"audio", L"opus", 128, p.c_str());
        g_config.wav_bitdepth = GetPrivateProfileIntW(L"audio", L"wav", 16, p.c_str());
    }

}

INT_PTR CALLBACK ConfigDlgProc(HWND h, UINT m, WPARAM w, LPARAM) {

    switch (m) {

    case WM_INITDIALOG:

        for (auto& p : std::filesystem::directory_iterator(GetPresetDir()))
            if (p.path().extension() == L".preset")
                SendDlgItemMessageW(h, IDC_PRESET_COMBO, CB_ADDSTRING, 0,
                    (LPARAM)p.path().stem().c_str());

        SetDlgItemInt(h, EDIT_MP3_BITRATE, g_config.mp3_bitrate, FALSE);
        SetDlgItemInt(h, EDIT_SAMPLE_RATE, g_config.samplerate, FALSE);
        SetDlgItemInt(h, EDIT_FLAC_LEVEL, g_config.flac_level, FALSE);
        SetDlgItemInt(h, EDIT_OPUS_BITRATE, g_config.opus_bitrate, FALSE);
        SetDlgItemInt(h, EDIT_WAV_BITDEPTH, g_config.wav_bitdepth, FALSE);

        return TRUE;

    case WM_COMMAND:

        if (LOWORD(w) == IDC_SAVE_PRESET) {

            OPENFILENAMEW ofn{};
            wchar_t file[MAX_PATH] = L"audio.preset";

            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = h;
            ofn.lpstrFilter = L"Preset (*.preset)\0*.preset\0";
            ofn.lpstrFile = file;
            ofn.nMaxFile = MAX_PATH;
            ofn.lpstrInitialDir = GetPresetDir().c_str();
            ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
            ofn.lpstrDefExt = L"preset";

            if (GetSaveFileNameW(&ofn)) {

                g_config.mp3_bitrate = GetDlgItemInt(h, EDIT_MP3_BITRATE, nullptr, FALSE);
                g_config.samplerate = GetDlgItemInt(h, EDIT_SAMPLE_RATE, nullptr, FALSE);
                g_config.flac_level = GetDlgItemInt(h, EDIT_FLAC_LEVEL, nullptr, FALSE);
                g_config.opus_bitrate = GetDlgItemInt(h, EDIT_OPUS_BITRATE, nullptr, FALSE);
                g_config.wav_bitdepth = GetDlgItemInt(h, EDIT_WAV_BITDEPTH, nullptr, FALSE);

                SavePresetFile(file);

                SendDlgItemMessageW(h, IDC_PRESET_COMBO, CB_ADDSTRING, 0,
                    (LPARAM)std::filesystem::path(file).stem().c_str());
            }
        }

        if (LOWORD(w) == IDC_PRESET_COMBO && HIWORD(w) == CBN_SELCHANGE) {

            wchar_t name[64];
            GetDlgItemTextW(h, IDC_PRESET_COMBO, name, 64);
            LoadPreset(name);

            SetDlgItemInt(h, EDIT_MP3_BITRATE, g_config.mp3_bitrate, FALSE);
            SetDlgItemInt(h, EDIT_SAMPLE_RATE, g_config.samplerate, FALSE);
            SetDlgItemInt(h, EDIT_FLAC_LEVEL, g_config.flac_level, FALSE);
            SetDlgItemInt(h, EDIT_OPUS_BITRATE, g_config.opus_bitrate, FALSE);
            SetDlgItemInt(h, EDIT_WAV_BITDEPTH, g_config.wav_bitdepth, FALSE);
        }

        if (LOWORD(w) == IDOK) EndDialog(h, IDOK);
        if (LOWORD(w) == IDCANCEL) EndDialog(h, IDCANCEL);

        return TRUE;
    }

    return FALSE;
}

bool ConfigFunc(HWND h, HINSTANCE i) {
    return DialogBox(i, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), h, ConfigDlgProc) == IDOK;
}

bool OutputFunc(OUTPUT_INFO* oi) {

    char path[MAX_PATH];
    WideCharToMultiByte(CP_ACP, 0, oi->savefile, -1, path, MAX_PATH, nullptr, nullptr);

    std::string cmd = "ffmpeg -y -f f32le -ar " + std::to_string(g_config.samplerate)
        + " -ac " + std::to_string(oi->audio_ch) + " -i - ";

    std::wstring ext = oi->savefile;
    std::transform(ext.begin(), ext.end(), ext.begin(), towlower);

    if (ext.find(L".mp3") != std::wstring::npos)
        cmd += "-c:a libmp3lame -b:a " + std::to_string(g_config.mp3_bitrate) + "k ";
    else if (ext.find(L".opus") != std::wstring::npos)
        cmd += "-c:a libopus -b:a " + std::to_string(g_config.opus_bitrate) + "k ";
    else if (ext.find(L".flac") != std::wstring::npos)
        cmd += "-c:a flac -compression_level " + std::to_string(g_config.flac_level) + " ";
    else {
        if (g_config.wav_bitdepth == 32) cmd += "-c:a pcm_f32le ";
        else if (g_config.wav_bitdepth == 24) cmd += "-c:a pcm_s24le ";
        else cmd += "-c:a pcm_s16le ";
    }

    cmd += "\"" + std::string(path) + "\"";

    auto fp = _popen(cmd.c_str(), "wb");
    if (!fp) return false;

    constexpr int CHUNK = 4096;
    for (int i = 0; i < oi->audio_n; i += CHUNK) {
        int n = std::min(CHUNK, oi->audio_n - i);
        int r = 0;
        float* buf = (float*)oi->func_get_audio(i, n, &r, 3);
        if (buf && r > 0) fwrite(buf, sizeof(float), r * oi->audio_ch, fp);
    }

    _pclose(fp);
    return true;
}

extern "C" {

    __declspec(dllexport) OUTPUT_PLUGIN_TABLE* GetOutputPluginTable() {
        static OUTPUT_PLUGIN_TABLE t{};
        t.flag = OUTPUT_PLUGIN_TABLE::FLAG_AUDIO;
        t.name = PLUGIN_NAME;
        t.filefilter = FILE_FILTER;
        t.information = INFO_STR;   
        t.func_output = OutputFunc;
        t.func_config = ConfigFunc;
        return &t;
    }

}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) g_module = h;
    return TRUE;
}
