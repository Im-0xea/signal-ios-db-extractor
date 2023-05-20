#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <sqlite3.h>
#include <plist/plist.h>

#define VERSION "0.2"
/*
	seqdump
	
	TODO:
		- non-dm groups
		
*/

#include "html.h"

typedef enum message_type
{
	none,
	text,
	attach,
	call
}
msg_type;

typedef enum format
{
	irc,
	html
}
fmt;

typedef struct message
{
	time_t timestamp;
	time_t last_timestamp;
	time_t quote_timestamp;
	msg_type type;
	msg_type type_quote;
	int call_type;
	size_t body_max;
	size_t attachments_max;
	size_t quote_max;
	char * author;
	char * last_author;
	char * body;
	char * attachments;
	char * quote;
}
msg;

void html_print(int fd, msg * last_msg, const char * next_author, const time_t next_timestamp, const char * you)
{
	if (!last_msg->type)
	{
		return;
	}
	char time_buffer[30];
	const struct tm tm_info      = *gmtime(&last_msg->timestamp);
	const struct tm tm_info_last = *gmtime(&last_msg->last_timestamp);
	const struct tm tm_info_next = *gmtime(&next_timestamp);
	if (tm_info.tm_yday != tm_info_last.tm_yday)
	{
		strftime(time_buffer, 30, "%e, %b %Y", &tm_info);
		dprintf(fd, html_dater, time_buffer);
	}
	strftime(time_buffer, 30, "%H:%M", &tm_info);
	
	if (last_msg->type == call)
	{
		char calli[64];
		const char * call_msg = last_msg->call_type == 2  ? "Outgoing" : \
		                        last_msg->call_type == 1  ? "Incoming" : \
		                        last_msg->call_type == 8  ? "Unanswered" : \
		                        last_msg->call_type == 12 ? "Missed call while on Do not disturb" : \
		                        last_msg->call_type == 7  ? "Declied" : \
		                        last_msg->call_type == 3  ? "Missed" : "";
		sprintf(calli, "&#128222 %s voice call · ", call_msg);
		strcat(calli, time_buffer);
		dprintf(fd, html_dater, calli);
		return;
	}
	
	
	char * dir;
	if (strcmp(last_msg->author, last_msg->last_author) == 0)
	{
		if (strcmp(last_msg->author, next_author) == 0)
		{
			if (tm_info.tm_yday != tm_info_last.tm_yday)
			{
				if (tm_info.tm_yday != tm_info_next.tm_yday)
				{
					dir = "standalone";
				}
				else
				{
					dir = "top";
				}
			}
			else
			{
				if (tm_info.tm_yday != tm_info_next.tm_yday)
				{
					dir = "bottom";
				}
				else
				{
					dir = "middle";
				}
			}
		}
		else
		{
			if (tm_info.tm_yday != tm_info_last.tm_yday)
			{
				dir = "standalone";
			}
			else
			{
				dir = "bottom";
			}
		}
	}
	else
	{
		if (strcmp(last_msg->author, next_author) == 0)
		{
			if (tm_info.tm_yday < tm_info_next.tm_yday)
			{
				dir = "standalone";
			}
			else
			{
				dir = "top";
			}
		}
		else
		{
			dir = "standalone";
		}
	}
	char lor[16];
	if (strcmp(last_msg->author, you) == 0)
	{
		strcpy(lor, "right");
	}
	else
	{
		strcpy(lor, "left");
	}
	dprintf(fd, html_message_start, lor, dir);
	
	if (last_msg->type_quote != none)
	{
		dprintf(fd, "%s", html_message_reply_start);
		if (last_msg->type_quote == text)
		{
			dprintf(fd, html_body, last_msg->quote, "");
		}
		if (last_msg->type_quote == attach)
		{
			char * attach_ptr = last_msg->quote;
			while(1)
			{
				if (*attach_ptr == '\0')
				{
					break;
				}
				char * next_ptr = strchr(attach_ptr, '|');
				if (!next_ptr) break;
				*next_ptr = '\0';
				dprintf(fd, html_image, attach_ptr);
				attach_ptr = next_ptr + 1;
			}
		}
		dprintf(fd, "%s", html_message_reply_end);
	}
	
	char * attach_ptr = last_msg->attachments;
	while(1)
	{
		if (*attach_ptr == '\0')
		{
			break;
		}
		char * next_ptr = strchr(attach_ptr, '|');
		if (!next_ptr) break;
		*next_ptr = '\0';
		dprintf(fd, html_image, attach_ptr);
		attach_ptr = next_ptr + 1;
	}
	
	if (last_msg->type == text)
	{
		dprintf(fd, html_body, last_msg->body, time_buffer);
	}
	dprintf(fd, "%s", html_message_end);
}

