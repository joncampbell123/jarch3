#!/bin/bash
echo touch NEWS README AUTHORS ChangeLog INSTALL COPYING
touch NEWS README AUTHORS ChangeLog INSTALL COPYING

echo autoreconf --verbose --install --symlink --warnings=all
exec autoreconf --verbose --install --symlink --warnings=all

mkdir -p m4

