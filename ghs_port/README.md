* Compilation  
** Build dependent Libraries  
    project files and cmake configured files are located at corresponding directories at ghs_port folder
    To build each library, need to execute following commands to clone source code of all submodules first  
    git submodule init  
    git submodule update  

    Then set path of INTEGRITY OS, BSP name tool and tgt dir in the config.cmk.tmpl  

    To have tgt files, please use GreenHill Multi and create a sample project file then the tgt dir    
    which contains some linking tool appear in the sample project  

    
    Then go to each directory in ghs_port and compile  
    E.g.  
    At the esdiff dir execute following commands to  compile  
       mkdir build && cd build  
       cmake .. && make  
** Build libua.a, tmpl-updateagent and deltapatcher.bin  
After compiling all dependent libs, got to parent directory for compiling updateagent-c:  
    export PORT=ghs_port  
    mkdir build && cd build && cmake ..  
    make  
    To automatically compile all, execute build_all_ghs.sh

* Test environment  
  HW Platform: Renesas Rcar Salvator-XS (RcarM3)  
  OS: INTEGRITY 11.78  
  Compilter: GreenHill compilter version 201814  

* Notes  
  FAT32 file system is used, sdcard is mounted to file system at /sdcard.  
  Ramdisk is mounted to "/"  
  Node certificate is placed at the /sdcard  
  backup and cache folder is created at /sdcard/backup and /sdcard/cache  
  normal update and delta functionality has been tested in INTEGRITY OS  
