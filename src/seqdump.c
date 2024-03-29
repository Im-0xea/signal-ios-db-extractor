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

#include "msg.h"
#include "html.h"
#include "irc.h"

#define VERSION "0.3"
/*
	seqdump
*/

enum fmt {
	irc,
	html
};

int dump(const char *source, const char *output, const bool list, const char *groups, const char *nnumber, enum fmt format);

void sql_error(sqlite3 *db, const char *part)
{
	fprintf(stderr, "failed at %s", part);
	sqlite3_close(db);
	exit(1);
}

void free_table(char **table, size_t pos)
{
	while (pos > 0) {
		pos--;
		free(table[pos]);
	}
	free(table);
}

void realloc_check(char **buf, size_t new, size_t *old)
{
	if (new > *old) {
		*buf = realloc(*buf, new + 1);
		*old = new;
	}
}

static void help(void)
{
	puts("Usage seqdump: -s [DATABASE] -o [OUTPUT] \n\n"
		"A tool for dumping Signal history from the iOS database.\n"
		"\n"
		" -h --help      -> print this page\n"
		" -V --version   -> show current version\n"
		" -s --sql       -> define sql database\n"
		" -o --output    -> define output\n"
		" -l --list      -> list rooms\n"
		" -f --format    -> set format (irc, html)\n"
		" -n --number    -> extract number of Name\n"
		" -g --groups    -> define groups to dump\n");
}

int main(const int argc, char **argv)
{
	const char *short_options = "hVs:o:c:lf:n::g:";
	const struct option long_option[] = {
		{ "help",         no_argument,       0, 'h' },
		{ "version",      no_argument,       0, 'V' },
		{ "sql",          required_argument, 0, 's' },
		{ "output",       required_argument, 0, 'o' },
		{ "list",         no_argument,       0, 'l' },
		{ "format",       required_argument, 0, 'f' },
		{ "number",       optional_argument, 0, 'n' },
		{ "groups",       required_argument, 0, 'g' },
		{ 0, 0, 0, 0 },
	};

	char     *output = NULL;
	char     *source = NULL;
	char     *groups = NULL;
	char     *number = NULL;
	bool     list    = false;
	enum fmt format  = html;

	while (1) {
		const int opt = getopt_long(argc, argv, short_options, long_option, NULL);
		if (opt == -1)
			break;
		switch (opt) {
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
				format = irc;
			else if(strcmp(optarg, "html") == 0)
				format = html;
			continue;
		case 'n':
			if (optarg == NULL)
				number = (char *) 1;
			else if (*optarg == '=')
				number = optarg + 1;
			else
				number = optarg;
			continue;
		case 'g':
			groups = optarg;
			continue;
		}
	}

	if (source == NULL) {
		help();
		return 1;
	}

	dump(source, output, list, groups, number, format);
}

char *lookup(char **table, const char *key, const size_t limit)
{
	size_t lookupn = 0;
	while (1) {
		if (strcmp(table[lookupn + 1], key) == 0) {
			return table[lookupn];
			break;
		}
		lookupn += 2;
		if (lookupn >= limit)
			break;
	}
	return NULL;
}

// parser for attachement uuids hidden in plists
// this is a strong bodge but its performant and simple
size_t uuid_plister(char ***dest, const char *plist, const size_t plist_size)
{
	const char *plist_end = plist + plist_size;
	size_t count = 0;
	size_t last_malloc = 10;
	while (plist + 37 <= plist_end) {
		if (*plist != '$' || *(plist + 9) != '-') {
			plist++;
			continue;
		}

		if (!count) {
			*dest = malloc(10 * sizeof(char *));
		} else if (last_malloc <= count) {
			*dest = realloc(*dest, (count + 10) * sizeof(char *));
			last_malloc += 10;
		}
		(*dest)[count] = malloc(40 * sizeof(char));
		bzero((*dest)[count], 40);
		strncpy((*dest)[count], plist + 1, 36);
		++count;
		plist += 36;
	}
	return count;
}

void attach_lookup(char **dest, char *key, sqlite3 *db)
{
	sqlite3_stmt *stmtu = NULL;
	if ((sqlite3_prepare_v2(db, "SELECT localRelativeFilePath FROM model_TSAttachment WHERE uniqueId = ?;", -1, &stmtu, NULL)) != SQLITE_OK)
		sql_error(db, "Attachment table");
	if (sqlite3_bind_text(stmtu, 1, key, -1, SQLITE_TRANSIENT) != SQLITE_OK)
		sql_error(db, "Attachment table");
	if (sqlite3_step(stmtu) == SQLITE_ROW) {
		const char *path = (char *)sqlite3_column_text(stmtu, 0);
		if (path) {
			const size_t path_l = strlen(path);
			*dest = malloc((path_l + 1) * sizeof(char));
			strcpy(*dest, path + 1);
		}
	}
	sqlite3_finalize(stmtu);
}

