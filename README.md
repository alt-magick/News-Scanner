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
nntp.exe server username password list newsgroup [number]<br>
nntp.exe server username password read newsgroup article_number [output_file]<br>
nntp.exe server username password search newsgroup term count [output_file]<br>
nntp.exe server username password help<br>


