#!/bin/bash
########################INPUTS#############################
declare -i NUM_COL=4
declare -i NUM_ROW=3
declare -i Inject_buffer=256
declare -i BW=16
declare -i PKT=16
fl="net-mesh"
NET="net0"

# KEEP IN MIND THAT IF YOU WANT VARIABLE SIZES FOR BUFFERS OF
# EACH DIFFERENT LINK, YOU SHOULD ADD THEM INDIVIDUALLY SINCE 
# THIS SCRIPT DOESN'T SUPPORT THIS.

##########################################################
Nodes=`expr $NUM_COL \\* $NUM_ROW`
Num_of_n=`expr $Nodes - 1`
Num_of_sw=`expr $Nodes - 1`
COL=`expr $NUM_COL - 1`
ROW=`expr $NUM_ROW - 1`

echo ";--------------------------------------------Network" > $fl
echo "[Network.$NET]" >> $fl
echo "DefaultInputBufferSize = $Inject_buffer" >>$fl
echo "DefaultOutputBufferSize = $Inject_buffer" >> $fl
echo "DefaultBandwidth = $BW" >>$fl
echo "DefaultPacketSize = $PKT" >>$fl
echo "" >> $fl

echo ";--------------------------------------------------Nodes" >> $fl

echo "Nodes :  $Nodes"

for i in $(seq 0 $Num_of_n)
do
echo "[Network.$NET.Node.n$i]" >> $fl
echo "Type = EndNode" >>$fl
echo "" >>$fl
done

echo ";---------------------------------------------------Switches" >> $fl

echo "Switches :  $Nodes"

for i in $(seq 0 $Num_of_sw)
do
echo "[Network.$NET.Node.s$i]" >> $fl
echo "Type = Switch" >> $fl
echo "" >>$fl
done

echo ";-----------------------------------------------------Links" >> $fl
for i in $(seq 0 $Num_of_n)
do
	echo "[Network.$NET.Link.n$i-s$i]" >> $fl
	echo "Type = Bidirectional" >>$fl
	echo "Source = n$i" >> $fl
	echo "Dest = s$i" >> $fl
	echo "" >> $fl
done

for i in $(seq 0 $ROW)
do
	for iter in $(seq 0 $COL)
	do
		X=`expr $iter + $i \\* $NUM_COL`
		Y=`expr $X + 1`
		if [ $iter -ne $COL ] 
		then
			echo "[Network.$NET.Link.s$X-s$Y]" >> $fl
			echo "Type = Bidirectional" >>$fl
			echo "Source = s$X" >> $fl
			echo "Dest = s$Y" >> $fl
			echo "" >> $fl
		fi

		W=`expr $X + $NUM_COL`
		if [ $i -ne $ROW ] 
		then
			echo "[Network.$NET.Link.s$X-s$W]" >> $fl
			echo "Type = Bidirectional" >>$fl
			echo "Source = s$X" >> $fl
			echo "Dest = s$W" >> $fl
			echo "" >> $fl
		fi
	done

done

