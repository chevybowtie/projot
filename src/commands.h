#pragma once

#include "cli.h"

// Each function returns an exit code (0 = success, non-zero = error).

int cmd_init(const Args& args);
int cmd_new(const Args& args);
int cmd_add_todo(const Args& args);
int cmd_list(const Args& args);
int cmd_complete(const Args& args);
int cmd_add_note(const Args& args);
int cmd_set_link(const Args& args);
int cmd_set_app_id(const Args& args);
int cmd_add_github(const Args& args);
int cmd_add_swagger(const Args& args);
int cmd_add_blizzard(const Args& args);
int cmd_render(const Args& args);
int cmd_install_hook(const Args& args);
int cmd_config(const Args& args);
