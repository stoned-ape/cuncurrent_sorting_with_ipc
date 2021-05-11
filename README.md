This repo contains cuncurrent implementations of merge sort using a variety of interprocess communication methods.
The merge sort algorithm splits an array in half, sorts the two halves separately, then merges them together.
Since the two halves are sorted independently, the can be sorted in parallel on two cpu cores.
The first implementation does this the obvious way, by sorting the two halves in different threads.
The less efficient yet more interesting solution is to sort the two halves in two different processes.
fork() the process, sort the upper half in the child and the lower half in the parent.  
The parent then waits for child to finish and merges the two halves.
IPC is necessary for the parent to get the sorted upper half from the child.
I have included implementations using a variety of IPC methods.
