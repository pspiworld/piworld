#pragma once

void pwlua_startup_init(void);
void pwlua_startup_deinit(void);
void pwlua_on_player_init(LocalPlayer *local);
int pwlua_on_menu_event(LocalPlayer *local, Menu *menu, int event);
void pwlua_startup_call_action(LocalPlayer *local, char *action_name, Event *event);

