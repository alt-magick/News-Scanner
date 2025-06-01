# News-Scanner
NNTP Group Scanner

This will compile on linux and windows<br>


This program will scan every newsgroup on your server for new 
articles. The first time you run it, it will list every newsgroup. After 
that run the program again with the argument 'clear'. The next time you 
run the program after that, it will list all of the newsgroups that have 
new posts.<br>


Usage:<br>
nntp_client server username password<br>
Lists all of the newsgroups with new articles<br>

nntp_client server username password clear<br>
Marks all newsgroups as being read<br>

nntp_client server username password clear newsgroup<br>
Marks one newsgroup as being read<br>

nntp_client server username password reset<br>
Sets all newsgroups as unread<br>

nntp_client server username password list newsgroup [number_of_articles]<br>
Lists new posts on newsgroup<br>

nntp_client server username password read newsgroup article_number [output_file]<br>
Reads a post on a newsgroup<br>

nntp_client server username password search newsgroup term count [output_file]<br>
Searches a newsgroup for a search term in the subject and author headers<br>

nntp_client server username password searchall searchTerm number_of_articles_to_check [output file]<br>
Searches all newsgroups with new posts for a search term in the subject and author headers<br>
