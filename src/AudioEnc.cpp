#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>

#include "output2.h"
#include "logger2.h"
#include "module2.h"
#include "../include/resource.h"

namespace {
    struct AudioConfig {
        int mp3_bitrate = 192;
        int samplerate = 44100;
        int flac_level = 5;
        int opus_bitrate = 128;
        int wav_bitdepth = 16;
    } g_config;

    LOG_HANDLE* g_logger = nullptr;
    HINSTANCE   g_module = nullptr;

    const wchar_t* const PLUGIN_NAME = L"‰¹ºo—Í";
    const wchar_t* const FILE_FILTER =
        L"WAV file (*.wav)\0*.wav\0"
        L"MP3 file (*.mp3)\0*.mp3\0"
        L"FLAC file (*.flac)\0*.flac\0"
        L"Opus file (*.opus)\0*.opus\0\0";

    const wchar_t* const INFO_STR = L"AudioEnc v1.1";

    void Log(const std::wstring& msg) {
        if (g_logger && g_logger->info) {
            g_logger->info(g_logger, msg.c_str());
        }
    }

    struct PipeDeleter {
        void operator()(FILE* fp) const { if (fp) _pclose(fp); }
    };
    using unique_pipe = std::unique_ptr<FILE, PipeDeleter>;
}

INT_PTR CALLBACK ConfigDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG: {
        SetDlgItemInt(hwnd, EDIT_MP3_BITRATE, g_config.mp3_bitrate, FALSE);
        SetDlgItemInt(hwnd, EDIT_SAMPLE_RATE, g_config.samplerate, FALSE);
        SetDlgItemInt(hwnd, EDIT_FLAC_LEVEL, g_config.flac_level, FALSE);
        SetDlgItemInt(hwnd, EDIT_OPUS_BITRATE, g_config.opus_bitrate, FALSE);
        SetDlgItemInt(hwnd, EDIT_WAV_BITDEPTH, g_config.wav_bitdepth, FALSE);
        return TRUE;
    }

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK: {
            g_config.mp3_bitrate = GetDlgItemInt(hwnd, EDIT_MP3_BITRATE, nullptr, FALSE);
            g_config.samplerate = GetDlgItemInt(hwnd, EDIT_SAMPLE_RATE, nullptr, FALSE);
            g_config.flac_level = GetDlgItemInt(hwnd, EDIT_FLAC_LEVEL, nullptr, FALSE);
            g_config.opus_bitrate = GetDlgItemInt(hwnd, EDIT_OPUS_BITRATE, nullptr, FALSE);
            g_config.wav_bitdepth = GetDlgItemInt(hwnd, EDIT_WAV_BITDEPTH, nullptr, FALSE);
            EndDialog(hwnd, IDOK);
            return TRUE;
        }
        case IDCANCEL:
            EndDialog(hwnd, IDCANCEL);
            return TRUE;
        }
        break;
    }
    return FALSE;
}
bool ConfigFunc(HWND hwnd, HINSTANCE hinst) {
    return DialogBox(hinst, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), hwnd, ConfigDlgProc) == IDOK;
}

bool OutputFunc(OUTPUT_INFO* oi) {
    if (!oi) return false;

    Log(L"Encoding process started...");

    std::wstring savePath = oi->savefile;
    auto to_lower = [](std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
        };
    std::wstring lowerPath = to_lower(savePath);

    char pathA[MAX_PATH];
    if (!WideCharToMultiByte(CP_ACP, 0, savePath.c_str(), -1, pathA, MAX_PATH, nullptr, nullptr)) {
        Log(L"Error: Failed to convert file path.");
        return false;
    }

    std::string cmd = "ffmpeg -y -f f32le -ar " + std::to_string(g_config.samplerate) +
        " -ac " + std::to_string(oi->audio_ch) + " -i - ";

    if (lowerPath.find(L".mp3") != std::wstring::npos) {
        cmd += "-c:a libmp3lame -b:a " + std::to_string(g_config.mp3_bitrate) + "k ";
    }
    else if (lowerPath.find(L".opus") != std::wstring::npos) {
        cmd += "-c:a libopus -b:a " + std::to_string(g_config.opus_bitrate) + "k ";
    }
    else if (lowerPath.find(L".flac") != std::wstring::npos) {
        cmd += "-c:a flac -compression_level " + std::to_string(g_config.flac_level) + " ";
    }
    else {
        std::string pcm_fmt = (g_config.wav_bitdepth == 24) ? "pcm_s24le" : "pcm_s16le";
        cmd += "-c:a " + pcm_fmt + " ";
    }

    cmd += "\"" + std::string(pathA) + "\"";

    unique_pipe fp(_popen(cmd.c_str(), "wb"));
    if (!fp) {
        Log(L"Error: Failed to launch ffmpeg. Please check your system PATH.");
        return false;
    }

    constexpr int CHUNK_SIZE = 44100;
    for (int i = 0; i < oi->audio_n; i += CHUNK_SIZE) {
        int n = (std::min)(CHUNK_SIZE, oi->audio_n - i);
        int read_samples = 0;
        float* audioBuf = static_cast<float*>(oi->func_get_audio(i, n, &read_samples, 3));

        if (audioBuf && read_samples > 0) {
            size_t elements_to_write = static_cast<size_t>(read_samples) * oi->audio_ch;
            if (fwrite(audioBuf, sizeof(float), elements_to_write, fp.get()) < elements_to_write) {
                Log(L"Error: Pipe write failed.");
                return false;
            }
        }
    }

    fp.reset();
    Log(L"Output completed successfully.");
    return true;
}

extern "C" {

    __declspec(dllexport) OUTPUT_PLUGIN_TABLE* GetOutputPluginTable() {
        static OUTPUT_PLUGIN_TABLE table = {};
        table.flag = OUTPUT_PLUGIN_TABLE::FLAG_AUDIO;
        table.name = PLUGIN_NAME;
        table.filefilter = FILE_FILTER;
        table.information = INFO_STR;
        table.func_output = OutputFunc;
        table.func_config = ConfigFunc;
        table.func_get_config_text = nullptr; // •K{‚Å‚Í‚È‚¢
        return &table;
    }

    __declspec(dllexport) void InitializeLogger(LOG_HANDLE* logger) {
        g_logger = logger;
    }

} 

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    if (ul_reason_for_call == DLL_PROCESS_ATTACH) {
        g_module = hModule;
        DisableThreadLibraryCalls(hModule);
    }
    return TRUE;
}
