typedef enum message_type
{
	none,
	text,
	attach,
	call
}
msg_type;

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
