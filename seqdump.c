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

#define VERSION "0.1"
/*
	seqdump
	
	TODO:
		- reply with only a attachement
		- non-dm groups
		
*/

int dump(const char * source, const char * output, const bool list, const char * groups);

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
		" -g --groups    -> define groups to dump\n");
}

int main(const int argc, char ** argv)
{
	const char * short_options = "hVs:o:c:lg:";
	const struct option long_option[] =
	{
		{
			"help",    no_argument,       0, 'h'
		},
		
		{
			"version", no_argument,       0, 'V'
		},
		
		{
			"sql",     required_argument, 0, 's'
		},
		
		{
			"output",  required_argument, 0, 'o'
		},
		
		{
			"list",    no_argument,       0, 'l'
		},
		
		{
			"groups",  required_argument, 0, 'g'
		},
		
		{
			0, 0, 0, 0
		},
	};
	
	char * output = NULL;
	char * source = NULL;
	char * groups = NULL;
	bool   list   = false;
	
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
	
	dump(source, output, list, groups);
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
const size_t uuid_plister(char *** dest, char * plist, char * plist_end)
{
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
	sqlite3_stmt * stmtu;
	if ((sqlite3_prepare_v2(db, "SELECT localRelativeFilePath from model_TSAttachment where uniqueId = ?;", -1, &stmtu, NULL)) != SQLITE_OK)
	{
		fprintf(stderr, "Error reading from Attachment table\n");
		sqlite3_close(db);
		exit(1);
	}
	if (sqlite3_bind_text(stmtu, 1, key, -1, SQLITE_TRANSIENT) != SQLITE_OK)
	{
		fprintf(stderr, "Error reading from Interaction table\n");
		sqlite3_close(db);
		exit(1);
	}
	if (sqlite3_step(stmtu) == SQLITE_ROW)
	{
		char * path = sqlite3_column_text(stmtu, 0);
		if (path)
		{
			const size_t path_l = strlen(path);
			*dest = malloc((path_l + 1) * sizeof(char));
			strcpy(*dest, path);
		}
	}
	sqlite3_finalize(stmtu);
}

// parser for quoted messages hidden in plists
const size_t quote_plister(char ** dest, char * plist_buf, size_t plist_size, sqlite3 * db)
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
		bool attach = false;
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
					attach = true;
				}
				//plist_mem_free(dkeys)
				free(diter);
				dict_checked = true;
			}
			else if (plist_get_node_type(avalue) == PLIST_STRING)
			{
				if (str_c == 1)
				{
					if (!attach)
					{
						char * str;
						plist_get_string_val(avalue, &str);
						const size_t str_l = strlen(str);
						if (str)
						{
							*dest = malloc((str_l + 1) * sizeof(char));
							strcpy(*dest, str);
							plist_mem_free(str);
							count++;
						}
					}
					else
					{
						char * str;
						plist_get_string_val(avalue, &str);
						char * path = NULL;
						attach_lookup(&path, str, db);
						plist_mem_free(str);
						if (path)
						{
							*dest = malloc((strlen(path) + 12) * sizeof(char));
							sprintf(*dest, "attachment %s", path);
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

int dump(const char * source, const char * output, const bool list, const char * groups)
{
	// either set output file descriptor to stdout or open defined output as fd
	int out_fd;
	if (output == NULL)
	{
		out_fd = 1;
	}
	else
	{
		out_fd = open(output, O_WRONLY | O_CREAT, 0644);
	}
	
	// open sqlite3 database
	sqlite3 * db;
	const char * tail;
	if ((sqlite3_open(source, &db)) != SQLITE_OK)
	{
		fprintf(stderr, "Error opening db\n");
		return 1;
	}
	
	// get profile phone number
	// I decided to just take the first Recipients phone number
	// as this consistently returns your own
	sqlite3_stmt * an_stmt;
	char * acc_number;
	if (sqlite3_prepare_v2(db, "SELECT recipientPhoneNumber FROM model_SignalRecipient WHERE id = 1;", -1, &an_stmt, NULL) != SQLITE_OK)
	{
		fprintf(stderr, "Error reading Account Phone number\n");
		sqlite3_close(db);
		return 1;
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
		fprintf(stderr, "Error reading Account Phone number\n");
		sqlite3_close(db);
		return 1;
	}
	sqlite3_finalize(an_stmt);
	
	
	// create profile lookup table
	// this table contains [0] Profile Name [1] Phone number
	// allowing you to find the name of the sender of a message
	// and looking up the name associated with a dm group
	sqlite3_stmt * pft_stmt;
	if (sqlite3_prepare_v2(db, "SELECT profileName, recipientPhoneNumber from model_OWSUserProfile;", -1, &pft_stmt, NULL) != SQLITE_OK)
	{
		fprintf(stderr, "Error Parsing Profile Table\n");
		sqlite3_close(db);
		return 1;
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
			strcpy(name_table[name_table_pos], name);
			strcpy(name_table[name_table_pos + 1], number);
			name_table_pos += 2;
		}
	}
	sqlite3_finalize(pft_stmt);
	
	// create group lookup table
	// this table contains [0] Group Name [1] Group ID
	// allowing you to associate messages with group names
	sqlite3_stmt * gt_stmt;
	if (sqlite3_prepare_v2(db, "SELECT uniqueId, contactPhoneNumber from model_TSThread;", -1, &gt_stmt, NULL) != SQLITE_OK)
	{
		fprintf(stderr, "Error Parsing Profile Table\n");
		sqlite3_close(db);
		return 1;
	}
	char ** group_table = malloc(10 * sizeof(char *));
	size_t group_table_pos = 0;
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
				strcpy(group_table[group_table_pos + 1], group_id);
				group_table_pos += 2;
			}
		}
	}
	sqlite3_finalize(gt_stmt);
	
	if (list)
	{
		goto close;
	}
	
	// message loop
	// loops over all entries of the interaction table and prints them accordingly
	sqlite3_stmt * stmt;
	if ((sqlite3_prepare_v2(db, "SELECT body, uniqueThreadId, authorPhoneNumber, timestamp, callType, attachmentIds, quotedMessage from model_TSInteraction;", -1, &stmt, &tail)) != SQLITE_OK)
	{
		fprintf(stderr, "Error reading from Interaction table\n");
		sqlite3_close(db);
		return 1;
	}
	
	const time_t unixstart = 0;
	struct tm tm_info = *gmtime(&unixstart);
	while (sqlite3_step(stmt) == SQLITE_ROW)
	{
		const unsigned char * record = sqlite3_column_text(stmt, 1);
		// get group name from group_table
		const unsigned char * group = lookup((const char **) group_table, record, group_table_pos);
		if (!groups || group && strcmp(group, groups) == 0)
		{
			unsigned char * author;
			// small hack - the authorPhoneNumber is Null if its from you
			// so this is why we needed to safe it before
			if (sqlite3_column_type(stmt, 2) == SQLITE_NULL)
			{
				author = acc_number;
			}
			else
			{
				author = (char *) sqlite3_column_text(stmt, 2);
			}
			const unsigned char * name = lookup((const char **) name_table, author, name_table_pos);
			
			char buffer[30];
			if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
			{
				// print message
				const time_t timestamp = sqlite3_column_int64(stmt, 3) / 1000;
				const double timestampd = (float) sqlite3_column_int64(stmt, 3) / (float) 1000.0;
				const struct tm tm_info_tmp = *gmtime((time_t *) &timestamp);
				if (tm_info.tm_yday != tm_info_tmp.tm_yday)
				{
					strftime(buffer, 30, "%d-%m-%Y", &tm_info_tmp);
					dprintf(out_fd, "------%s------\n\n", buffer);
				}
				tm_info = tm_info_tmp;
				strftime(buffer, 30, "%H:%M", &tm_info);
				const unsigned char * body = sqlite3_column_text(stmt, 0);
				dprintf(out_fd, "%s [ %s ] :\n\t", buffer, name);
				if (sqlite3_column_type(stmt, 6) != SQLITE_NULL)
				{
					void * blob_data = sqlite3_column_blob(stmt, 6);
					const size_t blob_size = sqlite3_column_bytes(stmt, 6);
					char * quote = NULL;
					if (quote_plister(&quote, blob_data, blob_size, db) && quote)
					{
						dprintf(out_fd, "<\"%s\">\n\t", quote);
						free(quote);
					}
				}
				if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
				{
					void * blob_data = sqlite3_column_blob(stmt, 5);
					const size_t blob_size = sqlite3_column_bytes(stmt, 5);
					char ** uuids = NULL;
					size_t uuidc = uuid_plister(&uuids, blob_data, blob_data + blob_size);
					while (uuidc)
					{
						--uuidc;
						char * path =NULL;
						attach_lookup(&path, uuids[uuidc], db);
						free(uuids[uuidc]);
						if (path)
						{
							dprintf(out_fd, "<attachment %s>\n\t", path);
						}
						free(path);
						if (!uuidc)
						{
							free(uuids);
						}
					}
					
				}
				dprintf(out_fd, "%s\n\n", body);
			}
			else if (sqlite3_column_type(stmt, 4) != SQLITE_NULL)
			{
				// print call
				const unsigned int call = sqlite3_column_int64(stmt, 4);
				const time_t timestamp = sqlite3_column_int64(stmt, 3) / 1000;
				const struct tm tm_info_tmp = *gmtime((time_t *) &timestamp);
				if (tm_info.tm_yday != tm_info_tmp.tm_yday)
				{
					strftime(buffer, 30, "%d-%m-%Y", &tm_info_tmp);
					dprintf(out_fd, "------%s------\n\n", buffer);
				}
				tm_info = tm_info_tmp;
				strftime(buffer, 30, "%H:%M", &tm_info);
				dprintf(out_fd, "%s [ %s ] :\n\t", buffer, name);
				dprintf(out_fd, "<%s voice call>\n\n", call == 2  ? "Outgoing" : \
				                                       call == 1  ? "Incomming" : \
				                                       call == 8  ? "Unanswered" : \
				                                       call == 12 ? "Missed call while on Do not disturb" : \
				                                       call == 7  ? "Declied" : \
				                                       call == 3  ? "Missed" : "");
			}
			else if (sqlite3_column_type(stmt, 5) != SQLITE_NULL)
			{
				void * blob_data = sqlite3_column_blob(stmt, 5);
				const size_t blob_size = sqlite3_column_bytes(stmt, 5);
				char ** uuids = NULL;
				size_t uuidc = uuid_plister(&uuids, blob_data, blob_data + blob_size);
				if (uuidc)
				{
					const time_t timestamp = sqlite3_column_int64(stmt, 3) / 1000;
					const time_t timestampd = sqlite3_column_int64(stmt, 3);
					const struct tm tm_info_tmp = *gmtime((time_t *) &timestamp);
					if (tm_info.tm_yday != tm_info_tmp.tm_yday)
					{
						strftime(buffer, 30, "%d-%m-%Y", &tm_info_tmp);
						dprintf(out_fd, "------%s------\n\n", buffer);
					}
					tm_info = tm_info_tmp;
					strftime(buffer, 30, "%H:%M", &tm_info);
					dprintf(out_fd, "%s [ %s ] :\n", buffer, name);
				}
				while (uuidc)
				{
					--uuidc;
					char * path = NULL;
					attach_lookup(&path, uuids[uuidc], db);
					if (path)
					{
						dprintf(out_fd, "\t<attachment %s>\n", path);
					}
					free (uuids[uuidc]);
					free (path);
					if (!uuidc)
					{
						free (uuids);
						dprintf(out_fd, "\n");
					}
				}
			}
		}
	}
	
	close:
	
	sqlite3_finalize(stmt);
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
	
	free(acc_number);
	
	return 0;
}
