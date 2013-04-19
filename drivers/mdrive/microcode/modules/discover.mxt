' Function: DI
' Discovery routine. Used to search for all units on a comm line. Use with
'
' *EX DI
'
' The methodology is that each unit will receive the instruction to be
' discovered, and each unit will wait an amount of time, based on its
' address, and then will print out its address. This will allow for quick
' scanning to find all units on a channel flashed with microcode supporting
' this operation
LB DI
  ' Allow time for `each address to transmit 5 chars. Trasmit time is
  ' 1.04ms for 9600 baud and faster for all faster bauds. This script will
  ' write out "d"\n\n, perhaps one more chars if the device is in checksum
  ' mode -- the checksum char
  H DN - 48 * 6
  PR DN
  E

' vim: set ft=vb:
