#include <stdio.h>
#include <string.h>
#include <time.h>

#include "msg.h"

const char * irc_dater = \
"------%d-%m-%y------";
const char * irc_message_start = \
"%s [ %s ] :\n\t";
const char * irc_attachment = \
"<attachment %s>\n\t";
const char * irc_call = \
"<%s voice call>\n";
const char * irc_quote = \
"<\"%s\">\n\t";

void irc_print(int fd, msg * last_msg)
{
	
}
