#!/bin/sh

set -e

Count=20
Qpktcountval=51
Holdcountval=20

MAXINT=32767

BUFSIZE=3
I_IDLE=1
I_WORK=2
I_HANDLERA=3
I_HANDLERB=4
I_DEVA=5
I_DEVB=6
PKTBIT=1
WAITBIT=2
HOLDBIT=4
NOTHOLDBIT=0XFFFB

S_RUN=0
S_RUNPKT=1
S_WAIT=2
S_WAITPKT=3
S_HOLD=4
S_HOLDPKT=5
S_HOLDWAIT=6
S_HOLDWAITPKT=7

alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZ"
tasktablen=10
taskcount=0
tasklist=0
tcb=0
taskid=0
v1=0
v2=0
qpktcount=0
holdcount=0
pktcount=0

for id in `seq 1 $tasktablen`
do
    eval "tasktab_${id}=0"
done

createtask()
{
    id=$1
    pri=$2
    wkq=$3
    state=$4
    fn=$5
    v1=$6
    v2=$7
    
    taskcount=$(($taskcount + 1))
    thistask=$taskcount
    eval "task_${thistask}_link=$tasklist"
    eval "task_${thistask}_id=$id"
    eval "task_${thistask}_pri=$pri"
    eval "task_${thistask}_wkq=$wkq"
    eval "task_${thistask}_state=$state"
    eval "task_${thistask}_fn=$fn"
    eval "task_${thistask}_v1=$v1"
    eval "task_${thistask}_v2=$v2"
    eval "tasktab_${id}=$thistask"
    tasklist=$thistask
}

pkt()
{
    link=$1
    id=$2
    kind=$3

    pktcount=$(($pktcount + 1))
    thispkt=$pktcount

    for i in `seq 0 $BUFSIZE`
    do
        eval "pkt_${thispkt}_a2_${i}=0"
    done
    eval "pkt_${thispkt}_link=$link"
    eval "pkt_${thispkt}_id=$id"
    eval "pkt_${thispkt}_kind=$kind"
    eval "pkt_${thispkt}_a1=$a1"
}

schedule()
{
    while test $tcb != 0
    do
        pkt=0
        if test $(eval echo \$task_${tcb}_state) -eq $S_WAITPKT
        then
            eval "pkt=\$task_${tcb}_wkq"
            eval "task_${tcb}_wkq=\$pkt_${pkt}_link"
            if test $(eval echo \$task_${tcb}_wkq) = 0
            then
                eval "task_${tcb}_state=$S_RUN"
            else
                eval "task_${tcb}_state=$S_RUNPKT"
            fi
        fi
        eval "state=\$task_${tcb}_state"
        if test $state -eq $S_RUN || test $state -eq $S_RUNPKT
        then
           eval "taskid=\$task_${tcb}_id"
           eval "v1=\$task_${tcb}_v1"
           eval "v2=\$task_${tcb}_v2"
           eval "fn=\$task_${tcb}_fn"
           eval "$fn $pkt"
           eval "task_${tcb}_v1=$v1"
           eval "task_${tcb}_v2=$v2"
           tcb=$newtcb
        elif test $state -eq $S_WAIT || test $state -eq $S_HOLD ||
                test $state -eq $S_HOLDPKT || test $state -eq $S_HOLDWAIT ||
                test $state -eq $S_HOLDWAITPKT
        then
            eval "tcb=\$task_${tcb}_link"
        else
            echo "Bad state $state"
            exit 1
        fi
    done
}

waitself()
{
    eval "task_${tcb}_state=\$((\$task_${tcb}_state | $WAITBIT))"
    newtcb=$tcb
}

holdself()
{
    holdcount=$(($holdcount + 1))
    eval "task_${tcb}_state=\$((\$task_${tcb}_state | $HOLDBIT))"
    newtcb=$tcb
}

findtcb()
{
    id=$1
    test $id -ge 1
    test $id -le $tasktablen
    eval "newtcb=\$tasktab_${id}"
    test $newtcb != 0
}

release()
{
    id=$1
    findtcb $id
    eval "task_${newtcb}_state=\$((\$task_${newtcb}_state & $NOTHOLDBIT))"
    if test $(eval echo \$task_${newtcb}_pri) -le $(eval echo \$task_${tcb}_pri)
    then
        newtcb=$tcb
    fi
}

