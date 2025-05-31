# News-Scanner
NNTP Group Scanner

This will compile on linux and windows<br>


This program will scan every newsgroup on your server for new 
articles. The first time you run it, it will list every newsgroup. After 
that run the program again with the argument 'clear'. The next time you 
run the program after that, it will list all of the newsgroups that have 
new posts.<br>


Usage:<br>
nntp.exe server username password clear [group]<br>
This marks all groups as read or optionally one group<br>

nntp.exe server username password list newsgroup [number]<br>
This lists the newest posts on a group<br>

nntp.exe server username password read newsgroup article_number [output_file]<br>
This prints out an article or optionally saves it to an output file<br>

nntp.exe server username password search newsgroup term count [output_file]<br>
This searches the newest posts for a term in the subject and author headers.<br>

nntp.exe server username password help<br>
This prints help<br>

If you run the program with no other arguments it lists all groups with new articles<br>



