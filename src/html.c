#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "msg.h"

static const char *html_header =
"<!DOCTYPE html>\n"
"<html>\n"
"	<head>\n"
"		<meta charset=\"UTF-8\">\n"
"		<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"		<link rel=\"stylesheet\" href=\"style.css\">\n"
"		<title>Signal IOS Dump</title>\n"
"	</head>\n"
"	<body>\n"
"		<div class=\"message-container\">";

static const char *html_footer =
"		</div>\n"
"	</body>\n"
"</html>\n";

static const char *html_dater =
"			<div class=\"dater\">\n"
"				%s\n"
"			</div>\n";

static const char *html_message_start =
"			<div class=\"message message-%s-%s\">\n";
static const char *html_message_reply_start =
"				<div class=\"%s-reply\">\n";

static const char *html_image =
"			<img src=\"Attachments/%s\" alt=\"image\">\n";

static const char *html_body = \
"				<div class=\"message-text\">\n"
"					<div class=\"message-content\">\n"
"						%s\n"
"					</div>\n"
"					<div class=\"time\">\n"
"						%s\n"
"					</div>\n"
"				</div>\n";

static const char *html_message_reply_end =
"				</div>\n";

static const char *html_message_end =
"			</div>\n";

void html_sanitise(char **in, size_t *max)
{
	const size_t str_l = strlen(*in);
	size_t offset = 0;
	char c = '\0';
	while ((c = *(*in + offset)) != '\0') {
		switch (c) {
		case '<':
			if (str_l + 3 > *max) {
				*in = realloc(*in, str_l + 4);
				*max = str_l + 3;
			}
			memmove((*in + offset) + 4, (*in + offset) + 1, (str_l + 5) - offset);
			memcpy((*in + offset), "&lt;", 4);
			break;
		case '>':
			if (str_l + 3 > *max) {
				*in = realloc(*in, str_l + 4);
				*max = str_l + 3;
			}
			memmove((*in + offset) + 4, (*in + offset) + 1, (str_l + 5) - offset);
			memcpy((*in + offset), "&gt;", 4);
			break;
		case '&':
			if (str_l + 4 > *max) {
				*in = realloc(*in, str_l + 5);
				*max = str_l + 4;
			}
			memmove((*in + offset) + 5, (*in + offset) + 1, (str_l + 6) - offset);
			memcpy((*in + offset), "&amp;", 5);
			break;
		case '"':
			if (str_l + 5 > *max) {
				*in = realloc(*in, str_l + 6);
				*max = str_l + 5;
			}
			memmove((*in + offset) + 6, (*in + offset) + 1, (str_l + 7) - offset);
			memcpy((*in + offset), "&quot;", 6);
			break;
		case '\'':
			if (str_l + 5 > *max) {
				*in = realloc(*in, str_l + 6);
				*max = str_l + 5;
			}
			memmove((*in + offset) + 6, (*in + offset) + 1, (str_l + 7) - offset);
			memcpy((*in + offset), "&apos;", 6);
			break;
		}
		offset++;
	}
}

void html_init(int fd)
{
	dprintf(fd, "%s\n", html_header);
}
void html_close(int fd)
{
	dprintf(fd, "%s\n", html_footer);
}

void html_print(int fd, msg *last_msg, const char *next_author, const time_t next_timestamp, const char *you)
{
	if (!last_msg->type)
		return;

	char time_buffer[30];
	const struct tm tm_info      = *gmtime(&last_msg->timestamp);
	const struct tm tm_info_last = *gmtime(&last_msg->last_timestamp);
	const struct tm tm_info_next = *gmtime(&next_timestamp);
	if (tm_info.tm_yday != tm_info_last.tm_yday) {
		strftime(time_buffer, 30, "%e, %b %Y", &tm_info);
		dprintf(fd, html_dater, time_buffer);
	}
	strftime(time_buffer, 30, "%H:%M", &tm_info);

	if (last_msg->type == call) {
		char calli[64];
		const char *call_msg = last_msg->call_type == 2  ? "Outgoing" :
		                       last_msg->call_type == 1  ? "Incoming" :
		                       last_msg->call_type == 8  ? "Unanswered" :
		                       last_msg->call_type == 12 ? "Missed call while on Do not disturb" :
		                       last_msg->call_type == 7  ? "Declied" :
		                       last_msg->call_type == 3  ? "Missed" : "";
		sprintf(calli, "&#128222 %s voice call · ", call_msg);
		strcat(calli, time_buffer);
		dprintf(fd, html_dater, calli);
		return;
	}

	char *dir;
	if (strcmp(last_msg->author, last_msg->last_author) == 0) {
		if (next_author && strcmp(last_msg->author, next_author) == 0) {
			if (tm_info.tm_yday != tm_info_last.tm_yday) {
				if (tm_info.tm_yday != tm_info_next.tm_yday)
					dir = "standalone";
				else
					dir = "top";
			} else {
				if (tm_info.tm_yday != tm_info_next.tm_yday)
					dir = "bottom";
				else
					dir = "middle";
			}
		} else {
			if (tm_info.tm_yday != tm_info_last.tm_yday)
				dir = "standalone";
			else
				dir = "bottom";
		}
	} else {
		if (next_author && strcmp(last_msg->author, next_author) == 0) {
			if (tm_info.tm_yday < tm_info_next.tm_yday)
				dir = "standalone";
			else
				dir = "top";
		} else {
			dir = "standalone";
		}
	}
	char lor[16];
	if (strcmp(last_msg->author, you) == 0)
		strcpy(lor, "right");
	else
		strcpy(lor, "left");
	dprintf(fd, html_message_start, lor, dir);

	if (last_msg->type_quote != none) {
		dprintf(fd, html_message_reply_start, lor);
		if (last_msg->type_quote == text)
			dprintf(fd, html_body, last_msg->quote, "");
		if (last_msg->type_quote == attach) {
			char *attach_ptr = last_msg->quote;
			while(1) {
				if (*attach_ptr == '\0')
					break;
				char *next_ptr = strchr(attach_ptr, '|');
				if (!next_ptr)
					break;
				*next_ptr = '\0';
				dprintf(fd, html_image, attach_ptr);
				attach_ptr = next_ptr + 1;
			}
		}
		dprintf(fd, "%s", html_message_reply_end);
	}

	char *attach_ptr = last_msg->attachments;
	while(1) {
		if (*attach_ptr == '\0')
			break;
		char *next_ptr = strchr(attach_ptr, '|');
		if (!next_ptr)
			break;
		*next_ptr = '\0';
		dprintf(fd, html_image, attach_ptr);
		attach_ptr = next_ptr + 1;
	}

	if (last_msg->type == text) {
		//html_sanitise(&last_msg->body, &last_msg->body_max)
		dprintf(fd, html_body, last_msg->body, time_buffer);
	}
	dprintf(fd, "%s", html_message_end);
}
