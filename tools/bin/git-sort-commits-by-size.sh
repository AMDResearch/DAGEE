# Copyright (c) 2018-Present Advanced Micro Devices, Inc. See LICENSE.TXT for terms.

#!/bin/bash 
git rev-list --objects --all \
  | git cat-file --batch-check='%(objecttype) %(objectname) %(objectsize) %(rest)' \
  | sed -n 's/^blob //p' \
  | sort --numeric-sort --key=2 \
  | cut -c 1-12,41- \
  | numfmt --field=2 --to=iec-i --suffix=B --padding=7 --round=nearest

cat <<EOM
Remove large files from the history using the following command (WARNING: DANGEROUS OPERATION)

git filter-branch --tree-filter 'rm -f large files' HEAD

IMPORTANT PRECAUTIONS:
-command must be executed from root dir of repo. File paths should be relative to root 

-large files may be in multiple branches, so need to execute this for each branch

-The command will re-write history of the branch and replace existing commits with
new similar looking commits. This has more implications:

   - Specify all large file names once so history is rewritten once only

   - If the local branch, say B, tracks a remote branch, say origin/B, a simple
   push won't work. You'll have to git push --force. 

   - Other people who have checked out origin/B will need to remove their local B
   and checkout again. For every branch. A simple pull will try to merge and do the
   wrong thing, i.e. retain the deleted files. 
EOM