int dump(const char * source, const char * output, const bool list, const char * groups, const char * nnumber, fmt format);

void sql_error(sqlite3 * db, const char * part)
{
	fprintf(stderr, "failed at %s", part);
	sqlite3_close(db);
	exit(1);
}

static void help(void)
{
	puts("Usage seqdump: -s [DATABASE] -o [OUTPUT] \n\n" \
		"A tool for dumping Signal history from the iOS database.\n" \
		"\n" \
		" -h --help      -> print this page\n" \
		" -V --version   -> show current version\n" \
		" -s --sql       -> define sql database\n" \
		" -o --output    -> define output\n" \
		" -l --list      -> list rooms\n" \
		" -f --format    -> set format (irc, html)\n" \
		" -n --number    -> extract number of Name\n" \
		" -g --groups    -> define groups to dump\n");
}

int main(const int argc, char ** argv)
{
	const char * short_options = "hVs:o:c:lf:n::g:";
	const struct option long_option[] =
	{
		{
			"help",         no_argument,       0, 'h'
		},
		
		{
			"version",      no_argument,       0, 'V'
		},
		
		{
			"sql",          required_argument, 0, 's'
		},
		
		{
			"output",       required_argument, 0, 'o'
		},
		
		{
			"list",         no_argument,       0, 'l'
		},
		
		{
			"format",       required_argument, 0, 'f'
		},
		
		{
			"number",       optional_argument, 0, 'n'
		},
		
		{
			"groups",       required_argument, 0, 'g'
		},
		
		{
			0, 0, 0, 0
		},
	};
	
	char * output = NULL;
	char * source = NULL;
	char * groups = NULL;
	char * number = NULL;
	bool   list   = false;
	fmt    format = html;
	
	while (1)
	{
		const int opt = getopt_long(argc, argv, short_options, long_option, NULL);
		if (opt == -1) break;
		switch (opt)
		{
			default:
			case '?':
				fprintf(stderr, "invalid option %c\n", opt);
				return 1;
			case 'h':
				help();
				return 0;
			case 'V':
				fputs("seqdump version "VERSION"\n", stdout);
				return 0;
			case 's':
				source = optarg;
				continue;
			case 'o':
				output = optarg;
				continue;
			case 'l':
				list = true;
				continue;
			case 'f':
				if(strcmp(optarg, "irc") == 0)
				{
					format = irc;
				}
				else if(strcmp(optarg, "html") == 0)
				{
					format = html;
				}
				continue;
			case 'n':
				if (optarg != NULL)
				{
					if (*optarg == '=')
					{
						number = optarg + 1;
					}
					else
					{
						number = optarg;
					}
				}
				else
				{
					number = (char *) 1;
				}
				continue;
			case 'g':
				groups = optarg;
				continue;
		}
	}
	
	if (source == NULL)
	{
		help();
		return 1;
	}
	
	dump(source, output, list, groups, number, format);
}

const char * lookup(const char ** table, const char * key, const size_t limit)
{
	size_t lookupn = 0;
	while (1)
	{
		if (strcmp(table[lookupn + 1], key) == 0)
		{
			return table[lookupn];
			break;
		}
		lookupn += 2;
		if (lookupn >= limit)
		{
			break;
		}
	}
	return NULL;
}

// parser for attachement uuids hidden in plists
// this is a strong bodge but its performant and simple
const size_t uuid_plister(char *** dest, const char * plist, const size_t plist_size)
{
	const char * plist_end = plist + plist_size;
	size_t count = 0;
	size_t last_malloc = 10;
	while (plist + 37 <= plist_end)
	{
		if (*plist == '$' && *(plist + 9) == '-')
		{
			if (!count)
			{
				*dest = malloc(10 * sizeof(char *));
			}
			else if (last_malloc <= count)
			{
				*dest = realloc(*dest, (count + 10) * sizeof(char *));
				last_malloc += 10;
			}
			(*dest)[count] = malloc(40 * sizeof(char));
			bzero((*dest)[count], 40);
			strncpy((*dest)[count], plist + 1, 36);
			count++;
			plist += 35;
		}
		plist++;
	}
	return count;
}

