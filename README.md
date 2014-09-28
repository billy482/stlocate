stlocate
========

Tool design to help to restore path of file after fsck.

Like the unix command *locate*, we need to build a database with *stupdate_db*.
Then, after an *fsck*, we can find some files into *lost+found*.
So *stmvfile* should help to move file to the original place.
