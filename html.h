const char * html_header = \
"<!DOCTYPE html>\n" \
"<html>\n" \
"	<head>\n" \
"		<meta charset=\"UTF-8\">\n" \
"		<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n" \
"		<link rel=\"stylesheet\" href=\"style.css\">\n" \
"		<title>Signal IOS Dump</title>\n" \
"	</head>\n" \
"	<body>\n" \
"		<div class=\"message-container\">";
const char * html_footer = \
"		</div>\n" \
"	</body>\n" \
"</html>\n";
const char * html_dater = \
"			<div class=\"dater\">\n" \
"				%s\n" \
"			</div>\n";
const char * html_message_start = \
"			<div class=\"message message-text message-%s-%s\">\n";
const char * html_body = \
"				<div class=\"message-content\">\n" \
"					%s\n" \
"				</div>\n" \
"				<div class=\"time\">\n" \
"					%s\n" \
"				</div>\n";
const char * html_message_end = \
"			</div>\n";
const char * html_reply = \
"				<div class\"quote\">\n" \
"					%s\n" \
"				</div>";
const char * html_image = \
"			<img src=\"Attachments/%s\" alt=\"image\">\n";
