# ProjectFolder

~~It seems there are two approaches we can choose from:
We can adopt the GPU usage or we can use the model files that we have and
implement on FPGA.

If it's FPGA we will still need the borph file and python files, but the cuda files
will not be needed.

If it's GPUs then the files need to be cleaned up, and then we just add the new
functions in.

The gist is this: python script turns files into borph then the borph file
is loaded and configured to the roach board.


borph.bof	: borph file :D

model.mdl	: Model

config.py 	: Configuration script

udp_grab.py	: Data Aquisition script
