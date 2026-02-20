#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <filesystem>

#include "output2.h"
#include "module2.h"
#include "../include/resource.h"

#pragma comment(lib,"Comdlg32.lib")

namespace {

    struct AudioConfig {
        int mp3_bitrate = 192;
        int opus_bitrate = 128;
        int ogg_bitrate = 160;
        int samplerate = 48000; 
        int flac_level = 5;
        int wav_bitdepth = 16;
        std::wstring current_preset = L"default";
    } g_config;

    HINSTANCE g_module = nullptr;

    const wchar_t* INFO_STR = L"AudioEnc v1.3";
    const wchar_t* PLUGIN_NAME = L"âπê∫èoóÕ";

    const wchar_t* FILE_FILTER =
        L"WAV (*.wav)\0*.wav\0"
        L"MP3 (*.mp3)\0*.mp3\0"
        L"FLAC (*.flac)\0*.flac\0"
        L"Opus (*.opus)\0*.opus\0"
        L"OGG (*.ogg)\0*.ogg\0\0";

    const int kRates[] = { 32000, 44100, 48000, 88200, 96000 };
    const int kBitrates[] = { 64, 80, 96, 128, 160, 192, 256, 320 };


    std::filesystem::path GetIniPath() {
        auto dir = std::filesystem::absolute(L"C:\\ProgramData\\aviutl2\\Plugin");
        if (!std::filesystem::exists(dir)) std::filesystem::create_directories(dir);
        return dir / L"AudioEnc.ini";
    }


    void SaveToIni(const std::wstring& section) {
        std::wstring p = GetIniPath().wstring();
        WritePrivateProfileStringW(section.c_str(), L"mp3", std::to_wstring(g_config.mp3_bitrate).c_str(), p.c_str());
        WritePrivateProfileStringW(section.c_str(), L"opus", std::to_wstring(g_config.opus_bitrate).c_str(), p.c_str());
        WritePrivateProfileStringW(section.c_str(), L"ogg", std::to_wstring(g_config.ogg_bitrate).c_str(), p.c_str());
        WritePrivateProfileStringW(section.c_str(), L"sr", std::to_wstring(g_config.samplerate).c_str(), p.c_str());
        WritePrivateProfileStringW(section.c_str(), L"flac", std::to_wstring(g_config.flac_level).c_str(), p.c_str());
        WritePrivateProfileStringW(section.c_str(), L"wav", std::to_wstring(g_config.wav_bitdepth).c_str(), p.c_str());

        WritePrivateProfileStringW(L"Settings", L"LastPreset", section.c_str(), p.c_str());
        WritePrivateProfileStringW(nullptr, nullptr, nullptr, p.c_str());
    }

    bool LoadFromIni(const std::wstring& section) {
        std::wstring p = GetIniPath().wstring();
        if (!std::filesystem::exists(p)) return false;

        g_config.mp3_bitrate = GetPrivateProfileIntW(section.c_str(), L"mp3", 192, p.c_str());
        g_config.opus_bitrate = GetPrivateProfileIntW(section.c_str(), L"opus", 128, p.c_str());
        g_config.ogg_bitrate = GetPrivateProfileIntW(section.c_str(), L"ogg", 160, p.c_str());
        g_config.samplerate = GetPrivateProfileIntW(section.c_str(), L"sr", 48000, p.c_str());
        g_config.flac_level = GetPrivateProfileIntW(section.c_str(), L"flac", 5, p.c_str());
        g_config.wav_bitdepth = GetPrivateProfileIntW(section.c_str(), L"wav", 16, p.c_str());
        g_config.current_preset = section;
        return true;
    }

    std::vector<std::wstring> GetPresetNames() {
        std::vector<std::wstring> names;
        wchar_t buf[4096];
        if (GetPrivateProfileSectionNamesW(buf, 4096, GetIniPath().c_str()) > 0) {
            wchar_t* p = buf;
            while (*p) {
                std::wstring s(p);
                if (s != L"Settings") names.push_back(s);
                p += s.length() + 1;
            }
        }
        return names;
    }


