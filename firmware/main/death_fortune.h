#pragma once
#include <string>
namespace skull::fortune {
// Boot/control-task only. Transactional bounded load, leaves no stale content on failure.
bool load(const char *path);
// Same strict parser, using the exact verified card template compiled in flash.
// Attempt only after both configured/default SD paths fail.
bool load_builtin();
std::string generate();
}
