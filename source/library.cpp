#include "library.h"
#include "id3.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace {

const size_t kMaxSongs = 4000;
const int kMaxDepth = 8;

bool has_mp3_extension(const std::string& path) {
    if (path.size() < 4) return false;
    std::string ext = path.substr(path.size() - 4);
    return to_lower_ascii(ext) == ".mp3";
}

bool is_directory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}

void add_song(std::vector<Song>& songs, const std::string& path) {
    if (songs.size() >= kMaxSongs) return;

    Song song;
    song.path = path;
    id3_read_metadata(path.c_str(), song);
    mp3_probe(path.c_str(), song);

    if (song.title.empty()) song.title = base_name_no_ext(path);
    if (song.artist.empty()) song.artist = "Unknown Artist";
    if (song.album.empty()) song.album = "Unknown Album";
    song.title = trim_text(song.title);
    song.artist = trim_text(song.artist);
    song.album = trim_text(song.album);
    if (song.title.empty()) song.title = "Untitled";
    if (song.artist.empty()) song.artist = "Unknown Artist";
    if (song.album.empty()) song.album = "Unknown Album";

    songs.push_back(std::move(song));
}

void scan_dir(std::vector<Song>& songs, const std::string& dir, int depth) {
    if (depth > kMaxDepth || songs.size() >= kMaxSongs) return;

    DIR* d = opendir(dir.c_str());
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (songs.size() >= kMaxSongs) break;
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;

        std::string path = dir;
        if (!path.empty() && path.back() != '/') path.push_back('/');
        path += ent->d_name;

        if (is_directory(path)) {
            scan_dir(songs, path, depth + 1);
        } else if (has_mp3_extension(path)) {
            add_song(songs, path);
        }
    }

    closedir(d);
}

void scan_root_files(std::vector<Song>& songs) {
    DIR* d = opendir("sdmc:/");
    if (!d) return;

    struct dirent* ent;
    while ((ent = readdir(d)) != nullptr) {
        if (songs.size() >= kMaxSongs) break;
        if (std::strcmp(ent->d_name, ".") == 0 || std::strcmp(ent->d_name, "..") == 0)
            continue;

        std::string path = "sdmc:/";
        path += ent->d_name;
        if (!is_directory(path) && has_mp3_extension(path)) add_song(songs, path);
    }

    closedir(d);
}

bool song_less(const Song& a, const Song& b) {
    std::string at = to_lower_ascii(a.title);
    std::string bt = to_lower_ascii(b.title);
    if (at != bt) return at < bt;
    std::string aa = to_lower_ascii(a.artist);
    std::string ba = to_lower_ascii(b.artist);
    if (aa != ba) return aa < ba;
    return a.path < b.path;
}

}  // namespace

size_t library_scan(std::vector<Song>& songs) {
    songs.clear();
    songs.reserve(128);

    scan_root_files(songs);

    if (is_directory("sdmc:/Music"))
        scan_dir(songs, "sdmc:/Music", 0);
    else if (is_directory("sdmc:/music"))
        scan_dir(songs, "sdmc:/music", 0);

    std::sort(songs.begin(), songs.end(), song_less);
    return songs.size();
}
