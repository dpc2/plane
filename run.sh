

FILE=$1
sudo bash -c "LIBGL_PACK_SIZE=1024 LIBGL_DEBUG_CVG=1 LD_LIBRARY_PATH=/home/cvg/5/rootfs/lib  ./${FILE}" 


rm -rf gl_inst.data
rm -rf vbo.data
rm -rf mesa.out
rm -rf cvg.out



