#!/usr/bin/env python
#
# $Id$
#
# This file is part of FreeRCT.
# FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
# FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
#
from rcdlib import ground_tiles

fname = '../sprites/groundtiletemplatetemplate8bpp64_masked.png'

lout = ground_tiles.std_layout
dummy_lout = [['x' for _v in row] for row in lout]

# The image has 12 rows, 6 for tiles with grid, and then 6 for tiles without
# grid. We are only interested in the non-grid ones, so the grid tiles get a
# dummy name.

images = ground_tiles.split_image(fname, -32, -33, 64, 64, dummy_lout + lout)

ground_tiles.write_groundRCD(images, 64, 16, ground_tiles.GRASS, 'groundtile_8bpp64.rcd')