#!/bin/bash
echo touch NEWS README AUTHORS ChangeLog INSTALL COPYING
touch NEWS README AUTHORS ChangeLog INSTALL COPYING

mkdir -p m4

echo autoreconf --verbose --install --symlink --warnings=all
exec autoreconf --verbose --install --symlink --warnings=all