    void SyncConfigToUI(HWND h) {
        SetDlgItemInt(h, EDIT_FLAC_LEVEL, g_config.flac_level, FALSE);
        SetDlgItemInt(h, EDIT_WAV_BITDEPTH, g_config.wav_bitdepth, FALSE);

        auto SelectValue = [&](int id, int val) {
            int count = (int)SendDlgItemMessageW(h, id, CB_GETCOUNT, 0, 0);
            for (int i = 0; i < count; i++) {
                wchar_t buf[64]{};
                SendDlgItemMessageW(h, id, CB_GETLBTEXT, i, (LPARAM)buf);
                if (_wtoi(buf) == val) { SendDlgItemMessageW(h, id, CB_SETCURSEL, i, 0); return; }
            }
            SetDlgItemTextW(h, id, std::to_wstring(val).c_str());
            };

        SelectValue(IDC_SAMPLE_RATE, g_config.samplerate);
        SelectValue(IDC_MP3_BITRATE, g_config.mp3_bitrate);
        SelectValue(IDC_OPUS_BITRATE, g_config.opus_bitrate);
        SelectValue(IDC_OGG_BITRATE, g_config.ogg_bitrate);
        SetDlgItemTextW(h, IDC_PRESET_COMBO, g_config.current_preset.c_str());
    }

    void SyncUItoConfig(HWND h) {
        wchar_t buf[128]{};
        GetDlgItemTextW(h, IDC_SAMPLE_RATE, buf, 64); g_config.samplerate = _wtoi(buf);
        GetDlgItemTextW(h, IDC_MP3_BITRATE, buf, 64);  g_config.mp3_bitrate = _wtoi(buf);
        GetDlgItemTextW(h, IDC_OPUS_BITRATE, buf, 64); g_config.opus_bitrate = _wtoi(buf);
        GetDlgItemTextW(h, IDC_OGG_BITRATE, buf, 64);  g_config.ogg_bitrate = _wtoi(buf);
        g_config.flac_level = GetDlgItemInt(h, EDIT_FLAC_LEVEL, nullptr, FALSE);
        g_config.wav_bitdepth = GetDlgItemInt(h, EDIT_WAV_BITDEPTH, nullptr, FALSE);
        GetDlgItemTextW(h, IDC_PRESET_COMBO, buf, 128);
        g_config.current_preset = (wcslen(buf) > 0) ? buf : L"default";
    }

    INT_PTR CALLBACK ConfigDlgProc(HWND h, UINT m, WPARAM w, LPARAM l) {
        switch (m) {
        case WM_INITDIALOG: {
            for (int b : kBitrates) {
                std::wstring s = std::to_wstring(b);
                SendDlgItemMessageW(h, IDC_MP3_BITRATE, CB_ADDSTRING, 0, (LPARAM)s.c_str());
                SendDlgItemMessageW(h, IDC_OPUS_BITRATE, CB_ADDSTRING, 0, (LPARAM)s.c_str());
                SendDlgItemMessageW(h, IDC_OGG_BITRATE, CB_ADDSTRING, 0, (LPARAM)s.c_str());
            }
            for (int r : kRates) {
                SendDlgItemMessageW(h, IDC_SAMPLE_RATE, CB_ADDSTRING, 0, (LPARAM)std::to_wstring(r).c_str());
            }
            auto names = GetPresetNames();
            for (auto& n : names) SendDlgItemMessageW(h, IDC_PRESET_COMBO, CB_ADDSTRING, 0, (LPARAM)n.c_str());
            SyncConfigToUI(h);
            return TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(w) == IDC_PRESET_COMBO && HIWORD(w) == CBN_SELCHANGE) {
                wchar_t name[128]{};
                int idx = (int)SendDlgItemMessageW(h, IDC_PRESET_COMBO, CB_GETCURSEL, 0, 0);
                if (idx != CB_ERR) {
                    SendDlgItemMessageW(h, IDC_PRESET_COMBO, CB_GETLBTEXT, idx, (LPARAM)name);
                    if (LoadFromIni(name)) SyncConfigToUI(h);
                }
                return TRUE;
            }
            if (LOWORD(w) == IDC_SAVE_PRESET) {
                SyncUItoConfig(h);
                SaveToIni(g_config.current_preset);
                MessageBoxW(h, L"ï€ë∂ÇµÇ‹ÇµÇΩÅB", L"AudioEnc", MB_OK);
                return TRUE;
            }
            if (LOWORD(w) == IDOK) {
                SyncUItoConfig(h);
                SaveToIni(g_config.current_preset);
                EndDialog(h, IDOK);
                return TRUE;
            }
            if (LOWORD(w) == IDCANCEL) { EndDialog(h, IDCANCEL); return TRUE; }
            break;
        }
        return FALSE;
    }