void attach_lookup(char ** dest, char * key, sqlite3 * db)
{
	sqlite3_stmt * stmtu = NULL;
	if ((sqlite3_prepare_v2(db, "SELECT localRelativeFilePath FROM model_TSAttachment WHERE uniqueId = ?;", -1, &stmtu, NULL)) != SQLITE_OK)
	{
		sql_error(db, "Attachment table");
	}
	if (sqlite3_bind_text(stmtu, 1, key, -1, SQLITE_TRANSIENT) != SQLITE_OK)
	{
		sql_error(db, "Attachment table");
	}
	if (sqlite3_step(stmtu) == SQLITE_ROW)
	{
		const char * path = sqlite3_column_text(stmtu, 0);
		if (path)
		{
			const size_t path_l = strlen(path);
			*dest = malloc((path_l + 1) * sizeof(char));
			strcpy(*dest, path + 1);
		}
	}
	sqlite3_finalize(stmtu);
}

// parser for quoted messages hidden in plists
const size_t quote_plister(char ** dest, size_t * dest_s, msg_type * type, const char * plist_buf, size_t plist_size, sqlite3 * db)
{
	size_t count = 0;
	plist_t plist = NULL;
	plist_format_t form = PLIST_FORMAT_BINARY;
	plist_from_memory(plist_buf, plist_size, &plist, &form);
	
	if (plist == NULL || plist_get_node_type(plist) != PLIST_DICT)
	{
		return 0;
	}
	
	plist_dict_iter iter = NULL;
	plist_dict_new_iter(plist, &iter);
	
	while (1)
	{
		plist_t value;
		plist_dict_next_item(plist, iter, NULL, &value);
		
		if (!value)
		{
			break;
		}
		
		if (plist_get_node_type(value) != PLIST_ARRAY)
		{
			continue;
		}
		
		plist_array_iter aiter = NULL;
		plist_array_new_iter(value, &aiter);
		size_t str_c = 0;
		bool attachi = false;
		bool dict_checked = false;
		while (1)
		{
			plist_t avalue;
			plist_array_next_item(value, aiter, &avalue);
			
			if (!avalue) break;
			
			if (!dict_checked && plist_get_node_type(avalue) == PLIST_DICT)
			{
				plist_dict_iter diter = NULL;
				plist_dict_new_iter(avalue, &diter);
				plist_t dkey;
				plist_dict_next_item(avalue, diter, NULL, &dkey);
				plist_t dkeyn;
				plist_dict_next_item(avalue, diter, NULL, &dkeyn);
				char * dkeys;
				plist_dict_get_item_key(dkeyn, &dkeys);
				if (strcmp(dkeys, "quotedAttachment") == 0)
				{
					attachi = true;
				}
				plist_mem_free(dkeys);
				free(diter);
				dict_checked = true;
			}
			else if (plist_get_node_type(avalue) == PLIST_STRING)
			{
				if (str_c == 1)
				{
					if (!attachi)
					{
						*type = text;
						char * str;
						plist_get_string_val(avalue, &str);
						if (str)
						{
							const size_t str_l = strlen(str);
							if (str_l > *dest_s)
							{
								*dest = realloc(*dest, str_l + 1);
								*dest_s = str_l;
							}
							strcpy(*dest, str);
							plist_mem_free(str);
							count++;
						}
					}
					else
					{
						*type = attach;
						char * str;
						plist_get_string_val(avalue, &str);
						char * path = NULL;
						attach_lookup(&path, str, db);
						plist_mem_free(str);
						if (path)
						{
							const size_t path_l = strlen(path);
							if (path_l > *dest_s)
							{
								*dest = realloc(*dest, path_l + 1);
								*dest_s = path_l;
							}
							sprintf(*dest, "%s", path);
							free(path);
							count++;
						}
					}
					break;
				}
				str_c++;
			}
		}
		free(aiter);
		if (count) 
		{
			break;
		}
	}
	
	free(iter);
	plist_free(plist);
	return count;
}

