# News-Scanner
NNTP Group Scanner

This will compile on linux and windows<br>


This program will scan every newsgroup on your server for new 
articles. The first time you run it, it will list every newsgroup. After 
that run the program again with the argument 'clear'. The next time you 
run the program after that, it will list all of the newsgroups that have 
new posts.

Usage:<br>

nntp.exe <server> <username> <password> clear [group]<br>
This will clear all of the groups as being read, or optionally just one group<br>

nntp.exe <server> <username> <password> list <newsgroup> [number]<br>
This will list all of the posts in a newsgroup from the newest to the number you give it<br>

nntp.exe <server> <username> <password> read <newsgroup> <article_number> [output_file]<br>
This will print the content of an article, or optionally save it to an outputfile<br>

nntp.exe <server> <username> <password> search <newsgroup> <term> <count> [output_file]<br>
This prints out all the posts contains a search term in the article author or subject headers, optionally saving the list to a file<br>


nntp.exe <server> <username> <password> help<br>
This prints the commands<br>

Ifyou run the program without any arguments it prints all of the groups with new posts<br>


