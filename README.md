# FindDupes

Recursively compares files in a directory tree identifying duplicates, optionally deleting the redundant files.

    Usage: FindDupes 'directory' [-d] [-r] [-p]  
      -r : report duplicates (more verbose than default behavior)  
      -p : paranoid content check (instead of file size + 128bit MD5 hash)  
      -d : delete identified redundant files  
