# Self-implemented basic MPI APIs

To run the program, use

`make`

`./my_prun ./my_rtt`

### Basic explanation of implemention: <br>
The program use a shared file to communicate the host information before socket is established. Then, each node will create a socket thread to listen and receive data from others continously. MPI_Message struct is used as a message protocal for MPI_Send() and MPI_Recv(). After receiving data, the processor will cache the data temporarily in a global message buffer. Once MPI_Recv() is called, the message will be retrieved from there. Finially, most of clean up work will be done when MPI_Finilize() is called.

MPI_Barrier() is realized by sending an empty message to the master node. For now, we define the rank 0 as the master node, and this is specified by the micro. After the master node receiving messages from all nodes, it will send back a message to notify them success.

### Comparison with standard MPI <br>
Obviously, the self implemented MPI libraries have lower effiency than the standard ones. The result shows that the self implemented ones took 5 times time of the standard ones. Therefore, there is still a lot of room to improve and optimize.

### Some remaining question: <br>
1. Mutiple processors write a single file without lock, is there any problem?
After some basic research, it seems that on POSIX system, writing a small size string to the file should be atomic.

2. Can the ip be resolved by gethostbyname?
Yes.

3. PORT are now be forced to use the fixed number, so that each processor could only lanuch one node for now.
This can be improved by wirting PORT to the shared file.

4. How to deal with the case that MPI_Recv called before MPI_Send?
For now, the MPI_Recv block by performing a while iteration to check if the expected data is received. But there may be a more elegant way.
