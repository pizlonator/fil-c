#as: -march=rv32i_xcvmac
#objdump: -d

.*:[ 	]+file format .*


Disassembly of section .text:

0+000 <.text>:
[ 	]+[0-9a-f]+:[ 	]+907332ab[ 	]+cv.mac[ 	]+t0,t1,t2
[ 	]+[0-9a-f]+:[ 	]+9053beab[ 	]+cv.mac[ 	]+t4,t2,t0
[ 	]+[0-9a-f]+:[ 	]+906f3e2b[ 	]+cv.mac[ 	]+t3,t5,t1
[ 	]+[0-9a-f]+:[ 	]+407362db[ 	]+cv.machhsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+5653eedb[ 	]+cv.machhsn[ 	]+t4,t2,t0,11
[ 	]+[0-9a-f]+:[ 	]+7e6f6e5b[ 	]+cv.machhsn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+c07362db[ 	]+cv.machhsrn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+f053eedb[ 	]+cv.machhsrn[ 	]+t4,t2,t0,24
[ 	]+[0-9a-f]+:[ 	]+fe6f6e5b[ 	]+cv.machhsrn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+407372db[ 	]+cv.machhun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+6453fedb[ 	]+cv.machhun[ 	]+t4,t2,t0,18
[ 	]+[0-9a-f]+:[ 	]+7e6f7e5b[ 	]+cv.machhun[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+c07372db[ 	]+cv.machhurn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+ca53fedb[ 	]+cv.machhurn[ 	]+t4,t2,t0,5
[ 	]+[0-9a-f]+:[ 	]+fe6f7e5b[ 	]+cv.machhurn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+007362db[ 	]+cv.macsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+3053eedb[ 	]+cv.macsn[ 	]+t4,t2,t0,24
[ 	]+[0-9a-f]+:[ 	]+3e6f6e5b[ 	]+cv.macsn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+807362db[ 	]+cv.macsrn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+9253eedb[ 	]+cv.macsrn[ 	]+t4,t2,t0,9
[ 	]+[0-9a-f]+:[ 	]+be6f6e5b[ 	]+cv.macsrn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+007372db[ 	]+cv.macun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+3653fedb[ 	]+cv.macun[ 	]+t4,t2,t0,27
[ 	]+[0-9a-f]+:[ 	]+3e6f7e5b[ 	]+cv.macun[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+807372db[ 	]+cv.macurn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+b253fedb[ 	]+cv.macurn[ 	]+t4,t2,t0,25
[ 	]+[0-9a-f]+:[ 	]+be6f7e5b[ 	]+cv.macurn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+927332ab[ 	]+cv.msu[ 	]+t0,t1,t2
[ 	]+[0-9a-f]+:[ 	]+9253beab[ 	]+cv.msu[ 	]+t4,t2,t0
[ 	]+[0-9a-f]+:[ 	]+926f3e2b[ 	]+cv.msu[ 	]+t3,t5,t1
[ 	]+[0-9a-f]+:[ 	]+407342db[ 	]+cv.mulhhsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+4053cedb[ 	]+cv.mulhhsn[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+406f4e5b[ 	]+cv.mulhhsn[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+407342db[ 	]+cv.mulhhsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+6053cedb[ 	]+cv.mulhhsn[ 	]+t4,t2,t0,16
[ 	]+[0-9a-f]+:[ 	]+7e6f4e5b[ 	]+cv.mulhhsn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+c07342db[ 	]+cv.mulhhsrn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+e253cedb[ 	]+cv.mulhhsrn[ 	]+t4,t2,t0,17
[ 	]+[0-9a-f]+:[ 	]+fe6f4e5b[ 	]+cv.mulhhsrn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+407352db[ 	]+cv.mulhhun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+4053dedb[ 	]+cv.mulhhun[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+406f5e5b[ 	]+cv.mulhhun[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+407352db[ 	]+cv.mulhhun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+6053dedb[ 	]+cv.mulhhun[ 	]+t4,t2,t0,16
[ 	]+[0-9a-f]+:[ 	]+7e6f5e5b[ 	]+cv.mulhhun[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+c07352db[ 	]+cv.mulhhurn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+d253dedb[ 	]+cv.mulhhurn[ 	]+t4,t2,t0,9
[ 	]+[0-9a-f]+:[ 	]+fe6f5e5b[ 	]+cv.mulhhurn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+007342db[ 	]+cv.mulsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0053cedb[ 	]+cv.mulsn[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+006f4e5b[ 	]+cv.mulsn[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+007342db[ 	]+cv.mulsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0853cedb[ 	]+cv.mulsn[ 	]+t4,t2,t0,4
[ 	]+[0-9a-f]+:[ 	]+3e6f4e5b[ 	]+cv.mulsn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+807342db[ 	]+cv.mulsrn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+9453cedb[ 	]+cv.mulsrn[ 	]+t4,t2,t0,10
[ 	]+[0-9a-f]+:[ 	]+be6f4e5b[ 	]+cv.mulsrn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+007352db[ 	]+cv.mulun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0053dedb[ 	]+cv.mulun[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+006f5e5b[ 	]+cv.mulun[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+007352db[ 	]+cv.mulun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0e53dedb[ 	]+cv.mulun[ 	]+t4,t2,t0,7
[ 	]+[0-9a-f]+:[ 	]+3e6f5e5b[ 	]+cv.mulun[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+807352db[ 	]+cv.mulurn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+9653dedb[ 	]+cv.mulurn[ 	]+t4,t2,t0,11
[ 	]+[0-9a-f]+:[ 	]+be6f5e5b[ 	]+cv.mulurn[ 	]+t3,t5,t1,31
[ 	]+[0-9a-f]+:[ 	]+407342db[ 	]+cv.mulhhsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+4053cedb[ 	]+cv.mulhhsn[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+406f4e5b[ 	]+cv.mulhhsn[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+407352db[ 	]+cv.mulhhun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+4053dedb[ 	]+cv.mulhhun[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+406f5e5b[ 	]+cv.mulhhun[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+007342db[ 	]+cv.mulsn[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0053cedb[ 	]+cv.mulsn[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+006f4e5b[ 	]+cv.mulsn[ 	]+t3,t5,t1,0
[ 	]+[0-9a-f]+:[ 	]+007352db[ 	]+cv.mulun[ 	]+t0,t1,t2,0
[ 	]+[0-9a-f]+:[ 	]+0053dedb[ 	]+cv.mulun[ 	]+t4,t2,t0,0
[ 	]+[0-9a-f]+:[ 	]+006f5e5b[ 	]+cv.mulun[ 	]+t3,t5,t1,0