// parser for quoted messages hidden in plists
const size_t quote_plister(char **dest, size_t *dest_s, msg_type *type, const char *plist_buf, size_t plist_size, sqlite3 *db)
{
	size_t count = 0;
	plist_t plist = NULL;
	plist_format_t form = PLIST_FORMAT_BINARY;
	plist_from_memory(plist_buf, plist_size, &plist, &form);

	if (plist == NULL || plist_get_node_type(plist) != PLIST_DICT)
		return 0;

	plist_dict_iter iter = NULL;
	plist_dict_new_iter(plist, &iter);

	while (1) {
		plist_t value;
		plist_dict_next_item(plist, iter, NULL, &value);

		if (!value)
			break;

		if (plist_get_node_type(value) != PLIST_ARRAY)
			continue;

		plist_array_iter aiter = NULL;
		plist_array_new_iter(value, &aiter);
		size_t str_c = 0;
		bool attachi = false;
		bool dict_checked = false;
		while (1) {
			plist_t avalue;
			plist_array_next_item(value, aiter, &avalue);

			if (!avalue) break;

			if (!dict_checked && plist_get_node_type(avalue) == PLIST_DICT) {
				plist_dict_iter diter = NULL;
				plist_dict_new_iter(avalue, &diter);
				plist_t dkey;
				plist_dict_next_item(avalue, diter, NULL, &dkey);
				plist_t dkeyn;
				plist_dict_next_item(avalue, diter, NULL, &dkeyn);
				char *dkeys;
				plist_dict_get_item_key(dkeyn, &dkeys);
				if (strcmp(dkeys, "quotedAttachment") == 0)
					attachi = true;
				plist_mem_free(dkeys);
				free(diter);
				dict_checked = true;
				continue;
			}
			if (plist_get_node_type(avalue) != PLIST_STRING)
				continue;

			if (str_c != 1) {
				++str_c;
				continue;
			}
			if (!attachi) {
				*type = text;
				char *str;
				plist_get_string_val(avalue, &str);
				if (!str) continue;
				realloc_check(dest, strlen(str), dest_s);
				strcpy(*dest, str);
				plist_mem_free(str);
				++count;
			} else {
				*type = attach;
				char *str;
				plist_get_string_val(avalue, &str);
				char *path = NULL;
				attach_lookup(&path, str, db);
				plist_mem_free(str);
				if (!path) continue;
				realloc_check(dest, strlen(path), dest_s);
				sprintf(*dest, "%s", path);
				free(path);
				count++;
			}
			break;
		}
		free(aiter);
		if (count) 
			break;
	}

	free(iter);
	plist_free(plist);
	return count;
}

void group_plister(char **dest, const char *plist_buf, size_t plist_size)
{
	plist_t plist = NULL;
	plist_format_t form = PLIST_FORMAT_BINARY;
	plist_from_memory(plist_buf, plist_size, &plist, &form);

	if (plist == NULL || plist_get_node_type(plist) != PLIST_DICT)
		return;

	plist_dict_iter iter = NULL;
	plist_dict_new_iter(plist, &iter);

	while (1) {
		plist_t value;
		plist_dict_next_item(plist, iter, NULL, &value);

		if (!value)
			break;

		if (plist_get_node_type(value) != PLIST_ARRAY)
			continue;

		plist_array_iter aiter = NULL;
		plist_array_new_iter(value, &aiter);
		size_t str_c = 0;
		while (1) {
			plist_t avalue;
			plist_array_next_item(value, aiter, &avalue);

			if (!avalue)
				break;

			if (plist_get_node_type(avalue) != PLIST_STRING)
				continue;
			if (str_c != 1) {
				++str_c;
				continue;
			}
			
			char *str;
			plist_get_string_val(avalue, &str);
			if (!str)
				break;

			if (strncmp("groups/", str, 7) == 0) {
				str_c -= 1;
				plist_mem_free(str);
				continue;
			}

			const size_t str_l = strlen(str);
			*dest = malloc(str_l + 1);
			strcpy(*dest, str);
			plist_mem_free(str);
			break;
		}
		free(aiter);
	}

	free(iter);
	plist_free(plist);
}

