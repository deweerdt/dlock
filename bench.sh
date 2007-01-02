#   bench.sh - benchmark for dlock, uses perf.c
#   Copyright (C) 2006,2007  Frederik Deweerdt <frederik.deweerdt@gmail.com>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License along
#   with this program; if not, write to the Free Software Foundation, Inc.,
#   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.


LOOPS=10
rm -f res_dlock; for i in $(seq 1 $LOOPS); do (time ./perf_dlock) 2>> res_dlock; done
DLOCK=$(cat res_dlock  | grep real | sed 's/.*m\(.*\)s.*/\1/' | awk '{tot+=$1;}END{print tot;}')
rm -f res_dlock

rm -f res_nodlock; for i in $(seq 1 $LOOPS); do (time ./perf_nodlock) 2>> res_nodlock; done
NO_DLOCK=$(cat res_nodlock  | grep real | sed 's/.*m\(.*\)s.*/\1/' | awk '{tot+=$1;}END{print tot;}')
rm -f res_nodlock

echo "With dlock: $DLOCK"
echo "Without dlock: $NO_DLOCK"
echo "dlock takes : $(echo "scale=5; 100 * (($DLOCK / $NO_DLOCK) - 1);" | bc)% more time"