    bool ConfigFunc(HWND h, HINSTANCE i) {
        return (INT_PTR)DialogBoxW(g_module, MAKEINTRESOURCEW(IDD_CONFIG_DIALOG), h, ConfigDlgProc) == IDOK;
    }


    bool OutputFunc(OUTPUT_INFO* oi) {
        if (system("ffmpeg -version >nul 2>&1") != 0) {
            MessageBoxW(nullptr, L"ffmpeg Ç™å©Ç¬Ç©ÇËÇ‹ÇπÇÒÅB", L"AudioEnc", MB_ICONERROR);
            return false;
        }

        char out[MAX_PATH * 3]{};
        WideCharToMultiByte(CP_UTF8, 0, oi->savefile, -1, out, sizeof(out), nullptr, nullptr);

        std::filesystem::path p(oi->savefile);
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        std::string codec;
        if (ext == ".mp3")  codec = "-c:a libmp3lame -b:a " + std::to_string(g_config.mp3_bitrate) + "k";
        else if (ext == ".opus") codec = "-c:a libopus -b:a " + std::to_string(g_config.opus_bitrate) + "k";
        else if (ext == ".flac") codec = "-c:a flac -compression_level " + std::to_string(g_config.flac_level);
        else if (ext == ".ogg")  codec = "-c:a libvorbis -b:a " + std::to_string(g_config.ogg_bitrate) + "k";
        else {
            codec = (g_config.wav_bitdepth == 24) ? "-c:a pcm_s24le" :
                (g_config.wav_bitdepth == 32) ? "-c:a pcm_s32le" : "-c:a pcm_s16le";
        }

        std::string cmd = "ffmpeg -threads 0 -y -f f32le -sample_fmt flt -ar " +
            std::to_string(oi->audio_rate) + " -ac " + std::to_string(oi->audio_ch) +
            " -i - -ar " + std::to_string(g_config.samplerate) + " " + codec + " \"" + out + "\"";

        auto fp = _popen(cmd.c_str(), "wb");
        if (!fp) return false;

        constexpr int CHUNK = 4096;
        for (int i = 0; i < oi->audio_n; i += CHUNK) {
            if (oi->func_is_abort()) { _pclose(fp); return false; }
            int r = 0;
            int n = std::min(CHUNK, oi->audio_n - i);
            float* buf = (float*)oi->func_get_audio(i, n, &r, 3);
            if (buf && r > 0) fwrite(buf, sizeof(float), (size_t)r * oi->audio_ch, fp);
        }
        _pclose(fp);
        return true;
    }

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

        wchar_t last[128]{};
        GetPrivateProfileStringW(L"Settings", L"LastPreset", L"default", last, 128, GetIniPath().c_str());
        LoadFromIni(last);

        return &t;
    }
}

BOOL APIENTRY DllMain(HMODULE h, DWORD r, LPVOID) {
    if (r == DLL_PROCESS_ATTACH) {
        g_module = h;
        DisableThreadLibraryCalls(h);
    }
    return TRUE;
}