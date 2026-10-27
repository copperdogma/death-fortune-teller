#include "death_fault_log_core.h"
#include "death_fault_archive.h"

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace skull::faultlog;

namespace {
std::string read_file(const std::string &path) {
    FILE *file = std::fopen(path.c_str(), "rb");
    assert(file);
    std::string result;
    char buffer[512];
    size_t n;
    while ((n = std::fread(buffer, 1, sizeof(buffer), file)) != 0)
        result.append(buffer, n);
    assert(!std::ferror(file));
    assert(std::fclose(file) == 0);
    return result;
}

void write_file(const std::string &path, const char *text) {
    FILE *file = std::fopen(path.c_str(), "wb");
    assert(file);
    const size_t length = std::strlen(text);
    assert(std::fwrite(text, 1, length, file) == length);
    assert(std::fclose(file) == 0);
}

void remove_tree(const std::string &path) {
    DIR *dir = opendir(path.c_str());
    if (!dir) { unlink(path.c_str()); return; }
    while (const dirent *entry = readdir(dir)) {
        if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
        const std::string child = path + "/" + entry->d_name;
        struct stat info{};
        if (lstat(child.c_str(), &info) == 0 && S_ISDIR(info.st_mode)) remove_tree(child);
        else unlink(child.c_str());
    }
    closedir(dir);
    rmdir(path.c_str());
}

std::string temp_directory() {
    char path[] = "/tmp/skull-fault-log-test-XXXXXX";
    char *created = mkdtemp(path);
    assert(created);
    return created;
}

void test_ring_freeze_and_coalescing() {
    Core core;
    core.session = 0x1234;
    std::strcpy(core.firmware, "test-build");
    for (size_t i = 0; i < ring_lines + 3; ++i) {
        char line[64];
        std::snprintf(line, sizeof(line), "I (100) death: event-%02u", unsigned(i));
        core.capture(line, false, static_cast<int64_t>(i) * 1000);
    }
    assert(core.count == ring_lines);
    core.capture("E (900) death: first failure", true, 900000);
    assert(core.pending);
    core.materialize();core.frozen=false;
    assert(core.report.id == ((uint64_t(0x1234) << 32) | 1));
    assert(core.report.uptime_us == 900000);
    const std::string frozen(core.report.text, core.report.length);
    assert(frozen.find("event-00") == std::string::npos);
    // The fault line itself enters the ring before snapshotting and evicts event-03.
    assert(frozen.find("event-03") == std::string::npos);
    assert(frozen.find("event-04") != std::string::npos);
    assert(frozen.find("event-34") != std::string::npos);
    core.capture("E (901) skull_sd: another failure", true, 901000);
    core.capture("E (902) skull_radio: later failure", true, 902000);
    assert(core.coalesced == 2);
    assert(std::string(core.report.text, core.report.length) == frozen);
    core.acknowledged();
    assert(!core.pending);
}

void test_selection_and_bounds() {
    char output[64]{};
    bool fault = false;
    assert(!select_line("I (1) CHIP: provisioning secret", output, sizeof(output), fault));
    assert(!select_line("E (2) wifi: password=secret", output, sizeof(output), fault));
    assert(!select_line("I (2) skull_sd: Card config role=primary speaker='private'", output, sizeof(output), fault));
    assert(!select_line("E (2) death: password=secret", output, sizeof(output), fault));
    assert(select_line("\033[0;31mE (3) death: audio read failed\033[0m\nnext", output,
                       sizeof(output), fault));
    assert(fault);
    assert(std::strstr(output, "\033") == nullptr);
    assert(std::strstr(output, "audio read failed") != nullptr);
    assert(select_line("E (4) sdspi_host: data CRC mismatch", output, sizeof(output), fault));
    assert(fault);
    assert(select_line("W (4) death: Interrupted reason=Dead Stop", output, sizeof(output), fault));
    assert(!fault);
    assert(select_line("W (5) death: Interrupted reason=audio failure", output, sizeof(output), fault));
    assert(fault);

    Core core;
    std::vector<char> huge(20000, 'X');
    huge.back() = '\0';
    core.capture(huge.data(), false, 1);
    assert(std::strlen(core.recent[0]) == line_bytes - 1);
    core.capture(nullptr, true, 2);
    assert(!core.pending);
}

void test_retry_cadence() {
    Cadence cadence;
    assert(!cadence.due(0, false, true));
    assert(!cadence.due(0, true, false));
    assert(cadence.due(0, true, true));
    cadence.attempted(0); // includes unavailable-card attempts
    assert(cadence.attempts == 1);
    assert(!cadence.due(retry_us - 1, true, true));
    assert(!cadence.due(retry_us, false, true));
    assert(cadence.due(retry_us, true, true)); // recovered card is eligible at cadence
    cadence.attempted(retry_us);
    assert(!cadence.due(retry_us + 1, true, true));
    assert(cadence.attempts == 2);
}

void test_archive_rotation_and_audio_preservation(const std::string &root) {
    const std::string audio = root + "/audio";
    const std::string archive = root + "/diagnostics";
    assert(mkdir(audio.c_str(), 0755) == 0);
    const std::string wav = audio + "/keep.wav";
    write_file(wav, "original audio bytes\0with tail");
    const std::string original_audio = read_file(wav);
    for (unsigned i = 0; i < archive_slots + 3; ++i) {
        Report report;
        const int n = std::snprintf(report.text, sizeof(report.text), "report-%u\n", i);
        assert(n > 0 && static_cast<size_t>(n) < sizeof(report.text));
        report.length = static_cast<uint32_t>(n);
        assert(archive_report(archive.c_str(), i % archive_slots, report));
    }
    for (unsigned slot = 0; slot < archive_slots; ++slot) {
        const std::string contents = read_file(archive + "/fault-" + std::to_string(slot) + ".log");
        const unsigned expected = slot < 3 ? slot + archive_slots : slot;
        assert(contents == "report-" + std::to_string(expected) + "\n");
    }
    assert(read_file(wav) == original_audio);
    DIR *dir = opendir(archive.c_str());
    assert(dir);
    unsigned reports = 0;
    while (const dirent *entry = readdir(dir))
        if (!std::strncmp(entry->d_name, "fault-", 6)) ++reports;
    closedir(dir);
    assert(reports == archive_slots);
}

void test_archive_rejections_and_io_failures(const std::string &root) {
    Report report;
    const char payload[] = "frozen report\n";
    std::memcpy(report.text, payload, sizeof(payload));
    report.length = sizeof(payload) - 1;
    assert(!archive_report(nullptr, 0, report));
    assert(!archive_report(root.c_str(), archive_slots, report));
    report.length = report_bytes;
    assert(!archive_report(root.c_str(), 0, report));
    report.length = sizeof(payload) - 1;

    // The report staging open must fail cleanly when the requested parent is absent.
    const std::string absent = root + "/absent/child";
    assert(!archive_report(absent.c_str(), 0, report));

    // A directory at the fixed staging filename causes fopen("wb") to fail.
    const std::string blocked = root + "/blocked";
    assert(mkdir(blocked.c_str(), 0755) == 0);
    assert(mkdir((blocked + "/pending.tmp").c_str(), 0755) == 0);
    assert(!archive_report(blocked.c_str(), 0, report));
    assert(access((blocked + "/pending.tmp").c_str(), F_OK) == 0);

    // A directory in the target slot makes replacement fail after staging/readback;
    // the caller can retain and retry the same frozen report.
    const std::string target_blocked = root + "/target-blocked";
    assert(mkdir(target_blocked.c_str(), 0755) == 0);
    assert(mkdir((target_blocked + "/fault-0.log").c_str(), 0755) == 0);
    assert(!archive_report(target_blocked.c_str(), 0, report));
    assert(access((target_blocked + "/fault-0.log").c_str(), F_OK) == 0);
}
} // namespace

int main() {
    const std::string root = temp_directory();
    test_ring_freeze_and_coalescing();
    test_selection_and_bounds();
    test_retry_cadence();
    test_archive_rotation_and_audio_preservation(root);
    test_archive_rejections_and_io_failures(root);
    remove_tree(root);
    std::puts("fault log core/archive tests passed");
    return 0;
}
