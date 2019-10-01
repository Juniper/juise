#!/bin/sh

cat - > test-02.out
cat <<EOF
<?xml version="1.0"?>
<top>
  <grumble/>
</top>
EOF

exit 0