int dump(const char * source, const char * output, const bool list, const char * groups, const char * nnumber, fmt format)
{
	int out_fd = 1;
	if (output != NULL)
	{
		out_fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	}
	
	sqlite3 * db;
	if ((sqlite3_open(source, &db)) != SQLITE_OK)
	{
		fprintf(stderr, "Error opening db\n");
		return 1;
	}
	
	// get profile phone number
	// I decided to just take the first Recipients phone number
	sqlite3_stmt * an_stmt = NULL;
	char * acc_number = NULL;
	if (sqlite3_prepare_v2(db, "SELECT recipientPhoneNumber FROM model_SignalRecipient WHERE id = 1;", -1, &an_stmt, NULL) != SQLITE_OK)
	{
		sql_error(db, "Account Phone number");
	}
	if (sqlite3_step(an_stmt) == SQLITE_ROW)
	{
		const char * num = sqlite3_column_text(an_stmt, 0);
		const size_t num_l = strlen(num);
		acc_number = malloc((num_l + 1) * sizeof(char));
		strcpy(acc_number, num);
	}
	else
	{
		sql_error(db, "Account Phone number");
	}
	sqlite3_finalize(an_stmt);
	
	
	// create profile lookup table
	// allowing you to find the name of the sender of a message
	// and looking up the name associated with a dm group
	sqlite3_stmt * pft_stmt;
	if (sqlite3_prepare_v2(db, "SELECT profileName, recipientPhoneNumber FROM model_OWSUserProfile;", -1, &pft_stmt, NULL) != SQLITE_OK)
	{
		sql_error(db, "Profile Table");
	}
	char ** name_table = malloc(10 * sizeof(char *));
	size_t name_table_pos = 0;
	size_t name_table_last_malloc = 0;
	while (sqlite3_step(pft_stmt) == SQLITE_ROW)
	{
		if (sqlite3_column_type(pft_stmt, 0) != SQLITE_NULL && (sqlite3_column_type(pft_stmt, 1) != SQLITE_NULL || name_table_pos == 0))
		{
			const unsigned char * name = sqlite3_column_text(pft_stmt, 0);
			unsigned char * number = NULL;
			if (name_table_last_malloc + 10 <= name_table_pos)
			{
				name_table = realloc(name_table, (name_table_pos + 10) * sizeof(char *));
				name_table_last_malloc += 10;
			}
			name_table[name_table_pos] = malloc(64);
			name_table[name_table_pos + 1] = malloc(64);
			if (name_table_pos == 0)
			{
				number = acc_number;
			}
			else
			{
				number = (unsigned char *) sqlite3_column_text(pft_stmt, 1);
			}
			if ((nnumber > (char *) 1 && strcmp(name, nnumber) == 0) || nnumber == (char *) 1)
			{
				dprintf(out_fd, "%s: %s\n", name, number);
			}
			strcpy(name_table[name_table_pos], name);
			strcpy(name_table[name_table_pos + 1], number);
			name_table_pos += 2;
		}
	}
	sqlite3_finalize(pft_stmt);
	
	// create group lookup table
	// allowing you to associate messages with group names
	sqlite3_stmt * gt_stmt = NULL;
	if (sqlite3_prepare_v2(db, "SELECT uniqueId, contactPhoneNumber FROM model_TSThread;", -1, &gt_stmt, NULL) != SQLITE_OK)
	{
		sql_error(db, "Group Table\n");
	}
	char ** group_table = malloc(10 * sizeof(char *));
	size_t group_table_pos = 0;
	char * group;
	size_t group_table_last_malloc = 0;
	while (sqlite3_step(gt_stmt) == SQLITE_ROW)
	{
		if (sqlite3_column_type(gt_stmt, 0) != SQLITE_NULL && sqlite3_column_type(gt_stmt, 1) != SQLITE_NULL )
		{
			const unsigned char * group_id = sqlite3_column_text(gt_stmt, 0);
			const unsigned char * number = sqlite3_column_text(gt_stmt, 1);
			const char * name = lookup((const char **) name_table, number, name_table_pos);
			if (name)
			{
				if (list)
				{
					dprintf(out_fd, "%s\n", name);
					continue;
				}
				if (group_table_last_malloc + 10 <= group_table_pos)
				{
					group_table = realloc(group_table, (group_table_pos + 10) * sizeof(char *));
					group_table_last_malloc += 10;
				}
				group_table[group_table_pos] = malloc(64);
				group_table[group_table_pos + 1] = malloc(64);
				strcpy(group_table[group_table_pos], name);
				if (strcmp(name, groups) == 0)
				{
					group = malloc(strlen(group_id) + 1);
					strcpy(group, group_id);
					
				}
				strcpy(group_table[group_table_pos + 1], group_id);
				group_table_pos += 2;
			}
		}
	}
	sqlite3_finalize(gt_stmt);
	
	if (list || nnumber != NULL || nnumber == (char *) 1)
	{
		goto close;
	}
	
	dprintf(out_fd, "%s\n", html_header);
	
	// loops over all message in the target thread
	sqlite3_stmt * stmt;
	if ((sqlite3_prepare_v2(db, "SELECT body, uniqueThreadId, authorPhoneNumber, timestamp, callType, attachmentIds, quotedMessage FROM model_TSInteraction WHERE uniqueThreadId = ? ORDER BY id;", -1, &stmt, NULL)) != SQLITE_OK)
	{
		sql_error(db, "Interaction table");
	}
	if (sqlite3_bind_text(stmt, 1, group, -1, SQLITE_TRANSIENT) != SQLITE_OK)
	{
		sql_error(db, "Interaction table");
	}
	
	msg last_msg =
	{
		.last_timestamp = 0,
		.timestamp = 0,
		.type            = none,
		.type_quote      = none,
		.body_max        = 128,
		.attachments_max = 128,
		.quote_max       = 128,
		.author          = malloc(128),
		.last_author     = malloc(128),
		.body            = malloc(128),
		.attachments     = malloc(128),
		.quote           = malloc(128),
		
	};
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const msg_type type = (sqlite3_column_type(stmt, 0) != SQLITE_NULL) ? text : \
		                      (sqlite3_column_type(stmt, 4) != SQLITE_NULL) ? call : \
		                      (sqlite3_column_type(stmt, 5) != SQLITE_NULL) ? attach : \
		                      none;
		if (!type) continue;
		const char * author = (sqlite3_column_type(stmt, 2) == SQLITE_NULL) ? \
		acc_number : \
		(const char *) sqlite3_column_text(stmt, 2);
		const time_t timestamp = sqlite3_column_int64(stmt, 3) / 1000;
		
		html_print(out_fd, &last_msg, author, timestamp, acc_number);
		
		const char * name = lookup((const char **) name_table, author, name_table_pos);
		
		if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
		{
			char ** uuids = NULL;
			size_t uuidc = uuid_plister(&uuids, sqlite3_column_blob(stmt, 5), sqlite3_column_bytes(stmt, 5));
			if (!uuidc && type == 3) continue;
			last_msg.attachments[0] = '\0';
			size_t path_l = 1;
			while (uuidc)
			{
				--uuidc;
				char * path = NULL;
				attach_lookup(&path, uuids[uuidc], db);
				free(uuids[uuidc]);
				if (path)
				{
					path_l += strlen(path) + 1;
					if (path_l > last_msg.attachments_max)
					{
						last_msg.attachments = realloc(last_msg.attachments, path_l + 1);
						last_msg.attachments_max = path_l;
						
					}
					strcat(last_msg.attachments, path);
					strcat(last_msg.attachments, "|");
					
					free(path);
				}
				if (!uuidc)
				{
					free(uuids);
				}
			}
		}
		last_msg.type_quote = none;
		if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
		{
			quote_plister(&last_msg.quote, &last_msg.quote_max, &last_msg.type_quote, sqlite3_column_blob(stmt, 6), sqlite3_column_bytes(stmt, 6), db);
		}
		
		if (type == text)
		{
			const char * body   = sqlite3_column_text(stmt, 0);
			const size_t body_l = strlen(body);
			if (body_l > last_msg.body_max)
			{
				last_msg.body = realloc(last_msg.body, body_l + 1);
				last_msg.body_max = body_l;
			}
			strcpy(last_msg.body, sqlite3_column_text(stmt, 0));
		}
		else if (type == call)
		{
			last_msg.call_type = sqlite3_column_int64(stmt, 4);
		}
		
		char * new_author = last_msg.last_author;
		last_msg.last_author = last_msg.author;
		last_msg.author = new_author;
		strcpy(last_msg.author, author);
		last_msg.last_timestamp = last_msg.timestamp;
		last_msg.timestamp = timestamp;
		last_msg.type = type;
		
	}
	html_print(out_fd, &last_msg, NULL, last_msg.timestamp,  acc_number);
	free(last_msg.author);
	free(last_msg.last_author);
	free(last_msg.body);
	free(last_msg.attachments);
	free(last_msg.quote);
	free(group);
	
	sqlite3_finalize(stmt);
	dprintf(out_fd, "%s\n", html_footer);
	
	close:
	sqlite3_close(db);
	
	// free name_table
	while (1)
	{
		name_table_pos -= 2;
		free(name_table[name_table_pos]);
		free(name_table[name_table_pos + 1]);
		if (name_table_pos <= 0)
		{
			break;
		}
	}
	free(name_table);
	
	// free group table (if needed)
	if (!list)
	{
		while (1)
		{
			group_table_pos -= 2;
			free(group_table[group_table_pos]);
			free(group_table[group_table_pos + 1]);
			if (group_table_pos <= 0)
			{
				break;
			}
		}
		free(group_table);
	}
	
	// free file if defined
	if (output != NULL)
	{
		close(out_fd);
	}
	
	if (acc_number)
	{
		free(acc_number);
	}
	
	return 0;
}
