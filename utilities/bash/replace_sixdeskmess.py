import re
import sys


filename=sys.argv[1]



old_notation=False

with open(filename) as f:
    for line in f:
        skipline=False
        if 'sixdeskmess=' in line:
            
            whitespace   = re.match(r"\s*", line).group()
            
            print whitespace+'sixdeskmess -1 '+line.split('=')[1],
            
            skipline=False
            
            continue
        
        elif 'sixdeskmess' in line and len(line.split())!=1:
            skipline=False
            print line,
        
        elif 'sixdeskmess' in line and 'sixdeskmess=' not in line and 'sixdeskmesslevel' not in line:
            continue
            

        else:
            if skipline:
                continue
            else:
                print line,

