#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <memory>

#include "output2.h"
#include "logger2.h"
#include "module2.h"

// ID定義
constexpr int IDC_EDIT_BITRATE = 1001;
constexpr int IDC_EDIT_SAMPLERATE = 1002;
constexpr int IDD_CONFIG_DIALOG = 101;

namespace {
    // 内部設定用構造体
    struct AudioConfig {
        int mp3_bitrate = 192;
        int samplerate = 44100;
    } g_config;

    // 内部グローバル変数
    LOG_HANDLE* g_logger = nullptr;
    HINSTANCE   g_module = nullptr;

    // 静的文字列リソース（DLLがアンロードされるまで有効）
    const wchar_t* const PLUGIN_NAME = L"音声出力";
    const wchar_t* const FILE_FILTER = L"WAV file (*.wav)\0*.wav\0MP3 file (*.mp3)\0*.mp3\0\0";
    const wchar_t* const INFO_STR = L"AudioEnc v1.0.0";

    // ログ出力ラッパー
    void Log(const std::wstring& msg) {
        if (g_logger && g_logger->info) {
            g_logger->info(g_logger, msg.c_str());
        }
    }

    // パイプ自動クローズ用RAIIクラス
    struct PipeDeleter {
        void operator()(FILE* fp) const { if (fp) _pclose(fp); }
    };
    using unique_pipe = std::unique_ptr<FILE, PipeDeleter>;
}

//---------------------------------------------------------
// 設定ダイアログ
//---------------------------------------------------------
INT_PTR CALLBACK ConfigDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_INITDIALOG:
        SetDlgItemInt(hwnd, IDC_EDIT_BITRATE, g_config.mp3_bitrate, FALSE);
        SetDlgItemInt(hwnd, IDC_EDIT_SAMPLERATE, g_config.samplerate, FALSE);
        return TRUE;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDOK:
            g_config.mp3_bitrate = GetDlgItemInt(hwnd, IDC_EDIT_BITRATE, nullptr, FALSE);
            g_config.samplerate = GetDlgItemInt(hwnd, IDC_EDIT_SAMPLERATE, nullptr, FALSE);
            [[fallthrough]];
        case IDCANCEL:
            EndDialog(hwnd, LOWORD(wParam));
            return TRUE;
        }
        break;
    }
    return FALSE;
}

bool ConfigFunc(HWND hwnd, HINSTANCE hinst) {
    return DialogBox(hinst, MAKEINTRESOURCE(IDD_CONFIG_DIALOG), hwnd, ConfigDlgProc) == IDOK;
}

//---------------------------------------------------------
// 出力ロジック
//---------------------------------------------------------
bool OutputFunc(OUTPUT_INFO* oi) {
    if (!oi) return false;

    Log(L"Encoding process started...");

    // 拡張子の判別
    std::wstring savePath = oi->savefile;
    auto to_lower = [](std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), ::towlower);
        return s;
        };
    bool isMp3 = (to_lower(savePath).find(L".mp3") != std::wstring::npos);

    // マルチバイト変換
    char pathA[MAX_PATH];
    if (!WideCharToMultiByte(CP_ACP, 0, savePath.c_str(), -1, pathA, MAX_PATH, nullptr, nullptr)) {
        Log(L"Error: Failed to convert file path.");
        return false;
    }

    // ffmpegコマンド構築
    std::string cmd = "ffmpeg -y -f f32le -ar " + std::to_string(g_config.samplerate) +
        " -ac " + std::to_string(oi->audio_ch) + " -i - ";

    if (isMp3) {
        cmd += "-b:a " + std::to_string(g_config.mp3_bitrate) + "k ";
    }
    cmd += "\"" + std::string(pathA) + "\"";

    // パイプオープン
    unique_pipe fp(_popen(cmd.c_str(), "wb"));
    if (!fp) {
        Log(L"Error: Failed to launch ffmpeg. Please check your system PATH.");
        return false;
    }

    // チャンク単位での書き込み
    constexpr int CHUNK_SIZE = 44100;
    for (int i = 0; i < oi->audio_n; i += CHUNK_SIZE) {
        int n = (std::min)(CHUNK_SIZE, oi->audio_n - i);
        int read_samples = 0;

        // 音声データ取得 (3 = float32)
        float* audioBuf = static_cast<float*>(oi->func_get_audio(i, n, &read_samples, 3));

        if (audioBuf && read_samples > 0) {
            size_t elements_to_write = static_cast<size_t>(read_samples) * oi->audio_ch;
            if (fwrite(audioBuf, sizeof(float), elements_to_write, fp.get()) < elements_to_write) {
                Log(L"Error: Pipe write failed. ffmpeg might have crashed.");
                return false;
            }
        }
    }

    fp.reset(); // 明示的に _pclose を呼ぶ
    Log(L"Output completed successfully.");
    return true;
}

//---------------------------------------------------------
// エクスポート関数
//---------------------------------------------------------
extern "C" {
    __declspec(dllexport) OUTPUT_PLUGIN_TABLE* GetOutputPluginTable() {
        static OUTPUT_PLUGIN_TABLE table = {};
        table.flag = OUTPUT_PLUGIN_TABLE::FLAG_AUDIO;
        table.name = PLUGIN_NAME;
        table.filefilter = FILE_FILTER;
        table.func_output = OutputFunc;
        table.func_config = ConfigFunc;
        table.information = INFO_STR;
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