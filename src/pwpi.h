#pragma once

#define MAX_PWPI_TEXT_LENGTH 512

typedef void (*PWPIMessageHandler)(char *line, char *return_message);

void pwpi_init(int port, PWPIMessageHandler message_handler);
void pwpi_enable();
void pwpi_disable();
void pwpi_handle_events();
void pwpi_shutdown();