void \
parse_interaction(sqlite3 *db, sqlite3_stmt *stmt, msg *last_msg, \
	char **name_table, const int name_table_pos, char **group_table, const char *acc_number, const int out_fd)
{
	const msg_type type = (sqlite3_column_type(stmt, 0) != SQLITE_NULL) ? text :
			      (sqlite3_column_type(stmt, 4) != SQLITE_NULL) ? call :
			      (sqlite3_column_type(stmt, 5) != SQLITE_NULL) ? attach :
			      none;
	if (!type)
		return;
	const char * author = (sqlite3_column_type(stmt, 2) == SQLITE_NULL) ?
	acc_number :
	(const char *) sqlite3_column_text(stmt, 2);
	const time_t timestamp = sqlite3_column_int64(stmt, 3) / 1000;

	html_print(out_fd, last_msg, author, timestamp, acc_number);

	//const char * name = lookup(name_table, author, name_table_pos);

	if (sqlite3_column_type(stmt, 5) != SQLITE_NULL) {
		char **uuids = NULL;
		size_t uuidc = uuid_plister(&uuids, sqlite3_column_blob(stmt, 5), sqlite3_column_bytes(stmt, 5));
		if (!uuidc && type == 3) return;
		last_msg->attachments[0] = '\0';
		size_t path_l = 1;
		while (uuidc) {
			--uuidc;
			char *path = NULL;
			attach_lookup(&path, uuids[uuidc], db);
			free(uuids[uuidc]);
			if (!path)
				return;

			path_l += strlen(path) + 1;
			realloc_check(&last_msg->attachments, path_l, &last_msg->attachments_max);
			strcat(last_msg->attachments, path);
			strcat(last_msg->attachments, "|");
			
			free(path);
		}
		if (uuids)
			free(uuids);
	}
	last_msg->type_quote = none;
	if (sqlite3_column_type(stmt, 6) != SQLITE_NULL) {
		quote_plister(&last_msg->quote, &last_msg->quote_max, &last_msg->type_quote, sqlite3_column_blob(stmt, 6), sqlite3_column_bytes(stmt, 6), db);
	}

	if (type == text) {
		const char *body = (char *)sqlite3_column_text(stmt, 0);
		realloc_check(&last_msg->body, strlen(body), &last_msg->body_max);
		strcpy(last_msg->body, (char *)sqlite3_column_text(stmt, 0));
	} else if (type == call) {
		last_msg->call_type = sqlite3_column_int64(stmt, 4);
	}

	char *new_author = last_msg->last_author;
	last_msg->last_author = last_msg->author;
	last_msg->author = new_author;
	strcpy(last_msg->author, author);
	last_msg->last_timestamp = last_msg->timestamp;
	last_msg->timestamp = timestamp;
	last_msg->type = type;
}