qpkt()
{
    pkt=$1
    findtcb $(eval echo \$pkt_${pkt}_id)
    qpktcount=$(($qpktcount + 1))
    eval "pkt_${pkt}_link=0"
    eval "pkt_${pkt}_id=$taskid"
    if test $(eval echo \$task_${newtcb}_wkq) = 0
    then
        eval "task_${newtcb}_wkq=$pkt"
        eval "task_${newtcb}_state=\$((\$task_${newtcb}_state | $PKTBIT))"
        if test $(eval echo \$task_${newtcb}_pri) -le $(eval echo \$task_${tcb}_pri)
        then
            newtcb=$tcb
        fi
    else
        append $pkt task_${newtcb}_wkq
        newtcb=$tcb
    fi
}

idlefn()
{
    pkt=$1
    v2=$(($v2 - 1))
    if test $v2 -eq 0
    then
        holdself
        return
    fi
    if test $(($v1 & 1)) -eq 0
    then
        v1=$((($v1 >> 1) & $MAXINT))
        release $I_DEVA
    else
        v1=$(((($v1 >> 1) & $MAXINT) ^ 0XD008))
        release $I_DEVB
    fi
}

workfn()
{
    pkt=$1
    if test $pkt = 0
    then
        waitself
    else
        v1=$(($I_HANDLERA + $I_HANDLERB - $v1))
        eval "pkt_${pkt}_id=$v1"
        eval "pkt_${pkt}_a1=0"
        for i in `seq 1 $BUFSIZE`
        do
            v2=$(($v2 + 1))
            if test $v2 -gt 26
            then
                v2=1
            fi
            letter=$(expr substr $alphabet $v2 1)
            value=$(printf "%d" "'$letter")
            eval "pkt_${pkt}_a2_${i}=$value"
        done
        qpkt $pkt
    fi
}

handlerfn()
{
    pkt=$1
    if test $pkt != 0
    then
        if test $(eval echo \$pkt_${pkt}_kind) = K_WORK
        then
            append $pkt v1
        else
            append $pkt v2
        fi
    fi
    if test $v1 -ne 0
    then
        workpkt=$v1
        eval "count=\$pkt_${workpkt}_a1"
        if test $count -gt $BUFSIZE
        then
           eval "v1=\$pkt_${v1}_link"
           qpkt $workpkt
           return
        fi

        if test $v2 -ne 0
        then
            devpkt=$v2
            eval "v2=\$pkt_${v2}_link"
            eval "pkt_${devpkt}_a1=\$pkt_${workpkt}_a2_${count}"
            eval "pkt_${workpkt}_a1=$(($count + 1))"
            qpkt $devpkt
            return
        fi
    fi
    waitself
}

devfn()
{
    pkt=$1
    if test $pkt -eq 0
    then
        if test $v1 -eq 0
        then
            waitself
            return
        fi
        pkt=$v1
        v1=0
        qpkt $pkt
    else
        v1=$pkt
        holdself
    fi
}

append()
{
    pkt=$1
    ptr=$2
    eval "pkt_${pkt}_link=0"
    while test $(eval echo \${$ptr}) -ne 0
    do
        eval "ptr=pkt_\${$ptr}_link"
    done
    eval "${ptr}=$pkt"
}

echo "Bench mark starting"

createtask $I_IDLE 0 0 $S_RUN idlefn 1 $Count

pkt 0 0 K_WORK
pkt $thispkt 0 K_WORK

createtask $I_WORK 1000 $thispkt $S_WAITPKT workfn $I_HANDLERA 0

pkt 0 $I_DEVA K_DEV
pkt $thispkt $I_DEVA K_DEV
pkt $thispkt $I_DEVA K_DEV

createtask $I_HANDLERA 2000 $thispkt $S_WAITPKT handlerfn 0 0

pkt 0 $I_DEVB K_DEV
pkt $thispkt $I_DEVB K_DEV
pkt $thispkt $I_DEVB K_DEV

createtask $I_HANDLERB 3000 $thispkt $S_WAITPKT handlerfn 0 0

createtask $I_DEVA 4000 0 $S_WAIT devfn 0 0
createtask $I_DEVB 4000 0 $S_WAIT devfn 0 0

tcb=$tasklist

schedule

echo finished
echo "qpkt count = $qpktcount  holdcount = $holdcount"

if test $qpktcount -eq $Qpktcountval && test $holdcount -eq $Holdcountval
then
    echo "These result are correct"
else
    echo "These results are incorrect"
    exit 1
fi

echo "end of run"

