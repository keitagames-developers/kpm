#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <archive.h>
#include <archive_entry.h>
#include <filesystem>

namespace fs = std::filesystem;

// ──────────────────────────────────────────────────────────────────────────
// プログレスバー（[=========])を定義
class ProgressBar {
    size_t total;
    size_t width = 40;

  public:
    ProgressBar(size_t totalBytes) : total(totalBytes) {}

    void update(size_t current) {
        double ratio = double(current) / total;
        size_t pos = size_t(width * ratio);
        std::cout << "\r[";
        for (size_t i = 0; i < width; ++i)
            std::cout << (i < pos ? '=' : ' ');
        std::cout << "] " << int(ratio * 100) << "% (" << current << "/" << total << " B)";
        std::cout.flush();
    }

    void finish() {
        std::cout << "\n";
    }
};

// ──────────────────────────────────────────────────────────────────────────
// libcurl コールバックとダウンロード関数
struct DownloadContext {
    std::ostringstream* stream;
    ProgressBar* bar;
    size_t downloaded = 0;
};

static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    // Content-Length を読み取る
    size_t len = size * nitems;
    std::string header(buffer, len);
    auto p = header.find("Content-Length:");
    if (p != std::string::npos) {
        size_t val = std::stoull(header.substr(p + 15));
        auto ctx = static_cast<DownloadContext*>(userdata);
        ctx->bar = new ProgressBar(val);
    }
    return len;
}

static size_t write_callback(void* ptr, size_t size, size_t nmemb, void* userdata) {
    size_t chunk = size * nmemb;
    auto ctx = static_cast<DownloadContext*>(userdata);
    ctx->stream->write(static_cast<char*>(ptr), chunk);
    ctx->downloaded += chunk;
    if (ctx->bar) ctx->bar->update(ctx->downloaded);
    return chunk;
}

std::string fetchWithProgress(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::ostringstream response;
    DownloadContext ctx{&response, nullptr, 0};

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &ctx);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &ctx);
    curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (ctx.bar) ctx.bar->finish();
    return response.str();
}

// ──────────────────────────────────────────────────────────────────────────
// アーカイブ中のエントリ数を数える
int countEntries(const std::string& filename) {
    struct archive* a = archive_read_new();
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    archive_read_open_filename(a, filename.c_str(), 10240);

    struct archive_entry* entry;
    int count = 0;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        ++count;
        archive_read_data_skip(a);
    }
    archive_read_close(a);
    archive_read_free(a);
    return count;
}

// ──────────────────────────────────────────────────────────────────────────
// プログレスバー付き展開
bool extractWithProgress(const std::string& filename, const std::string& dest) {
    if (!fs::exists(dest) && !fs::create_directories(dest)) {
        std::cerr << "" << dest << "\n";
        return false;
    }

    int total = countEntries(filename);
    ProgressBar bar(total);
    int done = 0;

    struct archive* a   = archive_read_new();
    struct archive* ext = archive_write_disk_new();
    archive_read_support_format_tar(a);
    archive_read_support_filter_gzip(a);
    archive_write_disk_set_options(ext, ARCHIVE_EXTRACT_TIME);
    archive_read_open_filename(a, filename.c_str(), 10240);

    struct archive_entry* entry;
    while (archive_read_next_header(a, &entry) == ARCHIVE_OK) {
        const char* rel = archive_entry_pathname(entry);
        std::string full = dest + "/" + rel;
        archive_entry_set_pathname(entry, full.c_str());
        if (archive_write_header(ext, entry) != ARCHIVE_OK) break;

        const void* buff;
        size_t size;
        la_int64_t offset;
        while (archive_read_data_block(a, &buff, &size, &offset) == ARCHIVE_OK) {
            archive_write_data_block(ext, buff, size, offset);
        }
        archive_write_finish_entry(ext);

        ++done;
        bar.update(done);
    }

    bar.finish();
    archive_read_close(a);
    archive_read_free(a);
    archive_write_close(ext);
    archive_write_free(ext);
    return true;
}

// ──────────────────────────────────────────────────────────────────────────
// メイン処理：install コマンド
int main(int argc, char* argv[]) {
    if (argc != 3 || std::string(argv[1]) != "install") {
        std::cerr << "Usage: mypm install <manifest_url>\n";
        return 1;
    }

    try {
        // マニフェスト取得（JSON前提）
        std::string manifest = fetchWithProgress(argv[2]);
        auto js = nlohmann::json::parse(manifest);
        std::string url = js["url"];
        std::string name = js["name"];
        std::string ver  = js["version"];
        std::string tmp  = "/tmp/" + name + "-" + ver + ".tar.gz";

        // パッケージ取得
        std::cout << "パッケージをダウンロードしています...\n";
        {
            std::string data = fetchWithProgress(url);
            std::ofstream ofs(tmp, std::ios::binary);
            ofs.write(data.data(), data.size());
        }

        // 展開
        std::string dest = "./packages/" + name + "-" + ver;
        std::cout << "パッケージを抽出しています\n";
        if (!extractWithProgress(tmp, dest)) {
            std::cerr << "失敗\n";
            return 1;
        }

        std::cout << "パッケージをインストールしています" << dest << "\n";
    }
    catch (const std::exception& e) {
        std::cerr << "エラー" << e.what() << "\n";
        return 1;
    }
    return 0;
}