int
dump(const char *source, const char *output, const bool list,
	const char *groups, const char *nnumber, enum fmt format)
{
	int out_fd = 1;
	if (output != NULL)
		out_fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);

	sqlite3 *db;
	if ((sqlite3_open(source, &db)) != SQLITE_OK) {
		fprintf(stderr, "Error opening db\n");
		return 1;
	}

	// get profile phone number
	// I decided to just take the first Recipients phone number
	sqlite3_stmt *an_stmt = NULL;
	char *acc_number = NULL;
	if (sqlite3_prepare_v2(db, "SELECT recipientPhoneNumber FROM model_SignalRecipient WHERE id = 1;", -1, &an_stmt, NULL) != SQLITE_OK)
		sql_error(db, "Account Phone number");

	if (sqlite3_step(an_stmt) != SQLITE_ROW)
		sql_error(db, "Account Phone number");

	const char   *num  = (char *)sqlite3_column_text(an_stmt, 0);
	const size_t num_l = strlen(num);
	acc_number = malloc((num_l + 1) * sizeof(char));
	strcpy(acc_number, num);
	sqlite3_finalize(an_stmt);

	// create profile lookup table
	// allowing you to find the name of the sender of a message
	// and looking up the name associated with a dm group
	sqlite3_stmt *pft_stmt;
	if (sqlite3_prepare_v2(db, "SELECT profileName, recipientPhoneNumber FROM model_OWSUserProfile;", -1, &pft_stmt, NULL) != SQLITE_OK)
		sql_error(db, "Profile Table");

	char **name_table = malloc(10 * sizeof(char *));
	size_t name_table_pos = 0;
	size_t name_table_last_malloc = 0;
	while (sqlite3_step(pft_stmt) == SQLITE_ROW) {
		if (sqlite3_column_type(pft_stmt, 0) == SQLITE_NULL || 
		   (sqlite3_column_type(pft_stmt, 1) == SQLITE_NULL && name_table_pos != 0))
			continue;

		const char *name = (char *)sqlite3_column_text(pft_stmt, 0);
		char *number = NULL;
		if (name_table_last_malloc + 10 <= name_table_pos) {
			name_table = realloc(name_table, (name_table_pos + 10) * sizeof(char *));
			name_table_last_malloc += 10;
		}
		name_table[name_table_pos] = malloc(64);
		name_table[name_table_pos + 1] = malloc(64);
		if (name_table_pos == 0)
			number = acc_number;
		else
			number = (char *)sqlite3_column_text(pft_stmt, 1);
		if ((nnumber > (char *) 1 && strcmp(name, nnumber) == 0) || nnumber == (char *) 1)
			dprintf(out_fd, "%s: %s\n", name, number);
		strcpy(name_table[name_table_pos], name);
		strcpy(name_table[name_table_pos + 1], number);
		name_table_pos += 2;
	}
	sqlite3_finalize(pft_stmt);

	// create group lookup table
	// allowing you to associate messages with group names
	sqlite3_stmt *gt_stmt = NULL;
	if (sqlite3_prepare_v2(db, "SELECT uniqueId, contactPhoneNumber, groupModel FROM model_TSThread;", -1, &gt_stmt, NULL) != SQLITE_OK)
		sql_error(db, "Group Table\n");
	char   **group_table = malloc(10 * sizeof(char *));
	size_t group_table_pos = 0;
	char   *group;
	size_t group_table_last_malloc = 0;
	while (sqlite3_step(gt_stmt) == SQLITE_ROW) {
		if (sqlite3_column_type(gt_stmt, 0) == SQLITE_NULL)
			continue;

		const char *group_id = (char *)sqlite3_column_text(gt_stmt, 0);
		char *name = NULL;
		if (sqlite3_column_type(gt_stmt, 2) != SQLITE_NULL) {
			const char *group_model = sqlite3_column_blob(gt_stmt, 2);
			group_plister(&name, group_model, sqlite3_column_bytes(gt_stmt, 2));
		} else {
			if (sqlite3_column_type(gt_stmt, 1) == SQLITE_NULL)
				continue;
			const char *number = (char *)sqlite3_column_text(gt_stmt, 1);
			name = lookup(name_table, number, name_table_pos);
		}
		if (!name)
			continue;
		if (list) {
			dprintf(out_fd, "%s\n", name);
			continue;
		}

		if (group_table_last_malloc + 10 <= group_table_pos) {
			group_table = realloc(group_table, (group_table_pos + 10) * sizeof(char *));
			group_table_last_malloc += 10;
		}
		if (sqlite3_column_type(gt_stmt, 2) != SQLITE_NULL) {
			group_table[group_table_pos] = name;
		} else {
			group_table[group_table_pos] = malloc(64);
			strcpy(group_table[group_table_pos], name);
		}
		group_table[group_table_pos + 1] = malloc(64);
		if (groups && strcmp(name, groups) == 0) {
			group = malloc(strlen(group_id) + 1);
			strcpy(group, group_id);
		}
		strcpy(group_table[group_table_pos + 1], group_id);
		group_table_pos += 2;
	}
	sqlite3_finalize(gt_stmt);

	if (list || nnumber != NULL || nnumber == (char *) 1)
		goto close;
	
	html_init(out_fd);

	// loops over all message in the target thread
	sqlite3_stmt * stmt;
	if ((sqlite3_prepare_v2(db, \
		"SELECT body, uniqueThreadId, authorPhoneNumber, timestamp, " \
		"callType, attachmentIds, quotedMessage FROM model_TSInteraction " \
		"WHERE uniqueThreadId = ? ORDER BY id;", -1, &stmt, NULL)) != SQLITE_OK)
		sql_error(db, "Interaction table");
	if (sqlite3_bind_text(stmt, 1, group, -1, SQLITE_TRANSIENT) != SQLITE_OK)
		sql_error(db, "Interaction table");

	msg last_msg = {
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
	while (sqlite3_step(stmt) == SQLITE_ROW) {
		parse_interaction(db, stmt, &last_msg, name_table, name_table_pos,
			group_table, acc_number, out_fd);
	}

	html_print(out_fd, &last_msg, NULL, last_msg.timestamp,  acc_number);
	free(last_msg.author);
	free(last_msg.last_author);
	free(last_msg.body);
	free(last_msg.attachments);
	free(last_msg.quote);
	free(group);

	sqlite3_finalize(stmt);
	html_close(out_fd);
	close:
	sqlite3_close(db);

	// free name_table
	free_table(name_table, name_table_pos);

	// free group table (if needed)
	if (!list)
		free_table(group_table, group_table_pos);

	// free file if defined
	if (output != NULL)
		close(out_fd);

	if (acc_number)
		free(acc_number);

	return 0;
}
