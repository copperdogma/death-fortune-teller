#pragma once
#include "death_fault_log_core.h"
namespace skull::faultlog {
// Fixed eight slots plus one bounded temporary file; never touches other paths.
// Uses checked flush/fsync/close and readback before replacing an older slot.
bool archive_report(const char *directory,unsigned slot,const Report &report);
}
