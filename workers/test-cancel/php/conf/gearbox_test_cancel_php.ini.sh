#!/bin/sh

cat<<EOM
open_basedir = \${open_basedir}:$ROOT/var/gearbox/db/test-cancel-php/
EOM