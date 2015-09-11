#!/bin/bash

file=$1

if [ -z "$file" ]; then
		echo "Usage: $0 log"
		exit 1
fi

cat $file | gawk -F'\n' '{ if($0 == "") { printf("\n"); } else { printf("%s ", $0); } }' \
		| grep mutex | sed -e 's/mutex #//' | tr -s ' ' | gawk -F':' '{ for (i=2; i<NF; i++) { printf("%s ", $i); } printf(" £ %s\n", $1); }' \
		| sort | gawk -F'£' '{ if($1 != prev) { prev=$1; printf("\nAt: %s: ", $1); };  printf("%s ", $2); } END { printf("\n"); }' \
		| tee tmp

while read line; do
		first=-1
		F=""
		for x in $(echo $line | cut -d':' -f3-); do
				if [ $first = -1 ]; then
						first=$x
				else
						F=$F"+"
				fi
				F=$F$(cat $file | grep -w "#$x" | grep -v mutex | tr -s ' ' | cut -d' ' -f3 | cut -d'%' -f1)
		done

		if [ $first != -1 ]; then
				echo -n "Mutex #$first: "
				echo $F | bc -l
		fi
done < tmp 

\rm -f tmp
